# hardware/kicad

KiCad 10 project for the gateway PCB: Board A and Board B (two ESP32-S3-WROOM-1
modules) on one 4-layer board, linked only by SPI (SCLK, MOSI, MISO, CS) and
the HS handshake line. Rev A is designed through ERC/DRC-clean outputs but not
fabricated.

## Current owner (provisional)

Carter Cattadoris. Any member may work here; the current owner reviews.

## Contents

| Path | Purpose |
|---|---|
| `IoT-Gateway.kicad_pro` | Project. Open this in KiCad 10 (files can't be opened by KiCad 9 or older) |
| `IoT-Gateway.kicad_sch` | Top sheet; sub-sheets `power.kicad_sch`, `module_a.kicad_sch`, `module_b.kicad_sch` |
| `IoT-Gateway.kicad_pcb` | 100 x 50 mm, 4 layers, JLCPCB JLC04161H-7628 stackup |
| `DESIGN_NOTES.md` | Requirements status, decisions and deviations log, power budget, stackup and routing notes |

Generated outputs (`outputs/`: schematic PDF, BOM, 3D renders, ERC/DRC
reports; `fab/`: Gerbers and drill files) and the Rev A generation scripts
(`scripts/`) are kept locally and git-ignored (see `.gitignore`). Regenerate
outputs from the KiCad files (File → Plot / Fabrication Outputs, or `kicad-cli`).

## Conventions

- Commit `.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, and any project-local
  symbol and footprint libraries.
- Don't commit `*.kicad_prl`, `*-backups/`, or `fp-info-cache` (git-ignored).
- Put fabrication outputs (Gerbers, drill files) in `kicad/fab/` only when
  ordering a board, and tag the commit. `fab/` is git-ignored, so add them
  with `git add -f`.
- KiCad version: this project uses KiCad 10 (team version still to be agreed).
