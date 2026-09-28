#!/usr/bin/env python3

"""
Rebuild a test fixture JSON from a Ledger Live APDU log.

The script reads the SIGN_TX APDUs (CLA=e0, INS=06) of a log, reassembles the
protobuf messages sent to the device, and writes a file with the same shape as
the files in tests/tx_examples/. It then splits the fixture again with
scripts/split_tx_util.py and compares the bytes with the log, so the result is
a proven capture and not a guess.

The script needs two packages that are not part of tests/requirements.txt:
    pip install rich rich-argparse

Examples:
    python3 scripts/ledger_wallet_log_to_json.py USDCxAcceptReceive.log
    python3 scripts/ledger_wallet_log_to_json.py apdu_fail.txt -o /tmp/fixture.json --hash <64 hex chars>
"""

import argparse
import copy
import json
import re
import sys
from pathlib import Path
from typing import Any, Optional

sys.path.append(f"{Path(__file__).parent.parent.resolve()}/proto")
sys.path.append(f"{Path(__file__).parent.resolve()}")

# pylint: disable=no-name-in-module, import-error, wrong-import-position
from google.protobuf.json_format import MessageToDict  # type: ignore
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich_argparse import RichHelpFormatter

from com.daml.ledger.api.v2.interactive.device_pb2 import (  # type: ignore
    DeviceDamlTransaction,
    DeviceMetadata,
)
from split_tx_util import split_transaction  # type: ignore

CLA_SIGN = 0xE0
INS_SIGN_TX = 0x06
P1_PREPARED_TX = 0x02  # P1 also carries hash and onboarding signatures, which are not transactions
P2_FIRST = 0x01
P2_MSG_END = 0x04

APDU_RE = re.compile(r'"apdu"\s*:\s*"([0-9a-fA-F]+)"')

# The test client reads the hash before it talks to the device, and refuses a fixture without one.
PLACEHOLDER_HASH = "00" * 32

console = Console()


def read_apdus(logfile: Path) -> list[bytes]:
    """Return every APDU of the log as raw bytes."""
    text = logfile.read_text(encoding="utf-8", errors="replace")
    return [bytes.fromhex(hex_str) for hex_str in APDU_RE.findall(text)]


def reassemble_sessions(apdus: list[bytes]) -> list[list[bytes]]:
    """Join the SIGN_TX chunks into one list of protobuf messages per signature."""
    sessions: list[list[bytes]] = []
    messages: list[bytes] = []
    current = bytearray()

    for apdu in apdus:
        if len(apdu) < 5 or apdu[0] != CLA_SIGN or apdu[1] != INS_SIGN_TX:
            continue
        p1, p2, lc = apdu[2], apdu[3], apdu[4]
        if p1 != P1_PREPARED_TX:
            continue
        if p2 & P2_FIRST:
            # The first chunk carries the BIP32 path and starts a new signature.
            if messages:
                sessions.append(messages)
            messages, current = [], bytearray()
            continue
        current += apdu[5:5 + lc]
        if p2 & P2_MSG_END:
            messages.append(bytes(current))
            current.clear()

    if messages:
        sessions.append(messages)
    return sessions


def decode_messages(messages: list[bytes]) -> tuple[dict, list[dict], dict, list[dict]]:
    """Decode the message sequence into transaction, nodes, metadata and contracts."""
    transaction = DeviceDamlTransaction()
    transaction.ParseFromString(messages[0])
    nodes_count = transaction.nodes_count

    nodes = []
    for raw in messages[1:1 + nodes_count]:
        node = DeviceDamlTransaction.Node()
        node.ParseFromString(raw)
        nodes.append(node)

    rest = messages[1 + nodes_count:]
    metadata = DeviceMetadata()
    metadata.ParseFromString(rest[0])

    contracts = []
    for raw in rest[1:1 + metadata.input_contracts_count]:
        contract = DeviceMetadata.InputContract()
        contract.ParseFromString(raw)
        contracts.append(contract)

    return (
        MessageToDict(transaction),
        [MessageToDict(node) for node in nodes],
        MessageToDict(metadata),
        [MessageToDict(contract) for contract in contracts],
    )


def build_fixture(messages: list[bytes], tx_hash: str) -> dict[str, Any]:
    """Assemble the fixture dictionary from the decoded messages."""
    tx_dict, node_dicts, metadata_dict, contract_dicts = decode_messages(messages)

    tx_dict.pop("nodesCount", None)
    metadata_dict.pop("inputContractsCount", None)

    # The wire order is reverse node id; the fixture stores the nodes in node id order.
    tx_dict["nodes"] = sorted(node_dicts, key=lambda node: int(node.get("nodeId", 0)))
    metadata_dict["inputContracts"] = contract_dicts

    return {"json": {"transaction": tx_dict, "metadata": metadata_dict}, "hash": tx_hash}


def verify_round_trip(fixture: dict[str, Any], messages: list[bytes]) -> Optional[int]:
    """Re-split the fixture and return the index of the first part that differs."""
    tx_data, nodes_pb, metadata_data, contracts_pb = split_transaction(
        json.dumps(copy.deepcopy(fixture))
    )
    rebuilt = [tx_data, *nodes_pb, metadata_data, *contracts_pb]

    if len(rebuilt) != len(messages):
        return min(len(rebuilt), len(messages))
    for index, (left, right) in enumerate(zip(rebuilt, messages)):
        if left != right:
            return index
    return None


def node_summary(node: dict[str, Any]) -> tuple[str, str, str]:
    """Return the kind, the module:entity and the choice of one node."""
    body = node.get("v1", {})
    kind = next(iter(body), "?")
    inner = body.get(kind, {})
    return kind, template_name(inner), inner.get("choiceId", "")


def template_name(body: dict[str, Any]) -> str:
    """Return the module:entity name of a node or contract body."""
    template = body.get("templateId", {})
    return f"{template.get('moduleName', '?')}:{template.get('entityName', '?')}"


def nodes_table(nodes: list[dict[str, Any]]) -> Table:
    """Build the summary table of the decoded nodes."""
    table = Table(title="Nodes")
    for column in ("id", "kind", "module:entity", "choice"):
        table.add_column(column)
    for node in nodes:
        kind, name, choice = node_summary(node)
        table.add_row(node.get("nodeId", "0"), kind, name, choice)
    return table


def contracts_table(contracts: list[dict[str, Any]]) -> Table:
    """Build the summary table of the input contracts."""
    table = Table(title="Input contracts")
    for column in ("#", "module:entity", "contract id"):
        table.add_column(column)
    for index, contract in enumerate(contracts):
        create = contract.get("v1", {})
        table.add_row(str(index), template_name(create),
                      create.get("contractId", "")[:24] + "...")
    return table


def session_paths(base: Path, count: int) -> list[Path]:
    """Name one output file per signature, numbering them only when there are several."""
    if count == 1:
        return [base]
    return [base.with_name(f"{base.stem}_{i}{base.suffix}") for i in range(1, count + 1)]


def print_report(fixture: dict[str, Any], bad_index: Optional[int], output: Path) -> None:
    """Print the decoded content and the round-trip result."""
    transaction = fixture["json"]["transaction"]
    metadata = fixture["json"]["metadata"]

    console.print(Panel(
        f"Transaction version {transaction.get('version', '?')} - "
        f"{len(transaction['nodes'])} nodes, "
        f"{len(metadata['inputContracts'])} input contracts\n"
        f"Fixture written to {output}",
        title=output.name,
    ))
    console.print(nodes_table(transaction["nodes"]))
    console.print(contracts_table(metadata["inputContracts"]))

    if bad_index is None:
        console.print("[bold green]PASS[/] round-trip: every part is byte-identical")
    else:
        console.print(f"[bold red]FAIL[/] round-trip: part {bad_index} differs")


def print_hash_note() -> None:
    """Explain how to obtain the transaction hash."""
    console.print(Panel(
        f"No --hash given, the fixture holds the placeholder {PLACEHOLDER_HASH[:8]}...\n"
        "Build with DEBUG=1, run the test, and read the line the app prints:\n"
        "  Transaction Hash: <64 hex chars>   (src/ui/nbgl_display_transaction.c)\n"
        "Copy it into the hash field, or pass --hash for a single-transaction log.",
        title="hash missing",
        style="yellow",
    ))


def main() -> int:
    """Read the log, write the fixture and verify it."""
    parser = argparse.ArgumentParser(
        description="Turn a Ledger Live APDU log into a tests/tx_examples fixture JSON.",
        epilog="The script fails when the fixture does not split back into the logged bytes.",
        formatter_class=RichHelpFormatter,
    )
    parser.add_argument("logfile", type=Path, help="APDU log file to read")
    parser.add_argument("-o", "--output", type=Path,
                        help="fixture file to write (default: <logfile>.json)")
    parser.add_argument("--hash", dest="tx_hash", default="",
                        help="64 hex chars of the expected transaction hash. Refused when the log "
                             "holds several transactions, since it would fit only one of them.")
    args = parser.parse_args()

    base = args.output or args.logfile.with_suffix(".json")

    sessions = reassemble_sessions(read_apdus(args.logfile))
    if not sessions:
        console.print("[bold red]No prepared transaction found in the log[/]")
        return 1
    if len(sessions) > 1:
        console.print(f"[yellow]The log holds {len(sessions)} prepared transactions, "
                      f"written to one file each.[/]")
        if args.tx_hash:
            console.print("[bold red]--hash needs a log with a single transaction[/]")
            return 1

    failed = False
    for messages, output in zip(sessions, session_paths(base, len(sessions))):
        fixture = build_fixture(messages, args.tx_hash or PLACEHOLDER_HASH)
        output.write_text(json.dumps(fixture, indent=2) + "\n", encoding="utf-8")

        bad_index = verify_round_trip(fixture, messages)
        print_report(fixture, bad_index, output)
        failed = failed or bad_index is not None

    if not args.tx_hash:
        print_hash_note()

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
