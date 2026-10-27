#!/usr/bin/env python3
"""Small lab TCP/UDP echo server for the SPI host, bounded to eight TCP clients."""
import argparse
import socket
import threading

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--bind", required=True, help="IPv4 of your lab PC on upstream Wi-Fi")
parser.add_argument("--port", type=int, default=9000)
args = parser.parse_args()
slots = threading.BoundedSemaphore(8)

def echo_tcp(connection):
    try:
        with connection:
            connection.settimeout(10)
            while data := connection.recv(4096):
                connection.sendall(data)
    except (OSError, TimeoutError):
        pass
    finally:
        slots.release()

def tcp_loop(server):
    with server:
        while True:
            connection, _ = server.accept()
            if slots.acquire(blocking=False):
                threading.Thread(target=echo_tcp, args=(connection,), daemon=True).start()
            else:
                connection.close()

tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
tcp_server.bind((args.bind, args.port))
tcp_server.listen(8)
threading.Thread(target=tcp_loop, args=(tcp_server,), daemon=True).start()
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
    server.bind((args.bind, args.port))
    print(f"Lab echo server TCP/UDP {args.bind}:{args.port}", flush=True)
    while True:
        data, address = server.recvfrom(4096)
        server.sendto(data, address)
