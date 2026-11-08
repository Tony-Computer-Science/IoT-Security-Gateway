#!/usr/bin/env python3
"""Build web/src into web/dist: minify, gzip, size report, budget check.

Pure Python standard library. The minifiers are deliberately conservative:
they strip comments and redundant whitespace but never rename or restructure
code, so they cannot change behavior. gzip does the heavy lifting.

Output: one <name>.gz per source file in web/dist/ for asset-size checks.
The separate server/PC service serves web/src/ directly; no ESP32 serves
these files and no asset flashing is required (see web/README.md).

Exit status is non-zero if:
  - the total gzipped size exceeds the 60 KB budget, or
  - the minified output uses a forbidden API or references an external URL
    (localStorage, sessionStorage, document.cookie, WebSocket, http(s)://).
"""

import argparse
import gzip
import re
import shutil
import sys
from pathlib import Path

WEB_DIR = Path(__file__).resolve().parent.parent
SRC_DIR = WEB_DIR / "src"
DIST_DIR = WEB_DIR / "dist"

BUDGET_BYTES = 60 * 1024

FORBIDDEN = [
    (re.compile(r"\blocalStorage\b"), "localStorage (R2: token must stay in memory)"),
    (re.compile(r"\bsessionStorage\b"), "sessionStorage (R2: token must stay in memory)"),
    (re.compile(r"\bdocument\.cookie\b"), "document.cookie (R2: token must stay in memory)"),
    (re.compile(r"\bWebSocket\b"), "WebSocket (use polling)"),
    (re.compile(r"https?://(?!www\.w3\.org/)", re.I), "external URL (page must work offline)"),
]


# ---------------------------------------------------------------------------
# JavaScript
# ---------------------------------------------------------------------------

_WORD = re.compile(r"[A-Za-z0-9_$\x80-￿]")
_REGEX_AFTER_CHARS = set("(,=:[!&|?{};+-*%<>~^")
_REGEX_AFTER_WORDS = {
    "return", "typeof", "case", "do", "else", "in", "of", "new", "delete",
    "void", "throw", "instanceof", "yield", "await",
}


def _is_word(ch):
    return bool(ch) and bool(_WORD.match(ch))


def minify_js(src):
    """Strip comments and collapse whitespace. Newlines are kept (one per
    statement line) so automatic semicolon insertion is never affected."""
    out = []
    i, n = 0, len(src)
    # Stack of brace depths for `${ ... }` inside template literals.
    template_stack = []
    brace_depth = 0

    def last():
        return out[-1][-1] if out and out[-1] else ""

    def last_sig():
        for chunk in reversed(out):
            stripped = chunk.rstrip()
            if stripped:
                return stripped[-1]
        return ""

    def last_word():
        text = "".join(out[-8:]).rstrip()
        m = re.search(r"[A-Za-z_$][A-Za-z0-9_$]*$", text)
        return m.group(0) if m else ""

    def copy_template(i):
        # Copy template literal body starting after the opening backtick (or
        # after a closing `}` of an expression). Returns (index, entered_expr).
        start = i
        while i < n:
            c = src[i]
            if c == "\\":
                i += 2
                continue
            if c == "`":
                out.append(src[start:i + 1])
                return i + 1, False
            if c == "$" and i + 1 < n and src[i + 1] == "{":
                out.append(src[start:i + 2])
                return i + 2, True
            i += 1
        raise ValueError("unterminated template literal")

    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        # Strings
        if c in "'\"":
            j = i + 1
            while j < n and src[j] != c:
                if src[j] == "\\":
                    j += 1
                elif src[j] == "\n":
                    raise ValueError("unterminated string literal")
                j += 1
            out.append(src[i:j + 1])
            i = j + 1
            continue

        # Template literals
        if c == "`":
            out.append("`")
            i, entered = copy_template(i + 1)
            if entered:
                template_stack.append(brace_depth)
                brace_depth = 0
            continue

        if c == "{":
            brace_depth += 1
            out.append(c)
            i += 1
            continue
        if c == "}":
            if template_stack and brace_depth == 0:
                out.append("}")
                i, entered = copy_template(i + 1)
                if entered:
                    brace_depth = 0
                else:
                    brace_depth = template_stack.pop()
                continue
            brace_depth -= 1
            out.append(c)
            i += 1
            continue

        # Comments and regex literals
        if c == "/":
            if nxt == "/":
                if out and out[-1] == " ":
                    out.pop()
                j = src.find("\n", i)
                i = n if j == -1 else j
                continue
            if nxt == "*":
                j = src.find("*/", i + 2)
                if j == -1:
                    raise ValueError("unterminated block comment")
                had_newline = "\n" in src[i:j]
                i = j + 2
                # Replace the comment with whitespace so tokens never merge.
                src_ws = "\n" if had_newline else " "
                src = src[:i] + src_ws + src[i:]
                n = len(src)
                continue
            prev = last_sig()
            if prev == "" or prev in _REGEX_AFTER_CHARS or last_word() in _REGEX_AFTER_WORDS:
                j = i + 1
                in_class = False
                while j < n:
                    ch = src[j]
                    if ch == "\\":
                        j += 2
                        continue
                    if ch == "\n":
                        raise ValueError("unterminated regex literal")
                    if ch == "[":
                        in_class = True
                    elif ch == "]":
                        in_class = False
                    elif ch == "/" and not in_class:
                        break
                    j += 1
                out.append(src[i:j + 1])
                i = j + 1
                continue
            out.append(c)
            i += 1
            continue

        # Whitespace
        if c in " \t\r\n":
            j = i
            has_newline = False
            while j < n and src[j] in " \t\r\n":
                has_newline |= src[j] == "\n"
                j += 1
            before = last()
            after = src[j] if j < n else ""
            if has_newline:
                if before and before != "\n" and after:
                    out.append("\n")
            elif before and before != "\n" and after:
                keep = (
                    (_is_word(before) and _is_word(after))
                    or (before in "+-" and after in "+-")
                    or before == "/" or after == "/"
                )
                if keep:
                    out.append(" ")
            i = j
            continue

        out.append(c)
        i += 1

    if template_stack:
        raise ValueError("unterminated template expression")
    return "".join(out).strip() + "\n"


# ---------------------------------------------------------------------------
# CSS
# ---------------------------------------------------------------------------

def minify_css(src):
    out = []
    i, n = 0, len(src)
    pending_space = False
    while i < n:
        c = src[i]
        if c == "/" and src.startswith("/*", i):
            j = src.find("*/", i + 2)
            if j == -1:
                raise ValueError("unterminated CSS comment")
            i = j + 2
            pending_space = True
            continue
        if c in "'\"":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            if pending_space and out and out[-1] not in "{};,:(":
                out.append(" ")
            pending_space = False
            out.append(src[i:j + 1])
            i = j + 1
            continue
        if c in " \t\r\n":
            pending_space = True
            i += 1
            continue
        if c in "{};,":
            pending_space = False
            if c == "}" and out and out[-1] == ";":
                out.pop()
            out.append(c)
            i += 1
            continue
        if pending_space and out and out[-1] not in "{};,:":
            out.append(" ")
        pending_space = False
        out.append(c)
        i += 1
    return "".join(out).strip() + "\n"


# ---------------------------------------------------------------------------
# HTML
# ---------------------------------------------------------------------------

_RAW_BLOCK = re.compile(r"(<(pre|textarea|script|style)\b.*?</\2\s*>)", re.I | re.S)


def minify_html(src):
    src = re.sub(r"<!--(?!\[if).*?-->", "", src, flags=re.S)
    parts = _RAW_BLOCK.split(src)
    out = []
    # split() with two groups yields: text, block, tagname, text, block, tagname, ...
    k = 0
    while k < len(parts):
        text = parts[k]
        out.append(re.sub(r"\s+", " ", text))
        if k + 1 < len(parts):
            out.append(parts[k + 1])
        k += 3
    html = "".join(out)
    html = re.sub(r">\s+<", "> <", html)
    return html.strip() + "\n"


MINIFIERS = {".js": minify_js, ".css": minify_css, ".html": minify_html}


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

def check_forbidden(name, text):
    problems = []
    for pattern, reason in FORBIDDEN:
        for m in pattern.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            problems.append("%s:%d (minified): %s" % (name, line, reason))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--keep-min", action="store_true",
                        help="also write the uncompressed minified files to dist/ (debugging)")
    args = parser.parse_args()

    sources = sorted(p for p in SRC_DIR.rglob("*") if p.is_file() and not p.name.startswith("."))
    if not sources:
        print("no source files in %s" % SRC_DIR, file=sys.stderr)
        return 1

    if DIST_DIR.exists():
        shutil.rmtree(DIST_DIR)
    DIST_DIR.mkdir(parents=True)

    rows, problems = [], []
    for path in sources:
        rel = path.relative_to(SRC_DIR).as_posix()
        raw = path.read_bytes()
        minifier = MINIFIERS.get(path.suffix)
        if minifier:
            try:
                text = minifier(raw.decode("utf-8"))
            except ValueError as e:
                print("error: %s: %s" % (rel, e), file=sys.stderr)
                return 1
            problems += check_forbidden(rel, text)
            minified = text.encode("utf-8")
        else:
            minified = raw
        packed = gzip.compress(minified, compresslevel=9, mtime=0)

        out = DIST_DIR / (rel + ".gz")
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(packed)
        if args.keep_min:
            (DIST_DIR / rel).write_bytes(minified)
        rows.append((rel, len(raw), len(minified), len(packed)))

    total_raw = sum(r[1] for r in rows)
    total_min = sum(r[2] for r in rows)
    total_gz = sum(r[3] for r in rows)
    width = max(len("TOTAL"), *(len(r[0]) for r in rows))

    print("Web build -> %s" % DIST_DIR.relative_to(WEB_DIR.parent))
    print()
    print("  %-*s %10s %10s %10s" % (width, "file", "raw", "minified", "gzip"))
    print("  %s" % ("-" * (width + 33)))
    for name, r, m, g in rows:
        print("  %-*s %10s %10s %10s" % (width, name, "{:,}".format(r), "{:,}".format(m), "{:,}".format(g)))
    print("  %s" % ("-" * (width + 33)))
    print("  %-*s %10s %10s %10s" % (width, "TOTAL", "{:,}".format(total_raw),
                                     "{:,}".format(total_min), "{:,}".format(total_gz)))
    print()
    pct = 100.0 * total_gz / BUDGET_BYTES
    over = total_gz > BUDGET_BYTES
    print("  Budget: {:,} / {:,} bytes gzipped ({:.1f}%)  {}".format(
        total_gz, BUDGET_BYTES, pct, "OVER BUDGET" if over else "OK"))

    if problems:
        print()
        print("Forbidden usage found:", file=sys.stderr)
        for p in problems:
            print("  " + p, file=sys.stderr)

    if over or problems:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
