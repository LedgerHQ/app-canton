#!/bin/python3
# Copied from https://github.com/LedgerHQ/app-ledger-sync/blob/develop/attestations/generateCryptoData.py and updated.
import argparse
import logging
from typing import List, Tuple
from createKey import check_file, check_exec, setLogger


logger = logging.getLogger(__name__)


def get_keys_bytes(key_file: str) -> Tuple[bytes, bytes]:
    """Extract private and public key bytes from a PEM file."""
    check_file(key_file)
    stdout = check_exec(f"openssl pkey -inform pem -in {key_file} -noout -text")
    lines = [line.strip().replace(":", "") for line in stdout.splitlines()]
    private_hex = "".join(lines[2:5])
    public_hex = "".join(lines[6:9])
    return bytes.fromhex(private_hex), bytes.fromhex(public_hex)


def format_data(prefix: str, data: List[str], step: int = 16) -> None:
    """Format key material and print to stdout

    Args:
        prefix (str): Key prefix pour C/H source file declaration
        data (List[str]): Data to be formatted
        step (int): Number of bytes to display per lines
    """

    key_data = ""
    offset = 0
    while offset < len(data):
        key_data += "    " + ", ".join([f"0x{int(x, base=16):02x}" for x in data[offset:offset + step]])
        offset += step
        if offset < len(data):
            key_data += ",\n"

    key = prefix + " {\n" + key_data + "};"
    print(key)


# ===============================================================================
#          Main
# ===============================================================================
def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", "-v", action='store_true', help="Verbose mode")
    parser.add_argument('env', type=str, help='CA, key and cert env', choices=["prod", "test"])

    # Check parameters
    args = parser.parse_args()

    env = args.env.upper()

    setLogger(logger, args.verbose)

    dir_path  = f"data/{args.env}"
    key_file  = f"{dir_path}/priv-key.pem"

    check_file(key_file)

    # Extract KEY parameters
    logger.debug(f"Extracting {key_file} parameters...")
    cmd = f"openssl pkey -inform pem -in {key_file} -noout -text"
    check_exec(cmd)

    # Get key bytes
    private_key, public_key = get_keys_bytes(key_file)

    # Generate ATTESTATION_KEY
    prefix = f"static const uint8_t {env}_ATTESTATION_KEY[] ="
    key_hex = private_key.hex()
    if args.env == "prod":
        print(f"PROD_ATTESTATION_KEY='0x{',0x'.join(key_hex)}'")
    else:
        format_data(prefix, key_hex)

    # Generate ATTESTATION_PUBKEY
    prefix = f"static const uint8_t {env}_ATTESTATION_PUBKEY[] ="
    key_hex = public_key.hex()
    format_data(prefix, key_hex)

if __name__ == "__main__":
    main()
