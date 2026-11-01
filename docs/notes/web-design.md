# Web interface design: why it looks the way it does

Code: `web/src/style.css` and `web/src/index.html`. Visual choices only.

## The brief

Make the device's state and the gateway's health unmistakable from the back
of a lecture room, and make a blocked command visible the moment it happens.
It is styled like lab test equipment, not like a website.

## Light, not dark

A projector cannot show black: a dark page projects as grey haze and light
text blurs. Dark ink on a light panel stays sharp, even in a black and white
photo. Automatic dark mode was removed so the laptop's settings cannot change
what the audience sees.

## Palette and type

Six base colours: case grey, an off-white faceplate, near-black ink (17.6:1
contrast), dark grey labels, mid grey wires, and one blue used only for the
keyboard focus ring, so blue never suggests a status. Fonts are the
computer's own, since the page runs offline from the board's flash.
Character comes from size and weight: status values are 40 pixel bold
monospace capitals, and labels are small spaced capitals like the printing
on an instrument panel.

## The signal path

The three status values sit on a drawing of the real hardware:
Board A, the SPI wire, Board B, the Wi-Fi hop, and the device C. The wire is
a double line (a bus); the radio hop is dotted. A failed hop changes
colour, opens a gap, and shows the failure icon, so the audience sees
*where* the failure is. In a narrow window the path turns vertical.

## The six states (requirements R6 and R7)

Each state differs in icon shape and border pattern, not only colour, so all
six stay distinct when projected badly or printed in grayscale:

| State | Icon | Pattern |
|---|---|---|
| ok | check mark | thin solid border |
| pending | three dots | dashed border |
| denied | stop sign | thick solid bar on the left |
| rate limited | stopwatch | double border |
| target unreachable | Wi-Fi with a slash | dotted border |
| link down | broken chain link | diagonal hatching |

Link down and target unreachable are different faults with different fixes
(the wire between the gateway boards, or the radio link to the device), so
they differ in icon, pattern, colour, and position on the path.

## The LED lamp follows confirmed state only

The LED lamp is filled when on and hollow when off. CSS reads it from a
`data-value` attribute that the code sets only from the gateway's answer,
never from the button pressed, so during a command it keeps the last
confirmed state. Lighting it on the click would be an optimistic update,
and R7 says a failed command must never look like it worked.

## HOLD

When a status update fails, the last known values stay and get a HOLD tag,
borrowed from multimeters that freeze a reading.

## Icons, motion, and access

- Icons are hand-drawn SVG, no library. Five are in the page; the six state
  icons are in the stylesheet, because the code writes the state text and
  would erase anything inside it. Each is hidden from screen readers and
  sits next to text.
- The only motion confirms a user action: keys move when pressed, and the
  pending dots pulse. Both stop under reduced motion.
- Every control shows a thick blue keyboard focus ring. Disabled buttons lose
  their fill and get a dashed edge.

The whole interface is 11.3 KB gzipped, under a fifth of its 60 KB budget.
