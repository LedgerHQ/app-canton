#  Split transaction script

## Description

This script reads PreparedTransaction or PreparedSubmissionResponse in JSON format,
splits it into its components (DAML transaction, nodes, metadata, input contracts),
serializes each component into protobuf binary format or prints the hex representation.

## Prerequisites

Run `proto/proto_gen.sh` script to generate python protobuf bindings.

## Usage

Print in hex

```bash
    python split_transaction.py --hex transaction.json

```

or write message binaries to files.

```bash
    python split_transaction.py --bin transaction.json
```
