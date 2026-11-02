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
| 0x05 | `GET_PUBLIC_KEY` | Get the Ed25519 public key for a derivation path |
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

The app derives the key with SLIP-10 on the Ed25519 curve.
The OS allows only paths that start with `44'/6767'`.

- P1:
  - 0x00 `P1_NONE`: Return public key without confirmation
  - 0x01 `P1_CONFIRM`: Show the party ID ("Verify Canton address") and return the public key
    only if the user approves it
- P2: 0x00 `P2_NONE`. With `P1_CONFIRM`, the app also accepts P2 values 0x01 to 0x03
  and ignores them.
- Data: derivation path
  - path_len (1 byte): number of path elements
  - path elements (4 bytes each, big-endian, hardened elements have bit 31 set)
- Response format:
  - pub_key_len (1 byte, value: 32)
  - pub_key (32 bytes): raw Ed25519 public key
  - chain_code_len (1 byte, value: 32)
  - chain_code (32 bytes)

See [TRANSACTION.md](./TRANSACTION.md#party-id) for the party ID format.

**Example:**

```shell
-> E0 05 00 00 15 058000002C80001A6F800000008000000080000000  // Path m/44'/6767'/0'/0'/0'
<= 20 C59F7F29374D24506DD6490A5DB472CF00958E195E146F3DC9C97F96D5C51097 20 FDE466B4A3E0CEAD6A4FB0ABA90422F54B5F960D51C0DBAEA8DB2A03E0A1098A 9000
```

### SIGN_TX (0x06)

#### Sign Modes (P1)

- 0x00 `P1_SIGN_HASH`: Sign a raw hash. 32 bytes long for prepared transaction hash, 34 bytes long for untyped versioned message hash.
  The app shows only the hash, so this mode needs the "Blind signing" setting.
- 0x01 `P1_SIGN_UNTYPED_VERSIONED_MESSAGE`: Sign untyped versioned message(s) transaction(s). For onboarding.
- 0x02 `P1_SIGN_PREPARED_TRANSACTION`: Sign a prepared transaction. For coin/token transfer etc.

All APDUs of one signing flow must use the same P1. Otherwise the app returns `SW_BAD_STATE`.

#### Message Flags (P2)

P2 flags can be combined using bitwise OR:

- 0x01 `P2_FIRST`: First APDU (contains the derivation path). It starts a new signing flow.
- 0x02 `P2_MORE`: More data to come
- 0x04 `P2_MSG_END`: Last chunk of current message

The app refuses other bits, a P2 without `P2_MORE` or `P2_MSG_END`, and `P2_FIRST` without
`P2_MORE` (`SW_WRONG_P1P2`).
The first APDU contains the derivation path, encoded as for `GET_PUBLIC_KEY`.
The app appends the data of the next APDUs to the current message, up to 12 KiB per message.
A message that ends with `P2_MORE | P2_MSG_END` is complete, and more messages follow.
A message that ends with `P2_MSG_END` alone is the last one.

#### SIGN_HASH P1 = 0x00 Example

Signs a raw hash directly (here a 32 bytes long hash)

```shell
-> E0 06 00 03 15 058000002C80001A6F800000008000000080000000  // Derivation path (P2_FIRST | P2_MORE)
<= 9000
-> E0 06 00 04 20 1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF  // Hash (P2_MSG_END)
<= [sig_len] [signature] [v] 9000
```

#### SIGN_UNTYPED_VERSIONED_MESSAGE (P1 = 0x01) Example

Signs topology transactions using `PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE` (11) for individual hashes and `PURPOSE_MULTI_TOPOLOGY_TRANSACTION_SIGNATURE` (55) for the final multi-hash.

For attested party onboarding, the first APDU can contain an optional challenge after the derivation path:

- challenge_len (1 byte, value: 0x18)
- challenge (24 bytes): 16 bytes of random challenge and 8 bytes of deadline

The app accepts at most 3 messages in one flow.

**Example APDU sequence:**

```shell
// Send derivation path first
-> E0 06 01 03 2E 058000002C80001A6F800000008000000080000000 18 F91C61B67F2A948A723A750DC89B407D2C54C7C27D3AC23F // Derivation path + challenge (P2_FIRST | P2_MORE)
<= 9000

// Send namespace delegation transaction (130 bytes)
-> E0 06 01 06 82 0A7E080110011A780A760A443132323062366665623630366665373264653936303634613333323232613962356335386361363135323662356161623761613231366537646236636437363336343037122C10031A20FF2D9C046D1FDF68E65ECD36278EB8EAD12AA38EF5B323769BAA5DF9216F85D820012A02010430011801101E
<= 9000

// Send party-to-key mapping transaction (137 bytes)
-> E0 06 01 06 89 0A8401080110021A7E82017B0A49626F623A3A31323230623666656236303666653732646539363036346133333232326139623563353863613631353236623561616237616132313665376462366364373633363430371801222C10031A20FF2D9C046D1FDF68E65ECD36278EB8EAD12AA38EF5B323769BAA5DF9216F85D820012A0201043001101E
<= 9000

// Send party-to-participant mapping transaction (179 bytes, final message)
-> E0 06 01 04 B3 0AAE01080110011AA7014AA4010A49626F623A3A313232306236666562363036666537326465393630363461333332323261396235633538636136313532366235616162376161323136653764623663643736333634303710011A550A517061727469636970616E743A3A31323230313235366361636138616135343236343436643262633461313632393163343862373537326238616262306431623365656136336364323939363732316362321002101E
// Response containing [signature length (1 byte)][signature (64)][v (1 byte)][challenge signature length (1 byte)][challenge signature (64)]
<= 40 50739f146e51c76cd98a1cc0a2e5d18cf40417217660423bf9b42f9060eb580eaf07415a3ca88169eadca1eb9a69a65a4180abbfdea198fb043fcb66bc5bd903 00 40 09f8d654f68f42a3aff275e57afb0f42bbbd145b6cc3a5fbfdf53462a32fe4b5f57606fc974d063662a1c1d769491bb71ecb13c6a4d152ff996ec7d64137870c 9000
```

**P2 Flag Usage:**

- 0x03 `P2_FIRST | P2_MORE`: Derivation path - first APDU, transactions following.
- 0x02 `P2_MORE`: More chunks to come for current transaction (if transaction split in several APDUs)
- 0x06 `P2_MORE | P2_MSG_END`: Transaction complete, more transactions expected
- 0x04 `P2_MSG_END`: Final transaction, end of sequence

**Processing Algorithm:**

1. The app receives the derivation path and the optional 24-byte attestation challenge in the first APDU.
2. The app hashes each fully received transaction using PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE (purpose 11),
   and checks it
   (see [TRANSACTION.md](./TRANSACTION.md#topology-transactions-for-onboarding-p1--0x01)).
3. Once all transaction messages are received, the app sorts the 34-byte hashes lexicographically in hex format.
4. The app concatenates the sorted hashes with length prefixes (big-endian):
   - 4-byte count of hashes
   - For each hash: 4-byte length (34) + 34-byte hash data
5. The app computes the final 34-byte hash using PURPOSE_MULTI_TOPOLOGY_TRANSACTION_SIGNATURE (purpose 55).
   If the challenge was provided in the first APDU, the app signs it with the attestation key
   (signing data = multi-hash + challenge).
6. After user approval, the app signs the multi-hash with the account key and returns the 64-byte Ed25519 signature,
   followed by the challenge signature if the challenge was provided.

#### SIGN_PREPARED_TRANSACTION (P1 = 0x02) Example

Signs a structured Canton transaction with multiple components sent in sequence.

As the components, this command uses parts of `PreparedTransaction` [protobuf message](https://github.com/digital-asset/canton/blob/main/community/ledger-api/src/main/protobuf/com/daml/ledger/api/v2/interactive/interactive_submission_service.proto),
split in the way described in [SPLIT_TRANSACTION.md](./SPLIT_TRANSACTION.md).

```shell
-> E0 06 02 03 15 058000002C80001A6F800000008000000080000000  // Derivation path (P2_FIRST | P2_MORE)
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
-> E0 06 02 04 80 [128 bytes contract 0 data]                   // Final chunk of the last component (P2_MSG_END)
<= [sig_len] [signature] [v] 9000
```

**Component sequence for prepared transactions:**

1. Derivation path
2. DAML transaction data
3. Transaction nodes (one or more)
4. Metadata
5. Input contracts (zero or more)

For more details on the transmission sequence of components, see [#Sending order](./SPLIT_TRANSACTION.md#sending-order)

Each component (**except the path**) ends with `P2_MSG_END` to signal completion of that data type.
The last component (the last input contract, or the metadata if there is no input contract) ends with
`P2_MSG_END` alone (0x04). The other components end with `P2_MORE | P2_MSG_END` (0x06).
The app then shows the transaction and returns the signature after user approval.

#### Response Format

All signing modes return the same format:

- sig_len (1 byte): 0x40 (64 bytes)
- signature (64 bytes): raw Ed25519 signature (R || S), made with the account key derived with SLIP-10.
  The app does not DER-encode it.
- v (1 byte): always 0x00. Ed25519 has no recovery value; the byte is kept for format compatibility.
- Only for `SIGN_UNTYPED_VERSIONED_MESSAGE` with a challenge:
  - challenge_sig_len (1 byte): 0x40
  - challenge_signature (64 bytes): raw Ed25519 signature made with the attestation key
- SW (2 bytes): Status word

## Status Words

| SW | Description |
| --- | --- |
| 0x9000 | Success |
| 0x6985 | Denied by user (SW_DENY) |
| 0x6A80 | Blind signing needed but disabled in the settings (SWO_INCORRECT_DATA) |
| 0x6A86 | Wrong P1/P2 (SW_WRONG_P1P2) |
| 0x6A87 | Wrong data length (SW_WRONG_DATA_LENGTH) |
| 0x6D00 | Invalid instruction (SW_INS_NOT_SUPPORTED) |
| 0x6E00 | Invalid CLA (SW_CLA_NOT_SUPPORTED) |
| 0xB002 | Display address failed (SW_DISPLAY_ADDRESS_FAIL) |
| 0xB004 | Wrong transaction length (SW_WRONG_TX_LENGTH) |
| 0xB005 | Transaction parsing failed (SW_TX_PARSING_FAIL) |
| 0xB006 | Transaction hash failed (SW_TX_HASH_FAIL) |
| 0xB007 | Bad state (SW_BAD_STATE) |
| 0xB008 | Signature failed (SW_SIGNATURE_FAIL) |
| 0xB009 | Challenge signature failed (SW_CHALLENGE_SIGNATURE_FAIL) |
| 0xB00A | Invalid node tree in the prepared transaction (SW_TX_INVALID_NODE_TREE) |
| 0xC101 | Multiple namespace delegations (SW_TOPOLOGY_MULTIPLE_NAMESPACE_DELEGATIONS) |
| 0xC103 | Missing target key in namespace delegation (SW_TOPOLOGY_MISSING_TARGET_KEY) |
| 0xC104 | Key is not the device public key (SW_TOPOLOGY_PARTY_KEY_MISMATCH) |
| 0xC105 | Key has a wrong format (SW_TOPOLOGY_PARTY_KEY_WRONG_FORMAT) |
| 0xC106 | Missing key (SW_TOPOLOGY_MISSING_PARTY_KEY) |
| 0xC201 | Party is not the device party ID (SW_TOPOLOGY_PARTY_ID_MISMATCH) |
| 0xC202 | Party-to-key mapping does not have exactly 1 signing key (SW_TOPOLOGY_UNEXPECTED_PARTY_TO_KEY_SIGNING_KEYS_COUNT) |
| 0xC203 | Party-to-key threshold is not the number of signing keys (SW_TOPOLOGY_UNEXPECTED_PARTY_TO_KEY_THRESHOLD_VALUE) |
| 0xC204 | Multiple party-to-key mappings (SW_TOPOLOGY_MULTIPLE_PARTY_TO_KEY_MAPPINGS) |
| 0xC301 | Missing party in party-to-participant mapping (SW_TOPOLOGY_MISSING_PARTY) |
| 0xC302 | Not exactly 2 participants (SW_TOPOLOGY_UNEXPECTED_NUMBER_OF_PARTICIPANTS) |
| 0xC303 | Duplicate participant (SW_TOPOLOGY_UNEXPECTED_DUPLICATE_PARTICIPANT) |
| 0xC304 | Missing participant data (SW_TOPOLOGY_MISSING_PARTICIPANT_DATA) |
| 0xC305 | Participant is not an expected validator (SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID) |
| 0xC306 | Threshold is not the number of participants (SW_TOPOLOGY_UNEXPECTED_THRESHOLD_VALUE) |
| 0xC307 | Unexpected party signing keys (SW_TOPOLOGY_UNEXPECTED_PARTY_SIGNING_KEYS) |
| 0xC308 | Multiple party-to-participant mappings (SW_TOPOLOGY_MULTIPLE_PARTY_TO_PARTICIPANTS) |
| 0xC401 | Unknown mapping type (SW_TOPOLOGY_UNKNOWN_MAPPING_TYPE) |
| 0xC402 | Topology operation is not `ADD_REPLACE` (SW_TOPOLOGY_UNSUPPORTED_OPERATION) |
| 0xC403 | Mandatory field missing (SW_TOPOLOGY_MANDATORY_FIELD_MISSING) |
| 0xC404 | More than 3 topology messages (SW_TOPOLOGY_TOO_MANY_MESSAGES) |

If the key derivation fails in `GET_PUBLIC_KEY`, the app returns the error code of the crypto library
as the status word.
