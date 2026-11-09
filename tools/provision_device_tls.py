#!/usr/bin/env python3
"""Provision a private CA and one PC/target identity pair using OpenSSL."""
import argparse
import ipaddress
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IDENTITY = ROOT / "local-backups" / "device-identity"
FILES = ("ca.pem", "ca.key", "server.pem", "server.key", "device.pem", "device.key")


def provision(directory, server_ip="192.168.78.10"):
    address = str(ipaddress.IPv4Address(server_ip))
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    directory.chmod(0o700)
    if any((directory / name).exists() for name in FILES):
        raise FileExistsError("Identity files already exist; use a new private directory to rotate")

    def openssl(*args):
        subprocess.run(["openssl", *args], cwd=directory, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    previous_mask = os.umask(0o077)
    try:
        openssl("req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
                "-nodes", "-keyout", "ca.key", "-out", "ca.pem", "-days", "3650",
                "-subj", "/CN=IoT private device CA", "-addext", "basicConstraints=critical,CA:TRUE",
                "-addext", "keyUsage=critical,keyCertSign,cRLSign")
        for name, usage, subject in (("server", "serverAuth", "IoT UI PC"),
                                      ("device", "clientAuth", "IoT target")):
            openssl("req", "-new", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
                    "-nodes", "-keyout", name + ".key", "-out", name + ".csr",
                    "-subj", "/CN=" + subject)
            extensions = "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n"
            extensions += "extendedKeyUsage=" + usage + "\n"
            if name == "server":
                extensions += "subjectAltName=IP:" + address + "\n"
            (directory / (name + ".ext")).write_text(extensions)
            openssl("x509", "-req", "-in", name + ".csr", "-CA", "ca.pem", "-CAkey", "ca.key",
                    "-CAcreateserial", "-out", name + ".pem", "-days", "3650", "-sha256",
                    "-extfile", name + ".ext")
        for path in directory.iterdir():
            if path.is_file():
                path.chmod(0o600)
    finally:
        os.umask(previous_mask)
    return directory


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=DEFAULT_IDENTITY)
    parser.add_argument("--server-ip", default="192.168.78.10")
    args = parser.parse_args()
    provision(args.directory, args.server_ip)
    print("Private PC/target identities provisioned. Do not commit this directory or firmware.")
