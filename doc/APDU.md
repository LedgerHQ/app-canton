# Application Protocol Data Unit (APDU)

The communication protocol used by the Ledger device OS (BOLOS) to exchange [APDU](https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit)
is very close to [ISO 7816-4](https://www.iso.org/standard/77180.html) with a few differences:

- `Lc` length is always exactly 1 byte
- No `Le` field in APDU command
- Maximum size of APDU command is 260 bytes: 5 bytes of header + 255 bytes of data
- Maximum size of APDU response is 260 bytes: 258 bytes of response data + 2 bytes of status word

Status words tend to be similar to common [APDU responses](https://www.eftlab.com/knowledge-base/complete-list-of-apdu-responses/)
in the industry.

## Command APDU

| Field name | Length (bytes) | Description |
| --- | --- | --- |
| CLA | 1 | Instruction class - indicates the type of command (0xE0) |
| INS | 1 | Instruction code - indicates the specific command |
| P1 | 1 | Instruction parameter 1 for the command |
| P2 | 1 | Instruction parameter 2 for the command |
| Lc | 1 | The number of bytes of command data to follow (0-255) |
| Data | var | Command data with `Lc` bytes |

## Response APDU

| Field name | Length (bytes) | Description |
| --- | --- | --- |
| Response | var | Response data (can be empty) |
| SW | 2 | Status word (e.g. `0x9000` for success) |

## Instruction Set (INS)

| INS code | Name | Description |
| --- | --- | --- |
| 0x03 | `GET_VERSION` | Get the application version |
| 0x04 | `GET_APP_NAME` | Get the application name |
| 0x05 | `GET_PUBLIC_KEY` | Get the public key for a given BIP32 path |
| 0x06 | `SIGN_TX` | Sign a Canton transaction |

### GET_VERSION (0x03)

- P1: 0x00 `P1_NONE`
- P2: 0x00 `P2_NONE`
- Data: None
- Response format:
  - MAJOR (1 byte)
  - MINOR (1 byte)
  - PATCH (1 byte)

**Example:**

```shell
-> E0 03 00 00 00
<= 01 02 03 9000  // Version 1.2.3
```

### GET_APP_NAME (0x04)

- P1: 0x00 `P1_NONE`
- P2: 0x00 `P2_NONE`
- Data: None
- Response: ASCII string of the application name

**Example:**

```shell
-> E0 04 00 00 00
<= 43616E746F6E 9000  // "Canton"
```

### GET_PUBLIC_KEY (0x05)

- P1:
  - 0x00 `P1_NONE`: Return public key without confirmation
  - 0x01 `P1_CONFIRM`: Display and confirm party ID
- P2: 0x00 `P2_NONE`
- Data: BIP32 derivation path (encoded using `pack_derivation_path`)
- Response format:
  - pub_key_len (1 byte, value: 32)
  - pub_key (32 bytes)
  - chain_code_len (1 byte, value: 32)
  - chain_code (32 bytes)

**Example:**

```shell
-> E0 05 00 00 15 058000002C80001A6F800000008000000080000000  // BIP32 path m/44'/6767'/0'/0'/0'
<= 20 C59F7F29374D24506DD6490A5DB472CF00958E195E146F3DC9C97F96D5C51097 20 FDE466B4A3E0CEAD6A4FB0ABA90422F54B5F960D51C0DBAEA8DB2A03E0A1098A 9000
```

### SIGN_TX (0x06)

#### Sign Modes (P1)

- 0x00 `P1_SIGN_HASH`: Sign a raw hash. 32 bytes long for prepared transaction hash, 34 bytes long for untyped versioned message hash.
- 0x01 `P1_SIGN_UNTYPED_VERSIONED_MESSAGE`: Sign untyped versioned message(s) transaction(s). For onboarding.
- 0x02 `P1_SIGN_PREPARED_TRANSACTION`: Sign a prepared transaction. For coin/token transfer etc.

#### Message Flags (P2)

P2 flags can be combined using bitwise OR:

- 0x00 `P2_NONE`: No flags
- 0x01 `P2_FIRST`: First message chunk (contains the BIP32 path)
- 0x02 `P2_MORE`: More chunks to come
- 0x04 `P2_MSG_END`: Last chunk of current message

#### SIGN_HASH P1 = 0x00 Example

Signs a raw hash directly (here a 32 bytes long hash)

```shell
-> E0 06 00 03 15 058000002C80001A6F800000008000000080000000  // BIP32 path (P2_FIRST | P2_MORE)
<= 9000
-> E0 06 00 04 20 1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF  // Hash (P2_MSG_END)
<= [der_sig_len] [DER signature] [v] 9000
```

#### SIGN_UNTYPED_VERSIONED_MESSAGE (P1 = 0x01) Example

Signs topology transactions using `PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE` (11) for individual hashes and `PURPOSE_MULTI_TOPOLOGY_TRANSACTION_SIGNATURE` (55) for the final multi-hash.

For attested party onboarding, an optional 24 bytes random challenge (16 bytes nonce + 8 bytes timestamp) can be provided
as part of the first APDU, following the BIP32 path.

**Example APDU sequence:**

```shell
// Send BIP32 path first
-> E0 06 01 03 15 058000002C80001A6F800000008000000080000000 18 F91C61B67F2A948A723A750DC89B407D2C54C7C27D3AC23F // BIP32 path + challenge (P2_FIRST | P2_MORE)
<= 9000

// Send namespace delegation transaction (130 bytes)
-> E0 06 01 06 82 0A7E080110011A780A760A443132323062366665623630366665373264653936303634613333323232613962356335386361363135323662356161623761613231366537646236636437363336343037122C10031A20FF2D9C046D1FDF68E65ECD36278EB8EAD12AA38EF5B323769BAA5DF9216F85D820012A02010430011801101E
<= 9000

// Send party-to-key mapping transaction (137 bytes)
-> E0 06 01 06 89 0A8401080110021A7E82017B0A49626F623A3A31323230623666656236303666653732646539363036346133333232326139623563353863613631353236623561616237616132313665376462366364373633363430371801222C10031A20FF2D9C046D1FDF68E65ECD36278EB8EAD12AA38EF5B323769BAA5DF9216F85D820012A0201043001101E
<= 9000

// Send party-to-participant mapping transaction (179 bytes, final message)
-> E0 06 01 04 B3 0AAE01080110011AA7014AA4010A49626F623A3A313232306236666562363036666537326465393630363461333332323261396235633538636136313532366235616162376161323136653764623663643736333634303710011A550A517061727469636970616E743A3A31323230313235366361636138616135343236343436643262633461313632393163343862373537326238616262306431623365656136336364323939363732316362321002101E
// Response containing [DER signature length (1 byte)][DER signature (64)][v (1 byte)][challenge signature len (1 byte)][challenge signature (64)]
<= 40 50739f146e51c76cd98a1cc0a2e5d18cf40417217660423bf9b42f9060eb580eaf07415a3ca88169eadca1eb9a69a65a4180abbfdea198fb043fcb66bc5bd903 00 40 09f8d654f68f42a3aff275e57afb0f42bbbd145b6cc3a5fbfdf53462a32fe4b5f57606fc974d063662a1c1d769491bb71ecb13c6a4d152ff996ec7d64137870c 9000
```

**P2 Flag Usage:**

- 0x03 `P2_FIRST | P2_MORE`: BIP32 path - first APDU, transactions following.
- 0x02 `P2_MORE`: More chunks to come for current transaction (if transaction split in several APDUs)
- 0x06 `P2_MORE | P2_MSG_END`: Transaction complete, more transactions expected
- 0x04 `P2_MSG_END`: Final transaction, end of sequence

**Processing Algorithm:**

1. Derivation path and optional 24 bytes attestation challenge is received in the first APDU.
2. Each fully received transaction is hashed using PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE (purpose 11)
3. Individual 34-byte hashes are collected and sorted lexicographically in hex format
4. Once all transaction messages are received, hashes are sorted lexicographically in hex format.
5. Sorted hashes are concatenated with length prefixes:
   - 4-byte count of hashes
   - For each hash: 4-byte length (34) + 34-byte hash data
6. Final 34 bytes hash computed using PURPOSE_MULTI_TOPOLOGY_TRANSACTION_SIGNATURE (purpose 55). If the challenge was provided in the first APDU, it is signed with the attestation key (signing data = multihash + challenge).
7. Returns 64-byte Ed25519 signature and challenge signature if challenge was provided.

#### SIGN_PREPARED_TRANSACTION (P1 = 0x02) Example

Signs a structured Canton transaction with multiple components sent in sequence.

As the components, this command uses parts of `PreparedTransaction` [protobuf message](https://github.com/digital-asset/canton/blob/main/community/ledger-api/src/main/protobuf/com/daml/ledger/api/v2/interactive/interactive_submission_service.proto),
split in the way described in [SPLIT_TRANSACTION.md](./SPLIT_TRANSACTION.md).

```shell
-> E0 06 02 03 15 058000002C80001A6F800000008000000080000000  // BIP32 path (P2_FIRST | P2_MORE)
<= 9000

// DAML Transaction
-> E0 06 02 02 FF [255 bytes DAML transaction]                   // DAML transaction chunk (P2_MORE)
-> E0 06 02 06 80 [128 bytes DAML transaction]                   // Final DAML chunk (P2_MORE | P2_MSG_END)
<= 9000

// Transaction Nodes (multiple nodes, each ending with P2_MSG_END)
-> E0 06 02 02 FF [255 bytes node 0 data]                       // Node 0 chunk (P2_MORE)
-> E0 06 02 06 80 [128 bytes node 0 data]                       // Final node 0 chunk (P2_MORE | P2_MSG_END)
<= 9000

// Metadata
-> E0 06 02 02 FF [255 bytes metadata]                          // Metadata chunk (P2_MORE)
-> E0 06 02 06 80 [128 bytes metadata]                          // Final metadata chunk (P2_MORE | P2_MSG_END)
<= 9000

// Input Contracts (multiple contracts, each ending with P2_MSG_END)
-> E0 06 02 02 FF [255 bytes contract 0 data]                   // Contract 0 chunk (P2_MORE)
-> E0 06 02 06 80 [128 bytes contract 0 data]                   // Final contract 0 chunk (P2_MSG_END)
<= [der_sig_len] [DER signature] [v] 9000
```

**Component sequence for prepared transactions:**

1. BIP32 derivation path
2. DAML transaction data
3. Transaction nodes (one or more)
4. Metadata
5. Input contracts (zero or more)

For more details on the transmission sequence of components, see [#Sending order](./SPLIT_TRANSACTION.md#sending-order)

Each component (**except the path**) ends with `P2_MSG_END` to signal completion of that data type.

#### Response Format

- sig_len (1 byte): 0x40 (64 bytes)
- signature (64 bytes): Raw Ed25519 signature
- v (1 byte): Recovery value (0x00)
- SW (2 bytes): Status word

## Status Words

| SW | Description |
| --- | --- |
| 0x9000 | Success |
| 0x6985 | Denied by user (SW_DENY) |
| 0x6A86 | Wrong P1/P2 (SW_WRONG_P1P2) |
| 0x6A87 | Wrong data length (SW_WRONG_DATA_LENGTH) |
| 0x6D00 | Invalid instruction (SW_INS_NOT_SUPPORTED) |
| 0x6E00 | Invalid CLA (SW_CLA_NOT_SUPPORTED) |
| 0xB000 | Wrong response length (SW_WRONG_RESPONSE_LENGTH) |
| 0xB001 | Display BIP32 path failed (SW_DISPLAY_BIP32_PATH_FAIL) |
| 0xB002 | Display address failed (SW_DISPLAY_ADDRESS_FAIL) |
| 0xB003 | Display amount failed (SW_DISPLAY_AMOUNT_FAIL) |
| 0xB004 | Wrong transaction length (SW_WRONG_TX_LENGTH) |
| 0xB005 | Transaction parsing failed (SW_TX_PARSING_FAIL) |
| 0xB006 | Transaction hash failed (SW_TX_HASH_FAIL) |
| 0xB007 | Bad state (SW_BAD_STATE) |
| 0xB008 | Signature failed (SW_SIGNATURE_FAIL) |
