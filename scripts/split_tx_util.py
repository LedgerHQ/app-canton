#!/usr/bin/env python3

"""
This script reads PreparedTransaction or PreparedSubmissionResponse in JSON format,
splits it into its components (DAML transaction, nodes, metadata, input contracts),
serializes each component into protobuf binary format or prints the hex representation.
"""

import json
from pathlib import Path

# pylint: disable=no-name-in-module, import-error
from google.protobuf.json_format import Parse # type: ignore

if __name__ == "__main__":
    import sys
    from pathlib import Path
    sys.path.append(f"{Path(__file__).parent.parent.resolve()}/proto")

# pylint: disable=no-name-in-module, import-error, wrong-import-position
from com.daml.ledger.api.v2.interactive.device_pb2 import (
    DeviceDamlTransaction,
    DeviceMetadata,
)  # type: ignore

def load_json(input_str: str):
    """Load JSON from either a JSON string or a file path."""

    # Try to parse as JSON first (safe for long strings)
    try:
        return json.loads(input_str)
    except json.JSONDecodeError:
        pass  # if not JSON string, then maybe file path

    # Otherwise try as file path, but safely
    try:
        p = Path(input_str)
        if p.is_file():
            with p.open("r", encoding="utf-8") as f:
                return json.load(f)
    except OSError:
        pass  # Path too long, illegal chars, etc.

    raise ValueError(
        "Input is neither valid JSON content nor a readable JSON file path."
    )

def split_transaction(json_file: str) -> tuple[bytes, list[bytes], bytes, list[bytes]]:
    """Read JSON transaction file and serialize into parts."""

    json_tx = load_json(json_file)

    # Determine if the JSON is a full PrepareSubmissionResponse or just a PreparedTransaction
    prepared_transaction = json_tx.get("prepared_transaction") or json_tx.get("preparedTransaction") or json_tx.get("json")
    if not prepared_transaction:
        prepared_transaction = json_tx

    daml_tx_data, nodes_pb = _process_daml_transaction(prepared_transaction["transaction"])
    metadata_data, input_contracts_pb = _process_metadata(prepared_transaction["metadata"])

    return (
        daml_tx_data,
        nodes_pb,
        metadata_data,
        input_contracts_pb
    )

def _process_daml_transaction(daml_tx: dict) -> tuple[bytes, list[bytes]]:
    """Process DAML transaction and its nodes."""

    nodes = daml_tx.pop("nodes", [])
    daml_tx["nodes_count"] = len(nodes)

    daml_tx_pb = DeviceDamlTransaction()
    Parse(json.dumps(daml_tx), daml_tx_pb)

    nodes_pb = [None] * len(nodes)

    # We have to send nodes in reverse order for every node tree.
    # ATM we have only one tree, so we can just reverse the list.
    for node in nodes:
        node_id = int(node.get('nodeId', node.get('node_id')))
        node_pb = DeviceDamlTransaction.Node()
        Parse(json.dumps(node), node_pb)
        pos = len(nodes) - 1 - node_id
        nodes_pb[pos] = node_pb.SerializeToString()

    return daml_tx_pb.SerializeToString(), nodes_pb

def _process_metadata(metadata: dict) -> tuple[bytes, list[bytes]]:
    """Process metadata and input contracts."""

    input_contracts = metadata.pop("inputContracts", [])
    metadata["input_contracts_count"] = len(input_contracts)

    metadata_pb = DeviceMetadata()
    Parse(json.dumps(metadata), metadata_pb)

    input_contracts_pb = list()
    for contract in input_contracts:
        # Remove eventBlob field if exists, they are not used in hash computation
        # and can be trimmed to decrease msg size
        contract.pop("eventBlob", None)
        contract_pb = DeviceMetadata.InputContract()
        Parse(json.dumps(contract), contract_pb)
        input_contracts_pb.append(contract_pb.SerializeToString())

    return metadata_pb.SerializeToString(), input_contracts_pb


if __name__ == "__main__":
    import argparse

    def main():
        parser = argparse.ArgumentParser(
            description="""
            This script reads PreparedTransaction or PreparedSubmissionResponse in JSON format,
            splits it into its components (DAML transaction, nodes, metadata, input contracts),
            serializes each component into protobuf binary format or prints the hex representation.
            """,
            epilog="""
            Examples:
              python split_transaction.py --hex transaction.json
              python split_transaction.py --bin transaction.json
            """,
        )

        parser.add_argument(
            "--hex",
            "-x",
            help="Expect the input is PrepareSubmissionResponse or PreparedTransaction in JSON format",
            action="store_true",
        )
        parser.add_argument(
            "--bin",
            "-b",
            help="Expect the input is PrepareSubmissionResponse or PreparedTransaction in JSON format",
            action="store_true",
        )
        parser.add_argument("json_file")
        json_file = parser.parse_args().json_file

        try:
            daml_tx_data, nodes_pb, metadata_data, input_contracts_pb = split_transaction(json_file)

            if parser.parse_args().hex:
                print(f"\nDAML Transaction (size {len(daml_tx_data)} bytes):")
                print(f"{daml_tx_data.hex()}\n")
                for i, node in enumerate(nodes_pb):
                    print(f"Node #{i} (size {len(node)} bytes):")
                    print(f"{node.hex()}\n")

                print(f"Metadata (size {len(metadata_data)} bytes):")
                print(f"{metadata_data.hex()}\n")
                for i, contract in enumerate(input_contracts_pb):
                    print(f"Input Contract #{i} (size {len(contract)} bytes):")
                    print(f"{contract.hex()}\n")

            elif parser.parse_args().bin:
                base_name = Path(json_file).stem

                # Write every component into separate binary files
                with open(f"{base_name}_daml_tx.bin", "wb") as f:
                    f.write(daml_tx_data)
                    print(f"Created {base_name}_daml_tx.bin")
                for i, node in enumerate(nodes_pb):
                    with open(f"{base_name}_node_{i}.bin", "wb") as f:
                        f.write(node)
                        print(f"Created {base_name}_node_{i}.bin")
                with open(f"{base_name}_metadata.bin", "wb") as f:
                    f.write(metadata_data)
                    print(f"Created {base_name}_metadata.bin")
                for i, contract in enumerate(input_contracts_pb):
                    with open(f"{base_name}_input_contract_{i}.bin", "wb") as f:
                        f.write(contract)
                        print(f"Created {base_name}_input_contract_{i}.bin")

            else:
                print("Please specify either --hex or --bin option")
                parser.print_help()
                sys.exit(1)

        except (FileNotFoundError, json.JSONDecodeError, KeyError) as e:
            print(f"Error processing transaction: {e}")
            sys.exit(1)

    main()
