# Requirements

The project is graded against these requirements. They are recorded verbatim.
**Do not reword them.** Refer to them by number (R1–R7) in issues, commits,
pull requests, and code comments.

---

**R1 Controlled entry point.** The target is reachable from the user-side
network only through the gateway's approved interface. The protected target
does not also join the user-side network or expose another control path.

**R2 Authentication.** A provisioned credential is required before any device
command is accepted. Authentication uses the encrypted client connection.
Sessions expire and are invalidated on restart. Credentials and session tokens
never appear in logs or in the repository.

**R3 Command access control.** Only status reads and explicit output changes
are permitted. Unknown paths, methods, commands, invalid values, and oversized
inputs are rejected. Clients cannot choose an arbitrary destination.

**R4 Repeated-request protection.** Configurable limits apply to failed
authentication attempts and to command requests. Connection counts, input
buffers, and timeouts are bounded.

**R5 Event visibility.** A bounded log records authentication outcomes,
rejected commands, rate-limit events, and target connection failures. Logs are
available only through an authorized interface.

**R6 Normal device operation.** An authorized user can read status and control
the output. Unavailable targets and invalid requests are reported clearly.
Rejected requests leave target state unchanged.

**R7 Restart behavior.** Commands remain disabled until both gateway boards and
their link are ready. Restarting either board invalidates outstanding requests.
Loss of connectivity never enables a bypass path.

---

## Where each requirement lives

This table is a navigation aid, not part of the requirements text. Keep it
current as the implementation lands.

| Req | Primary location(s) |
|---|---|
| R1 | Topology (`docs/architecture.md`), `hardware/`, `firmware/bridge-board` (`ap_mode`, `target_client`) |
| R2 | `firmware/station-board` (`net_station`, `auth`), `web/src/api.js` (in-memory token) |
| R3 | `firmware/station-board` (`policy`), `firmware/bridge-board` (`spi_device` frame validation, fixed target) |
| R4 | `firmware/station-board` (`policy`, `net_station` connection and timeout bounds) |
| R5 | `firmware/station-board` (`eventlog`), `GET /api/events` |
| R6 | REST contract (`docs/api-contract.md`), `web/` |
| R7 | `docs/spi-protocol.md` (link readiness), `spi_host`, `spi_device`, session invalidation in `auth` |
