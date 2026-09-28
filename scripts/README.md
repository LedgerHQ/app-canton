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

# Ledger wallet log to JSON script

## Description

This script reads an APDU log exported from the Ledger Live desktop app. It keeps the SIGN_TX
commands, joins their chunks into the protobuf messages sent to the device, and writes a fixture
JSON with the same shape as the files in `tests/tx_examples/`.

The script then splits the fixture again with `split_tx_util.py` and compares the result with the
bytes from the log. It reports PASS only if every part is identical. It exits with 1 if a part is
different, or if the log contains no prepared transaction.

## Prerequisites

Run the `proto/proto_gen.sh` script to generate the python protobuf bindings.

Install the two packages that are not in `tests/requirements.txt`:

```bash
    pip install rich rich-argparse
```

## Usage

Write the fixture next to the log.

```bash
    python3 scripts/ledger_wallet_log_to_json.py USDCxAcceptReceive.log
```

Choose the output file, and give the transaction hash.

```bash
    python3 scripts/ledger_wallet_log_to_json.py apdu_fail.txt \
        -o tests/tx_examples/token_transfer_cbtc_send.json --hash <64 hex chars>
```

## Transaction hash

The log does not contain the transaction hash. If you do not give `--hash`, the script writes 32
zero bytes in the `hash` field. The test client refuses a fixture with no hash at all, so the
placeholder lets you run the test and read the real value.

To get the real value, build with `DEBUG=1` and run the test. The app prints this line, from
`src/ui/nbgl_display_transaction.c`:

```
    Transaction Hash: <64 hex chars>
```

Copy the value into the `hash` field of the fixture.

## Logs with more than one transaction

A log can contain more than one signature. The script keeps only the prepared transactions, and
writes one file for each. It adds a number to the name:

```
    out_1.json  out_2.json  out_3.json
```

A log with one transaction keeps the plain name. The `--hash` option is refused for a log with
more than one transaction, because one hash cannot apply to all of them.
