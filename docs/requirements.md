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

This table is a navigation aid, not part of the requirements text above.
It points to related current code, not a claim of full compliance. The web
service is external to the boards and runs on a local computer for the demo;
see [Setting Up](../README.md#setting-up) and the
[architecture differences](architecture.md#original-graded-requirements).

| Req | Primary location(s) |
|---|---|
| R1 | `docs/architecture.md`, station/bridge routing policy; original command-only topology is not implemented |
| R2 | `web/mock/server.py` (session/login logic reused by the service), `web/src/api.js` (in-memory token); current lab HTTP is not encrypted |
| R3 | `web/device_server.py` (command validation), `firmware/target-device/main/target_protocol.c`; router traffic is not a fixed-target command allow-list |
| R4 | `web/device_server.py` (bounded requests/pending commands), reused login/rate limits in `web/mock/server.py` |
| R5 | Service event log in `web/mock/server.py`, exposed through authenticated `GET /api/events` |
| R6 | `docs/api-contract.md`, `web/`, `firmware/target-device/` |
| R7 | `firmware/common/gateway_link`, `firmware/common/gateway_protocol`, device boot/command invalidation and service sessions; local LED control does not require the upstream link |
