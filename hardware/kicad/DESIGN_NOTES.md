# IoT Security Gateway PCB — Design Notes

2026-10-25 · Carter Cattadoris · KiCad 10.0.7 · Rev A (not for fabrication)

Two ESP32-S3-WROOM-1-N8R2 modules on one 4-layer board, 100 × 50 mm. Board A (U1) is the SPI controller and Board B (U2) the SPI peripheral. The only signals between the two halves are SCLK, MOSI, MISO, CS and HS. Power is shared and USB data is not.

## Status at a glance

> **Final state:** after the scripted Rev A baseline, Carter made hand edits in KiCad (routing changes) and added teardrops to pads and vias (220 teardrop zones). Zones were refilled with KiCad's own refill before the final outputs were generated. Routing details further down (lengths, via counts, which nets use L4) describe the scripted baseline and may differ slightly from the final board. The checks below are from the final board.

| Check | Result |
| --- | --- |
| ERC (`kicad-cli sch erc`) | **0 errors**, 3 justified warnings (see D-05) |
| DRC (`kicad-cli pcb drc --schematic-parity`) | **0 errors, 0 unconnected, 0 parity issues**, 2 justified warnings (see D-17) |
| Netlist vs. intended connectivity (`scripts/verify_netlist.py`) | **29/29 nets match**, no unexpected nets |
| Track corners (`scripts/corners.py`) | **No 90° corners**: all 117 bends are 45° or shallower |
| Boundary check (HW-02) | Only SPI_SCLK/MOSI/MISO/CS/HS, power and GND connect the A and B sheets and halves |

## Deliverables

| Item | Location |
| --- | --- |
| KiCad project (4 hierarchical sheets + PCB) | `IoT-Gateway.kicad_pro`, `IoT-Gateway.kicad_sch` (top), `power.kicad_sch`, `module_a.kicad_sch`, `module_b.kicad_sch`, `IoT-Gateway.kicad_pcb` |
| Schematic PDF | `outputs/IoT-Gateway_schematic.pdf` |
| Gerbers + Excellon drill (PTH/NPTH, drill maps, job file), zipped | `fab/IoT-Gateway_gerbers_drill.zip` |
| BOM (CSV, grouped) | `outputs/IoT-Gateway_BOM.csv` |
| 3D renders | `outputs/IoT-Gateway_3D_top.png`, `outputs/IoT-Gateway_3D_bottom.png` |
| ERC / DRC reports | `outputs/ERC_report.rpt`, `outputs/DRC_report.rpt` |
| Generator scripts (reproducible build) | `scripts/` (see "Rebuilding") |

## Initial check-in answers

| Question | Answer |
| --- | --- |
| KiCad version | **KiCad 10** (10.0.7 is installed; the PRD default was 9) |
| Board size limit | Default: about 100 × 50 mm, set by placement → final **100.0 × 50.0 mm** |
| Fab and stackup | **JLCPCB 4-layer, 1.6 mm, JLC04161H-7628** |
| Module part number | Default: **ESP32-S3-WROOM-1-N8R2** |
| Output location | **Existing project `~/Formula/IoT-Gateway/IoT-Gateway.kicad_pro`**, built in place (not a new `iot-gateway-pcb/` folder) |
| Autorouter | Download Freerouting allowed. It is used for non-critical nets only; USB, SPI/HS and the power paths are routed by script |

## Library substitutions

None. Every part has a KiCad 10 standard-library symbol and footprint, and no custom symbols or footprints were created.

| Ref | Symbol | Footprint |
| --- | --- | --- |
| U1, U2 | `RF_Module:ESP32-S3-WROOM-1` | `RF_Module:ESP32-S3-WROOM-1` (antenna area and Espressif keepout zone included) |
| J1, J2 | `Connector:USB_C_Receptacle_USB2.0_16P` | `Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal` |
| U3, U4 | `Power_Protection:USBLC6-2SC6` | `Package_TO_SOT_SMD:SOT-23-6` |
| U5, U6 | `Power_Management:LM66100DCK` | `Package_TO_SOT_SMD:SOT-363_SC-70-6` |
| U7 | `Regulator_Switching:TLV62569DBV` | `Package_TO_SOT_SMD:SOT-23-5` (see D-01) |
| L1 | `Device:L` | `Inductor_SMD:L_Coilcraft_XAL4020-XXX` |
| SW1–SW4 | `Switch:SW_Push` | `Button_Switch_SMD:SW_SPST_B3U-1000P` |
| R, C | `Device:R`, `Device:C` | 0603, with 0805 for the 10 µF and 22 µF parts (D-11) |
| H1–H4 | `Mechanical:MountingHole_Pad` | `MountingHole:MountingHole_3.2mm_M3_Pad_Via` (plated, GND) |

Pin numbers were checked against the datasheets: ESP32-S3-WROOM-1 Table 3-1, TLV62569 §5 (DBV column), and LM66100 §5. The ST USBLC6-2 datasheet could not be downloaded (st.com blocks scripted access), so the USBLC6-2SC6 pinout (1/6 = I/O1, 3/4 = I/O2, 2 = GND, 5 = VBUS) comes from the KiCad library symbol.

## Decisions, assumptions and deviations log

| ID | Decision / deviation | Reason |
| --- | --- | --- |
| D-01 | **TLV62569DBVR uses SOT-23-5, not the SOT-23-6 the PRD states.** | The DBV package is 5-pin (TI SLVSDG1C §5: EN 1, GND 2, SW 3, VIN 4, FB 5). The SOT-23-6 version is the TLV62569P**DDC** (adds PG). I kept the BOM part number and used its real package, per the PRD's "footprints must match the real packages". |
| D-02 | **6.8 pF feed-forward cap C5** added across R1 (453k). | TLV62569 §8.2.2.2 recommends 6.8 pF when R2 = 100 kΩ. |
| D-03 | **LM66100 ORing uses TI's §9.2.1 "Dual Ideal Diode ORing"**: CE of each device tied to the *opposite* VBUS. ST is tied to GND (pin table: "connect to GND if not required"). NC (pin 4) is left open. | Higher VBUS wins with make-before-break and no reverse-current path into either host (HW-06). I rejected the CE = VOUT "always-on RCB" mode (§8.3.2): with two near-equal 5 V hosts it lets up to V_OFF/(2·R_ON) ≈ 0.4 A flow between them before the comparator trips (V_OFF up to 80 mV, R_ON ≈ 0.1 Ω), which is a back-feed. With exactly equal VBUS both channels may sit off; the board then runs through the body diodes (~0.5 V drop, still well inside the buck's 2.5–5.5 V input range). |
| D-04 | **100 kΩ bleed resistor on each VBUS** (R3, R6) added, not in the PRD BOM. | Without it an unplugged port's VBUS floats on LM66100 OUT→IN leakage (up to 2.7 µA). That node drives the *other* device's CE and could turn it off. 100 kΩ holds it below 0.3 V. |
| D-05 | **ERC pin map relaxed for two pin-type pairs (error → warning):** power_out↔power_out and open_collector↔power_out. | The two LM66100 VOUT pins are paralleled on VSYS by design (ORing). The LM66100 ST (open-drain) pins are tied to GND, which carries a PWR_FLAG. These are the 3 remaining ERC warnings. |
| D-06 | **Mounting holes at the corners (3.7 mm inset). The module feed points end up ~8 mm from the board's long edge rather than at the corner.** | Espressif's guidelines (§1.4.7, Fig. 24) prefer the antenna overhanging the edge with its feed point near the board edge. Each module is placed at a short edge with the antenna fully overhanging (no board material under it), feed point toward the near long edge (U1 top-left, U2 bottom-right). The M3 hole and its courtyard take the extreme corner. |
| D-07 | **Antenna keepout:** the module footprint carries Espressif's 15 mm keepout (48 × 21 mm) as an all-layer rule area. Board-level all-layer rule areas `ANT_KEEPOUT_U1/U2` repeat it and extend 0.75 mm onto the board along the antenna edge (no tracks, vias or pours). | HW-10. With the antenna overhanging, the whole keepout sits off-board, so L2 stays unbroken (HW-13). |
| D-08 | **J1/J2 front faces are recessed 0.5 mm from the board edge.** | Keeps the receptacle courtyard from overlapping the module's off-board antenna keepout courtyard. A plug still mates fully. |
| D-09 | **Board hole-to-copper clearance set to 0.15 mm** (instead of 0.2 mm). | Vendor footprint geometry: the GCT USB4105 GND pads sit 0.175 mm from the footprint's own NPTH alignment pegs. |
| D-10 | **SPI/HS lines each make one short L4 hop near the centreline** (PRD asked for L1 only). | Unavoidable: U2 is U1 rotated 180°, as it must be for antennas on opposite edges. That makes the pin order at B the exact reverse of A (A top→bottom MISO, SCLK, MOSI, CS; B CS, MOSI, SCLK, MISO), and a planar single-layer route cannot reverse a bus. The lanes interleave at 0.8 mm pitch so each hop is short: MOSI 0.8, CS 2.4, SCLK 4.0, MISO 7.2, HS 5.6 mm. Everything else of the bus is on L1 over L2. HS also hops under each module's USB pair (it starts on the far side of the module from the bus). |
| D-11 | 10 µF and 22 µF capacitors in **0805**; all other passives 0603. | Realistic X5R/X7R availability and DC-bias derating at these values (the TLV62569 reference design uses 0805). |
| D-12 | **Output capacitance:** 22 µF at the buck plus 22 µF + 100 nF at each module 3V3 pin (66 µF nominal). | TLV62569 Table 4 lists L = 2.2 µH with 2 × 22 µF and 100 µF as tested-stable for V_OUT ≥ 1.8 V. Espressif asks for 22 µF + 0.1 µF at the module. |
| D-13 | **USB 90 Ω differential = 0.29 mm width, 0.12 mm gap** on L1 over L2. | Computed with `scripts/impedance.py`, which implements the coupled-microstrip model KiCad's calculator uses (Hammerstad–Jensen + Kirschning–Jansen, static). Inputs: JLC04161H-7628 (7628 prepreg 0.2104 mm, εr 4.4, 1 oz outer copper). Result: Zdiff 90.1 Ω. Sanity check: the same model gives 51.7 Ω for a 0.35 mm single-ended trace. KiCad's PCB calculator is GUI-only, so it could not be run headlessly. Solder mask (not modelled) will pull the result down by about 2–4 Ω. Necked to 0.2 mm inside the connector fan-out. |
| D-14 | **USB-C A/B row joins:** D+ (A6/B6) joined on L1. D− (A7/B7) joined with a 0.5/0.25 mm via pair and a 1.4 mm L4 jumper. The pair passes through the USBLC6 flow-through (pins 1→6, 3→4) with no stubs. | The 0.5 mm pad pitch interleaves D+/D− and cannot be joined on one layer. D+ is 3.06 mm longer than D− (24.19 vs 21.13 mm, ≈ 20 ps), which is negligible for 12 Mb/s full-speed USB. |
| D-15 | **L3 split:** +3V3 plane over the whole board plus a VSYS island notched in from the bottom edge around the ORing and buck input. VBUS1/2 run as tracks on L1, not L3. | Matches the PRD's L3 intent (VSYS around the ORing/buck, 3V3 under both modules). Keeping VBUS off L3 keeps the 3V3 plane continuous between the two modules. |
| D-16 | **L4 use beyond "short escapes":** SPI/HS hops (D-10), the USB D− jumper, the VBUS pad-pair links under each USB-C connector (~5 mm), the VBUS2 crossing under the SPI lanes (12 mm), and Freerouting's A_EN route (11.5 mm) and VBUS1/VBUS2 local links. Total L4 copper is 86 mm vs 771 mm on L1, and the rest of L4 is a GND pour stitched to L2. | VBUS2 must cross the SPI corridor, and the connector's two VBUS pad pairs straddle the data pins. |
| D-17 | **Module footprint silkscreen trimmed**: the 8 segments that run off-board around the overhanging antenna are moved to User.Drawings. | Avoids silk-to-edge violations. This causes the 2 remaining DRC warnings ("footprint does not match library copy", U1/U2), which are expected. |
| D-18 | **Schematic style:** your guide (boxes, Anton titles, wires inside blocks, power symbols, labels between blocks, internal net labels) is followed with three exceptions. (a) Four hierarchical sheets instead of one flat sheet (PRD requirement). (b) Standard-library passives instead of CitrusComponents (PRD rule). (c) PRD reference designators (U1–U7, J1, J2) instead of functional prefixes, because the requirements cite them. Title block company is "Syracuse University" (not a team project). There are no test points (PRD "barebones" scope). | PRD takes precedence over the general style guide. |
| D-19 | **Switch assignment:** SW1 = BOOT A, SW2 = EN A, SW3 = BOOT B, SW4 = EN B. EN RC = 10 kΩ / 1 µF. GPIO0 BOOT uses the internal pull-up. | Espressif hardware design guidelines. |
| D-20 | **CC pull-downs:** 5.1 kΩ on CC1 and CC2 of each port (sink only). SBU1/2 not connected. Shield tied directly to GND. | USB-C sink default. A direct shield tie is the simplest choice for a non-fabricated board. |
| D-21 | **H1–H4 are excluded from the BOM CSV** (`in_bom = no`). | They are drilled holes, not purchased parts. |
| D-23 | **All bends are 45°.** The scripted USB, SPI, VBUS and buck routes are drawn with 45° bends; the USB-C D+ rows converge at 45° instead of meeting in a T. A post-route pass (`chamfer_corners` in `post_route.py`) chamfers any remaining ≥90° autorouter corner, with a clearance check, and collapses sub-0.25 mm jogs. | Rev A review feedback. |
| D-24 | **No tracks are locked in the delivered board.** Scripted routes are locked only while Freerouting runs (exported as `fix`), then unlocked. | So the board can be edited by hand. |
| D-25 | **Silkscreen artwork** (`scripts/silk_art.py`): CyberGuards badge (shield + padlock, Anton / Bebas Neue) top centre; dashed security-boundary line on the centreline with "SECURITY BOUNDARY · SPI + HS ONLY" and "A · UPLINK / B · IoT AP" tags; BOOT/RESET labels on SW1–SW4; USB-C A/B; board and power-section labels; stackup line. Every label is placed by a collision-checked search that keeps clear of pads, footprint silk, component bodies and reference designators. | Group branding and readable functions. Fonts are system TrueType fonts (Anton, Bebas Neue); KiCad substitutes its stroke font on a machine without them, but the Gerbers already contain the rendered outlines. |
| D-26 | **Filleted board corners (R 3.7 mm) and plated M3 mounting holes.** H1–H4 use `MountingHole_3.2mm_M3_Pad_Via` (6.4 mm pad, 8-via ring) and are tied to GND in the schematic (`Mechanical:MountingHole_Pad`). Each corner arc is concentric with its hole, leaving a 0.5 mm copper-to-edge ring. Stitching vias keep hole-to-hole spacing from every drilled hole. | Rev A review feedback. The holes also bond the GND pours to a chassis/standoff. |
| D-22 | **Inner layers are typed "power" in the stackup** (In1.Cu GND, In2.Cu 3V3/VSYS), with no tracks on L2 or L3. | HW-13 and the PRD stackup table. |

## Requirement status

| ID | Requirement | Status | Evidence |
| --- | --- | --- | --- |
| HW-01 | Two ESP32-S3-WROOM-1-N8R2, U1 (A) and U2 (B) | **Met** | BOM, schematic sheets Module A/B |
| HW-02 | Only SCLK, MOSI, MISO, CS, HS + GND between A and B | **Met** | The top sheet joins A and B only through `SPI_*` nets. In copper, the only signal nets crossing x = 50 mm are the 5 SPI/HS lines (plus VBUS1 and BUCK_FB inside the centre power cluster, which belong to neither module) |
| HW-03 | A controller, B peripheral; B signals A only via HS | **Met** | Hierarchical label directions: A drives SCLK/MOSI/CS, B drives MISO/HS |
| HW-04 | FSPI IO_MUX pins, ≥10 MHz, routing not limiting below 40 MHz | **Met** (layout review) | GPIO10/11/12/13 = FSPICS0/FSPID/FSPICLK/FSPIQ (WROOM-1 Table 3-1). Lines are about 75 mm long (~0.5 ns, so electrically short at 40 MHz), with 33 Ω source termination, over L2 GND. Each line has one via pair at the centreline, and HS has one more under each module's USB pair (D-10) |
| HW-05 | J1 → A native USB, J2 → B native USB | **Met** | GPIO19/20 (pins 13/14) through U3/U4 to J1/J2 only |
| HW-06 | Either port powers the board; no back-feed | **Met** (analysis) | D-03, D-04 |
| HW-07 | 3V3 ≥ 1 A continuous | **Met** | TLV62569 rated 2 A. Budget below. L1 Isat 5.6 A > 1.3 × IL,max |
| HW-08 | BOOT on GPIO0, EN switch with RC delay | **Met** | SW1–SW4, R9/C9, R14/C12 |
| HW-09 | ESD on D+, D−, VBUS of both ports | **Met** | USBLC6-2SC6 I/O1, I/O2 and VBUS pins on each port |
| HW-10 | Antennas at opposite edges, keepout on all four layers | **Met** | U1 antenna off the left edge, U2 off the right. Footprint and board rule areas on F.Cu, In1, In2, B.Cu (D-06, D-07) |
| HW-11 | GPIO3/45/46 no functional nets; GPIO0 only BOOT | **Met** | No-connect flags on pins 15, 26, 16. Pin 27 goes only to SW1/SW3 |
| HW-12 | PCB pin map = breadboard wiring | **Met on the PCB side** | Pin map identical on A and B and equal to the PRD table. Rewiring the breadboard is out of scope (open item) |
| HW-13 | 4-layer, unbroken GND on L2 except under antennas | **Met** | L2 is one GND zone with no tracks, only via clearances. The antennas overhang the board |
| FW-01 | AP channel rule | Out of scope (firmware) | — |

## Power budget

From ESP32-S3-WROOM-1 datasheet Table 6-4 (TX peak 355 mA for 802.11b at 20.5 dBm, RX ≈ 95 mA):

| Load | 3V3 | VBUS at 90 % efficiency |
| --- | --- | --- |
| Both modules RX / typical traffic | ≈ 0.19–0.25 A | ≈ 0.14–0.18 A |
| Both modules TX peak at once | 0.71 A | 0.52 A (short bursts; bulk capacitance carries them) |
| Design rating (HW-07) | 1.0 A | 0.73 A |

- **Buck:** TLV62569, 2 A. V_OUT = 0.6 × (1 + 453k/100k) = 3.32 V. Inductor ripple at 5 V in: ΔI_L = 3.3 × (1 − 3.3/5)/(2.2 µH × 1.5 MHz) = 0.34 A, so I_L,max = 1.17 A at 1 A load and 2.17 A at 2 A. XAL4020-222ME saturates at 5.6 A.
- **ORing:** LM66100 I_MAX is 1.5 A and R_ON ≤ 110 mΩ at 5 V, so drop is ≤ 0.08 V at 0.73 A.
- **USB 2.0 host limit:** the two-module TX peak reaches the 500 mA limit only in bursts, as the PRD anticipated.

## Layout summary

- **Board:** 100 × 50 mm with R 3.7 mm filleted corners, 1.6 mm, 4 layers. JLC04161H-7628 stackup is stored in the board file. Rules: 0.1 mm min clearance and track, 0.2 mm min drill, 0.3 mm copper-to-edge.
- **Net classes:**

  | Class | Track | Clearance | Notes |
  | --- | --- | --- | --- |
  | Default | 0.2 mm | 0.15 mm | |
  | SPI | 0.2 mm | 0.15 mm | |
  | USB | 0.29 mm | 0.12 mm | diff pair 0.29 / 0.12 mm |
  | Power (VBUS1/2, VSYS, BUCK_SW) | 0.6 mm | 0.15 mm | 0.8 mm vias |
  | 3V3 | 0.5 mm | 0.15 mm | 0.8 mm vias |
  | GND | 0.4 mm | 0.15 mm | |

  The schematic wire colours follow your style guide.
- **Placement:** U1 and J1 at the left short edge, U2 and J2 at the right. The ORing and buck sit at the bottom of the centreline. The SPI bus crosses the middle with series resistors at the driving end: R10–R12 at A, R16/R17 at B.
- **Pull-ups/downs:** R13 (HS pull-down) at A, R15 (CS pull-up) at B.
- **Pours:** L1 and L4 GND (solid pad connection). L2 solid GND. L3 split into 3V3 and a VSYS island.
- **Ground stitching:** 59 GND stitching vias (placed after the silkscreen, so no via drills sit under labels) around the edge and as a fence either side of the centreline. 160 vias in total.
- **Plane fanout:** every GND/3V3/VSYS SMD pad has a dedicated plane via, placed by a collision-checked fanout pass.
- **Routing split:** scripted routes cover the USB pairs, SPI/HS, the buck SW/output/input paths, the VBUS links and the VBUS2 trunk. Freerouting 1.9.0 routed the rest (CC, EN, BOOT, short local links).

## Rebuilding

> **Warning:** the KiCad files are now edited by hand (in parallel with scripted changes). `scripts/build.sh` regenerates the schematic and board **from scratch** and would discard every manual edit. Treat the scripts as the record of how Rev A was first generated, not as the source of truth. Make further changes in KiCad or with targeted scripts that modify the existing files.


`scripts/build.sh <work_dir> <python-with-shapely> <freerouting-1.9.0.jar>` regenerates everything from scratch and reruns ERC and DRC:

1. Project settings (`gen_pro.py`)
2. Schematic (`gen_sch.py`)
3. ERC
4. Netlist check (`verify_netlist.py`)
5. Placement, scripted routes, fanout and zones (`gen_pcb.py`)
6. Autorouting (`route.py`): the plane nets are stripped from the DSN and Freerouting runs under `xvfb-run` because 1.9 needs a display.
7. Pours, stitching and silkscreen (`post_route.py`)
8. DRC

Requirements: KiCad 10 Python (`pcbnew`), `shapely`, Java ≥ 17 and Xvfb. Freerouting 2.5 needs Java 25, and 2.1 ignores its pass limit, so 1.9.0 is the version that works here.

**Note:** your `~/.config/freerouting/freerouting.json` has telemetry/analytics enabled. The first two runs here (2.5 CLI, 2.1) used it as-is. Later runs passed the disable-analytics flags, but the file itself was not changed.

## Open items

- [ ] **Module variant (PRD open item):** confirm the variant on the DevKitC-1 boards. If they carry N8R8, the firmware PSRAM setting changes. N8R2 is used here (quad PSRAM, so GPIO35–37 stay free, though unused).
- [ ] Rewire the breadboard to the PCB pin map (HW-12; out of scope).
- [ ] Firmware team to accept FW-01.
- [ ] **Costed BOM:** the CSV has manufacturer part numbers for the ICs, connector and inductor but no prices. Passives are generic.
- [ ] If the board is ever fabricated:
  - check the diff-pair impedance with JLC's own calculator, including solder mask;
  - consider a test point on EN and GPIO0 (left out per the barebones scope);
  - confirm the 0.5 mm USB-C recess against the enclosure.
