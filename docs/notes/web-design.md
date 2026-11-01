# Web interface design: why it looks the way it does

Code: `web/src/style.css` and `web/src/index.html`. Behaviour is described in
the other notes in this folder; this note covers only the visual choices.

## The brief

The page has one job: make the device's state and the gateway's health
unmistakable at a glance, including from the back of a lecture room, and make
a blocked command visible the moment it happens. It is styled like lab test
equipment, not like a website.

## Light, not dark

A projector cannot show black. In a partly lit room a dark page projects as
grey haze, and thin light text blurs. Dark ink on a light panel stays sharp
and also survives a black and white photo. There is one tested light theme;
the automatic dark mode was removed so the laptop's settings cannot change
what the audience sees.

## Palette and type

Six base colours: case grey for the page, an off-white faceplate, near-black
ink (17.6:1 contrast), dark grey for printed labels, a mid grey for wires and
borders, and one blue used only for the keyboard focus ring, so blue never
suggests a status. Text uses the computer's own fonts, because the page must
work offline from the board's flash. Character comes from size and weight
instead: the three status values are 40 pixel bold monospace capitals, the
health message is the largest sentence on the page, and labels are small,
bold, spaced capitals like the printing on an instrument panel.

## The signal path

The three status values sit on a drawing of the real hardware:
Board A, the SPI wire, Board B, the Wi-Fi hop, and the device C. The wire is
a double line (a bus); the radio hop is dotted. When a hop fails, that
segment changes colour, opens a gap, and shows the failure icon. So the
audience can see *where* the failure is, not just that there is one. In a
narrow window the path turns vertical.

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

Link down and target unreachable were kept far apart on purpose. They are
different faults with different fixes (a wire between the gateway boards
versus the radio link to the device), so they differ in icon, pattern,
colour, and position on the signal path.

## The LED lamp follows confirmed state only

The LED value has a lamp: filled with a ring when on, hollow when off. CSS
reads it from a `data-value` attribute that the code sets only from the
gateway's answer, never from the button the user pressed. While a command
is on its way, the lamp keeps showing the last confirmed state. Lighting it
on the click would be an optimistic update, and R7 says a failed command
must never look like it worked.

## HOLD

When a status update fails, the last known values stay on screen and each one
gets a small HOLD tag, borrowed from multimeters that freeze a reading. The
message above says the same thing in words.

## Icons, motion, and access

- Icons are drawn by hand in SVG, with no icon library or font. Five sit in
  the page; the six state icons live in the stylesheet, because the state
  text is written by the code and would erase anything inside it. Every icon
  is hidden from screen readers and always sits next to text.
- The only motion confirms something the user did: the command keys move
  down when pressed, and the pending dots pulse while a request is in flight.
  Both stop when the system asks for reduced motion.
- Every control shows a thick blue focus ring when reached with the keyboard.
- Disabled buttons lose their fill and get a dashed edge, so they look as
  unavailable as they are.

The whole interface is 11.3 KB gzipped, under a fifth of its 60 KB budget.
