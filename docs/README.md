# docs/

Project-wide design documents and shared contracts.

Start with [Setting Up](../README.md#setting-up) in the root README for the
board startup order, LED checks, phone internet test and separate web service.

| Document | What it is | Status |
|---|---|---|
| [architecture.md](architecture.md) | Topology, trust zones, request flow, failure behavior | Draft |
| [requirements.md](requirements.md) | R1–R7, verbatim. What the project is graded against. | Final |
| [api-contract.md](api-contract.md) | Browser REST API, served by the PC in the current lab | v0.2.0 |
| [device-api.md](device-api.md) | Board C outbound polling and command confirmation | v0.1.0 |
| [client-network.md](client-network.md) | Fixed Windows UI address and outbound-only routing | Implemented; Windows acceptance pending |
| [spi-protocol.md](spi-protocol.md) | Frame format between Board A and Board B | v0.2.0 |
| [testing.md](testing.md) | How to test each component | Draft |
| [assets/](assets/) | Diagrams | n/a |

## Current owner (provisional)

Shared. The project lead reviews. The two contracts (`api-contract.md`,
`spi-protocol.md`) need review from the owners on **both** sides of the
interface (see [CONTRIBUTING.md](../CONTRIBUTING.md#changes-to-shared-contracts)).

## Building

Plain Markdown, nothing to build. Mermaid diagrams render on GitHub.
