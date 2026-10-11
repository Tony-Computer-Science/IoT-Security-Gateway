# docs/

Project-wide design documents and shared contracts.

| Document | What it is | Status |
|---|---|---|
| [architecture.md](architecture.md) | Topology, trust zones, request flow, failure behavior | Draft |
| [requirements.md](requirements.md) | R1–R7, verbatim. What the project is graded against. | Final |
| [api-contract.md](api-contract.md) | REST contract between the web UI and Board A | v0.1.0 |
| [spi-protocol.md](spi-protocol.md) | Frame format between Board A and Board B | Stub, all TODO |
| [testing.md](testing.md) | How to test each component | Draft |
| [assets/](assets/) | Diagrams | n/a |

## Current owner (provisional)

Shared. The project lead reviews. The two contracts (`api-contract.md`,
`spi-protocol.md`) need review from the owners on **both** sides of the
interface (see [CONTRIBUTING.md](../CONTRIBUTING.md#changes-to-shared-contracts)).

## Building

Plain Markdown, nothing to build. Mermaid diagrams render on GitHub.
