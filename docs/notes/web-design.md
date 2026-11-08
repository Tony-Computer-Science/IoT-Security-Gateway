# Web interface design: why it looks the way it does

Code: `web/src/style.css` and `web/src/index.html`. Visual choices only.

## The brief

Make the device's state and the gateway's health unmistakable from the back
of a lecture room, and make a blocked command visible the moment it happens.
It is styled like lab test equipment, not like a website.

## Light by default, dark by choice

A projector cannot show black: a dark page projects as grey haze and light
text blurs. Dark ink on a light panel stays sharp, even in a black and white
photo, so light is what we tested for the demo. The page therefore opens in
light on every load, whatever theme the laptop is in. A header switch offers
dark for operators who want it. The choice is deliberately not saved (no
storage, no cookie): a reload always returns to light, so the demo can never
start in a theme nobody tested.

## Palette and type

Six base colours: case grey, an off-white faceplate, near-black ink, dark
grey labels, mid grey wires, and one blue used only for the keyboard focus
ring, so blue never suggests a status. Dark mode swaps the same six roles.
Fonts are the computer's own, since the page runs from the local PC server
without external font requests; character comes from size and weight, with labels in small spaced
capitals like instrument panel printing.

## Hierarchy on the control view

The first version gave each segment two labels above its value, the values
sat at different heights, and the LED was squeezed into a corner. Now:

- **One unit, one label, one value.** The medium ("UART wire, bridge board B
  to station board A") is small print beneath the value, not a second heading.
- **One row of readouts.** All three values are the same size and weight and
  share one baseline, so the eye scans a single line.
- **The output is not an afterthought.** The LED has the same weight as the
  other two, and its keys and command result sit directly beneath it.
- **The diagram follows the hardware.** It reads B, A, C: the bridge board B
  joins the upstream Wi-Fi, a UART wire runs from B to the station board A,
  and A creates the private Wi-Fi that the device C joins.
- **The diagram recedes.** The B, A, C boxes and wires are small, grey and
  thin, because they answer "where?" only when something fails. A failed hop
  then takes its state colour, opens a gap, and shows the failure icon.
- **Health comes first.** A failure message sits at the top of the panel.

## The six states (requirements R6 and R7)

Each state differs in icon and pattern, not only colour, so all six stay
distinct when projected badly or printed in grayscale, in both themes:

| State | Icon | Pattern |
|---|---|---|
| ok | check mark | thin solid border |
| pending | three dots | dashed border |
| denied | stop sign | thick solid bar on the left |
| rate limited | stopwatch | double border |
| target unreachable | Wi-Fi with a slash | dotted border |
| link down | broken chain link | diagonal hatching |

Link down and target unreachable are different faults with different fixes
(the UART wire between the gateway boards, or A's Wi-Fi link to the device), so
they differ in icon, pattern, colour, and position on the path.

## The LED lamp follows confirmed state only

The lamp is filled when on and hollow when off. CSS reads it from a
`data-value` attribute set only from the gateway's answer, never from the
button pressed. Lighting it on the click would be an optimistic update, and
R7 says a failed command must never look like it worked. When a status
update fails, values stay with a HOLD tag, like a multimeter's frozen reading.

## Icons, motion, and access

- Hand-drawn SVG icons, no library, each hidden from screen readers and next
  to text. The six state icons live in the stylesheet because the code writes
  the state text and would erase anything inside it.
- Motion only confirms a user action (key travel, pending pulse) and stops
  under reduced motion. Every control has a thick keyboard focus ring.

Run `python3 web/scripts/build.py` for the current gzipped size and 60 KiB
budget check. The UI runs on the fixed-IP PC, not in ESP32 flash.

In live-device mode, the diagram reads API service, Wi-Fi, target. Its device
channel is measured by HTTP heartbeat; it does not measure the A/B link or
prove gateway isolation. The B/A/C labels describe the mock topology only.
