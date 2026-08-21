#!/bin/python3
# Copied from https://github.com/LedgerHQ/app-ledger-sync/blob/develop/attestations/createKey.py and updated.
import argparse
import os
import sys
import subprocess
import logging
import coloredlogs


logger = logging.getLogger(__name__)


def setLogger(log: logging.Logger, verbose: bool) -> None:
    level = logging.DEBUG if verbose else logging.INFO
    coloredlogs.install(level=level, fmt='%(message)s', logger=log)


def check_exec(cmdline: str) -> str:
    result = subprocess.run(cmdline, shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        logger.error(f"[ERROR] Failed to execute {cmdline}")
        logger.error(f"[ERROR] stderr: {result.stderr.decode()}")
        sys.exit(result.returncode)
    return result.stdout.decode()


def check_file(filename: str, exist: bool = True) -> str:
    logger.debug(f"Checking {filename}...")
    if os.path.isfile(filename) != exist:
        msg = "already" if not exist else "does not"
        logger.error(f"[ERROR] {filename} {msg} exist")
        sys.exit(1)


# ===============================================================================
#          Main
# ===============================================================================
def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", "-v", action='store_true', help="Verbose mode")

    # Check parameters
    args = parser.parse_args()

    setLogger(logger, args.verbose)

    dir_path = "data/test"
    key_file = f"{dir_path}/priv-key.pem"

    check_file(key_file, False)

    # Create dir if not present
    os.makedirs(dir_path, exist_ok=True)

    # Generate private key if necessary
    cmd = f"openssl genpkey -algorithm ed25519 -out {key_file}"
    logger.debug(f"Creating {key_file}...")
    check_exec(cmd)
    logger.info(f"Created {key_file}")


if __name__ == "__main__":
    main()
