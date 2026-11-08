# docs/

Project-wide design documents and shared contracts.

Start with [Setting Up](../README.md#setting-up) in the root README for the
board startup order, LED checks, phone internet test and separate web service.

| Document | What it is | Status |
|---|---|---|
| [architecture.md](architecture.md) | Topology, trust zones, request flow, failure behavior | Draft |
| [requirements.md](requirements.md) | R1–R7, verbatim. What the project is graded against. | Final |
| [api-contract.md](api-contract.md) | Browser REST API, served by the PC in the current lab | v0.3.1 |
| [device-api.md](device-api.md) | Board C outbound polling and command confirmation | v0.1.0 |
| [client-network.md](client-network.md) | Fixed Windows UI address and outbound-only routing | Firmware implemented; Windows/multiple-client acceptance pending |
| [spi-protocol.md](spi-protocol.md) | Frame format between Board A and Board B | v0.2.0 |
| [uart-link.md](uart-link.md) | Optional two-wire transport, wiring and recorded C6 hardware results | Envelope v0.2.0; C6 connectivity/restart checks recorded |
| [firmware-maintenance.md](firmware-maintenance.md) | Component responsibilities, concurrency rules and development checks | Current implementation guide |
| [testing.md](testing.md) | Local checks, dated results and remaining hardware acceptance | Software checks pass; hardware acceptance partial |
| [notes/web-login.md](notes/web-login.md) | Login, session expiry and lockout behavior | Implemented |
| [notes/web-control.md](notes/web-control.md) | Status polling, confirmed commands and unknown outcomes | Implemented |
| [notes/web-events.md](notes/web-events.md) | Shared polling, event filters and bounded log rendering | Implemented |
| [notes/web-design.md](notes/web-design.md) | Interface layout, theme and accessibility decisions | Implemented |
| [notes/web-packaging.md](notes/web-packaging.md) | PC asset serving and retained legacy image tooling | Current PC hosting guide |
| [s3-live-test.md](s3-live-test.md) | S3/C6 target build and live-service test procedure | S3 bench results recorded; fixed-IP gateway demo remains separate |
| [s3-test-evidence.md](s3-test-evidence.md) | Dated software and physical S3 evidence | Historical results; does not establish gateway isolation |
| [test-plan.md](test-plan.md) | Browser API evaluation categories and recorded results | Mock/software evidence with stated limits |
| [assets/](assets/) | Diagrams | n/a |

## Current owner (provisional)

Shared. The project lead reviews. Changes to the browser/device APIs or
SPI/UART contracts need review from the owners on **both** sides of the
interface (see [CONTRIBUTING.md](../CONTRIBUTING.md#changes-to-shared-contracts)).

## Building

Plain Markdown, nothing to build. Mermaid diagrams render on GitHub.
