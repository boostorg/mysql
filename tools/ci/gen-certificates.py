#!/usr/bin/env python3
# Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE.txt)
#

# Generates the ca and certificates used for CI testing.
# Usage: python gen-certificates.py [output-dir]

import os
import subprocess
import sys
import stat


def _run_openssl(*args: str) -> None:
    print(' +', *args)
    subprocess.run(['openssl', *args], check=True)


def main() -> None:
    output_dir = sys.argv[1] if len(sys.argv) > 1 else '/opt/ci-tls-mysql'
    os.makedirs(output_dir, exist_ok=True)
    os.chdir(output_dir)

    ca_key = os.path.join(output_dir, 'ca-key.pem')
    ca_crt = os.path.join(output_dir, 'ca-cert.pem')
    server_key = os.path.join(output_dir, 'server-key.pem')
    server_csr = os.path.join(output_dir, 'server.csr')
    server_crt = os.path.join(output_dir, 'server-cert.pem')

    # CA private key
    _run_openssl('genpkey', '-algorithm', 'RSA', '-out', ca_key, '-pkeyopt', 'rsa_keygen_bits:2048')

    # CA certificate
    _run_openssl(
        'req', '-x509', '-new', '-nodes', '-key', ca_key, '-sha256',
        '-days', '20000', '-out', ca_crt,
        '-subj', '/C=ES/O=Boost.MySQL CI CA/OU=IT/CN=boost-mysql-ci-ca',
    )

    # Server private key
    _run_openssl('genpkey', '-algorithm', 'RSA', '-out', server_key, '-pkeyopt', 'rsa_keygen_bits:2048')

    # Server certificate
    _run_openssl(
        'req', '-new', '-key', server_key, '-out', server_csr,
        '-subj', '/C=ES/O=Boost.MySQL CI CA/OU=IT/CN=mysql',
    )
    _run_openssl(
        'x509', '-req', '-in', server_csr, '-CA', ca_crt, '-CAkey', ca_key,
        '-CAcreateserial', '-out', server_crt, '-days', '20000', '-sha256',
    )
    os.remove(server_csr)
    os.remove(ca_key)

    # Required when running with Docker because of mismatched user IDs
    read_only = stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH  # 444
    for name in os.listdir(output_dir):
        os.chmod(os.path.join(output_dir, name), read_only)


if __name__ == '__main__':
    main()
