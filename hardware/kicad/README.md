# hardware/kicad

The KiCad project for the inter-board SPI link (schematic and PCB). Empty
until the first design is committed.

## Current owner (provisional)

Carter Cattadoris. Any member may work here; the current owner reviews.

## Conventions

- Commit `.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, and any project-local
  symbol and footprint libraries.
- Don't commit `*.kicad_prl`, `*-backups/`, or `fp-info-cache` (git-ignored).
- Put fabrication outputs (Gerbers, drill files) in `kicad/fab/` only when
  ordering a board, and tag the commit.
- TODO: agree on the team's KiCad version.
