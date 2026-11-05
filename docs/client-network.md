# Multiple clients, fixed UI address and outbound-only routing

## Address plan

Station's default IoT-Gateway AP accepts **four simultaneous Wi-Fi clients**,
including the UI computer. Maximum associated clients is configurable from
1 to 10 under **Station board WPA2 access point**. DHCP address count does not
increase the Wi-Fi association limit. All clients share the UART link's
bandwidth. Its queue is bounded; heavy simultaneous traffic can cause drops.

| Role | Address |
|---|---|
| Station gateway | 192.168.78.1/24 |
| Windows UI computer, manually configured | **192.168.78.10/24** |
| Automatic DHCP clients | 192.168.78.100–192.168.78.199 |

The UI address is excluded from the DHCP pool, preventing automatic leases
from taking it. Configure .10 on exactly one computer; this exclusion does
not assign an IP automatically or bind it to a MAC address. Manual duplicate
addresses can still conflict. Other IoT clients can continue using DHCP and
use **192.168.78.10** as their server address. Use the UI service's actual port.

The AP uses a private /24 with gateway .1. Its subnet, DHCP start/end host
octets, fixed UI host octet and maximum clients are configurable in menuconfig.
Startup rejects pool/UI overlap, gateway/broadcast addresses, reversed ranges,
pools over the SDK's 100-address limit, and pools too small for all clients.
The UI host's address must be updated if the AP subnet is changed.

## Windows UI computer

Join **IoT-Gateway**, then open **Settings → Network & internet → Wi-Fi →
Manage known networks → IoT-Gateway → IP assignment → Edit**. Select
**Manual**, enable **IPv4**, and enter:

| Windows field | Value |
|---|---|
| IP address | 192.168.78.10 |
| Subnet mask | 255.255.255.0 |
| Subnet prefix length, if shown instead of mask | 24 |
| Gateway | 192.168.78.1 |
| Preferred DNS | The DNS server learned by bridge from upstream DHCP |

For the current lab network, bridge reported **10.20.1.1** as its DNS server.
This is an observed network setting, not a public DNS fallback or a station
DNS proxy. Use the actual upstream DNS if the gateway is moved to another
network. Save the settings and check `ipconfig`: the Wi-Fi IPv4 address should
be 192.168.78.10. Follow
[Microsoft's Windows TCP/IP setup instructions](https://support.microsoft.com/en-gb/windows/experience/connectivity-networking/essential-network-settings-and-tasks-in-windows)
for version-specific field names. Return the Wi-Fi IP assignment to DHCP when
using this adapter on a different network if the Windows version applies the
manual settings to the adapter rather than just this saved network.

Bind the UI service to **192.168.78.10**, and allow its TCP port through the
Windows firewall only from **192.168.78.0/24**. For the repository's development
mock, the command is:

```powershell
python web/mock/server.py --host 192.168.78.10 --port 8000
```

That serves the existing mock UI/API; it does not implement a new IoT-device
integration. A scoped firewall rule for this example, run in Administrator
PowerShell, is:

```powershell
New-NetFirewallRule -DisplayName "IoT Gateway UI TCP 8000" -Direction Inbound -Action Allow -Protocol TCP -LocalAddress 192.168.78.10 -LocalPort 8000 -RemoteAddress 192.168.78.0/24 -Profile Any
```

Use the real application's port when it differs. This rule adds access for
station Wi-Fi clients; it does not change other existing firewall rules. See
[Microsoft's firewall rule reference](https://learn.microsoft.com/en-us/powershell/module/netsecurity/new-netfirewallrule?view=windowsdefender).

## Connection direction and boundaries

Clients joined to IoT-Gateway can initiate outbound IPv4 TCP/UDP connections
and ICMP echo, and matching reply traffic can return. Both NAT layers require
an outbound mapping for replies to reach downstream devices. No inbound port
maps are enabled, and both firmware projects reject builds that enable IPv6
or port mapping outside this supported configuration.

Both pre-NAT filters reject TCP SYN without ACK arriving from outside, even
if a packet's tuple happens to match an existing NAT entry. SYN+ACK and
established responses remain allowed and are then subject to normal NAT and
client TCP processing. Direct upstream routes into the private link subnet
are blocked at bridge; direct packets into the station client subnet are
blocked at station. This is outbound-initiated connectivity, not one-way data.

The Windows UI PC is **inside** the protected Wi-Fi network. Other clients
can reach it directly over the AP, and UI replies can return. This setup does
not isolate associated clients from each other at layer 2 or authenticate each
IoT application. Anyone admitted to the AP is inside that local boundary.
No router port forwarding, UPnP mapping or public tunnel is needed for the UI.

## Verification

1. Keep a phone and the UI PC joined simultaneously; station logs should show
   `connected=2/4` or a higher count if other devices are present.
2. Check the PC has .10 and the DHCP phone has .100–.199. Reconnect/reboot the
   PC and confirm .10 remains fixed. Two manual clients must not share .10.
3. From the phone, load `http://192.168.78.10:8000/` when using the development
   server above. Verify internet access from both clients separately.
4. From a device outside IoT-Gateway, attempt a new connection to the PC/client
   subnet and bridge's upstream address. There is no forwarded UI endpoint.
   Verify existing client-initiated connections still receive responses.

Portable tests cover multiple client source addresses, static/DHCP pool
validation, initial outside SYN rejection, reply allowance and direct-route
bypass rejection. They do not replace simultaneous-client, Windows firewall,
DHCP/static-IP or packet-level NAT hardware checks.

## Validation record, 2026-11-05

Both C6 gateway images and the S3 SPI diagnostic image build successfully.
Portable tests pass for the address-plan boundaries, multiple client source
addresses, blocked initial outside TCP SYN, allowed SYN+ACK/ACK replies and
direct-route bypass rejection. Both C6 images were flashed and verified.
Station startup reported four client slots, DHCP .100–.199 and fixed UI .10;
DNS and HTTP checks passed from bridge and from station through UART after
the outside-initiated TCP filters were enabled.

Two simultaneous radio clients, the Windows PC's manual address/firewall,
and an external packet-level inbound attempt still require hardware tests.
The fixed address is reserved in the plan; Windows itself has not yet been
configured or tested. No change to the development laptop's Wi-Fi was needed.
