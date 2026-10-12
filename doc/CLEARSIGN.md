# Clear Signing of Canton Transactions

The Canton app clear signs two kinds of transactions:

1. **Prepared transactions**: transfers, transfer offers and pre-approvals
   (`src/transaction/pb_node_display_parser.c`, `src/transaction/pb_node_display_config.c`).
2. **Topology transactions**: account onboarding (`src/transaction/untyped_versioned_msg.c`).

If the app cannot clear sign a transaction, it falls back to blind signing.
[TRANSACTION.md](./TRANSACTION.md) describes the data and the checks.

---

## 1. Prepared transactions

### How the app selects a screen

The app decodes each node with nanopb callbacks. It builds the path of each field as it walks
the node argument, for example `transfer.instrumentId.id`.
A dot inside a field name is escaped with a backslash, for example
`transfer.meta.values.splice\.lfdecentralizedtrust\.org/reason`.

The outermost record of the node argument selects the screen. The app selects a screen only if
the node performs the matching action:

- For an exercise node, the choice ID must be the record name, and the node template must be
  one of the templates listed for the screen.
- For a create node, the record must be the node template.

The app uses the first node that matches a screen. When the path of a field matches a path in
the field list of the screen, the app stores the value.

For accept, reject and withdraw, the exercise node only selects the screen.
The app then reads the fields from the disclosed input contract that the node exercises.
That input contract must have the same contract ID as the exercised contract, and its template
must be `Splice.AmuletTransferInstruction:AmuletTransferInstruction` or
`Utility.Registry.App.V0.Model.Transfer:TransferOffer`.

### Supported actions

| Action | Record that selects the screen | Node template |
| --- | --- | --- |
| Send tokens | `Splice.Api.Token.TransferInstructionV1:TransferFactory_Transfer` and `Splice.Api.Token.TransferInstructionV2:TransferFactory_Transfer` | `Splice.ExternalPartyAmuletRules:ExternalPartyAmuletRules` or `Utility.Registry.App.V0.Service.AllocationFactory:AllocationFactory` |
| Send Canton Coin | `Splice.ExternalPartyAmuletRules:ExternalPartyAmuletRules_CreateTransferCommand` | `Splice.ExternalPartyAmuletRules:ExternalPartyAmuletRules` |
| Pre-approve incoming transfers | `Splice.Wallet.TransferPreapproval:TransferPreapprovalProposal` (create) | The same template |
| Accept an incoming transfer | `TransferInstruction_Accept`, V1 and V2 | `AmuletTransferInstruction` or `TransferOffer` |
| Reject an incoming transfer | `TransferInstruction_Reject`, V1 and V2 | `AmuletTransferInstruction` or `TransferOffer` |
| Withdraw a transfer offer | `TransferInstruction_Withdraw`, V1 and V2 | `AmuletTransferInstruction` or `TransferOffer` |

For accept, reject and withdraw, the records are in the modules
`Splice.Api.Token.TransferInstructionV1` and `Splice.Api.Token.TransferInstructionV2`.
The V1 and V2 choices use the same screen and the same field list.

### Displayed fields

In the tables below, a field without a label is read but not shown.

#### Send tokens

Title: "Review transaction to send tokens".
If the ticker is CC, the title is "Review transaction to send Canton Coin",
and the last screen is "Sign transaction to send Canton Coin?".

| Label | V1 path | V2 path | Mandatory |
| --- | --- | --- | --- |
| From | `transfer.sender` | `transfer.sender.owner` | Yes |
| Amount | `transfer.amount` | `transfer.amount` | Yes |
| To | `transfer.receiver` | `transfer.receiver.owner` | Yes |
| Token | `transfer.instrumentId.id` | `transfer.instrumentId.id` | Yes |
| (none) | `transfer.instrumentId.admin` | `transfer.instrumentId.admin` | Yes |
| Memo | `transfer.meta.values.splice\.lfdecentralizedtrust\.org/reason` | Same as V1 | No |

#### Send Canton Coin

Title: "Review transaction to send Canton Coin".

| Label | Path | Mandatory |
| --- | --- | --- |
| From | `sender` | Yes |
| Amount | `amount` (the app adds "CC") | Yes |
| To | `receiver` | Yes |
| Memo | `description` | No |

#### Pre-approve incoming transfers

Title: "Review transaction to pre-approve incoming transfers".

| Label | Path | Mandatory |
| --- | --- | --- |
| Pre-approve for account | `receiver` | Yes |
| For asset | Fixed value "Canton Coin (CC)" | Yes |
| By validator | `provider` | Yes |

#### Accept and reject an incoming transfer

Titles: "Review transaction to accept incoming transfer" and
"Review transaction to reject incoming transfer".
Both screens use the same field list, read from the disclosed transfer instruction contract.

| Label | Path | Mandatory |
| --- | --- | --- |
| From | `transfer.sender` | Yes |
| Amount | `transfer.amount` | Yes |
| To | `transfer.receiver` | Yes |
| Token | `transfer.instrumentId.id` | Yes |
| (none) | `transfer.instrumentId.admin` | Yes |
| Expiration time | `transfer.executeBefore` | Yes |
| Memo | `transfer.meta.values.splice\.lfdecentralizedtrust\.org/reason` | No |

#### Withdraw a transfer offer

Title: "Review transaction to withdraw transfer offer".
The fields are read from the disclosed transfer instruction contract.

| Label | Path | Mandatory |
| --- | --- | --- |
| Withdraw to | `transfer.sender` | Yes |
| Amount | `transfer.amount` | Yes |
| Token | `transfer.instrumentId.id` | Yes |
| (none) | `transfer.instrumentId.admin` | Yes |

### Formatting

- **Amount**: the app removes the trailing zeros after the decimal point, and the decimal point
  if the amount is a whole number.
- **Ticker**: for token standard actions, the app looks up the instrument admin and ID in a list
  of known tokens (`INSTRUMENT_TO_TICKER_MAPPINGS`). The list has CC, CBTC, USDCx, SBC and CUSD
  for some networks. If the token is known, the app adds the ticker to the amount and does not
  show the Token row. If the token is not known, the app falls back to blind signing.
- **Expiration time**: the app shows the timestamp in UTC with a 12-hour clock, as
  `YYYY-MM-DD hh:mm:ss AM/PM UTC`. The line break is at a different place on Nano devices and on
  touchscreen devices.

### When the app refuses or falls back to blind signing

- No node matches a screen: blind signing.
- The token is not known: blind signing.
- A value appears twice on the same path, or the app has no memory to store it: blind signing.
- A value has a type that the app cannot show: blind signing.
- The value check fails (see [TRANSACTION.md](./TRANSACTION.md)): blind signing.
- The node tree check fails (see [TRANSACTION.md](./TRANSACTION.md)):
  blind signing, or refusal for a tree that a correct host cannot produce.
- A mandatory field is missing or empty: the app refuses the transaction with `SW_TX_PARSING_FAIL`.
- A node or an input contract cannot be decoded, or the app has no memory for the displayed fields:
  the app refuses the transaction with `SW_TX_PARSING_FAIL`.

---

## 2. Topology transactions (onboarding)

### Overview

The host sends the topology transactions that create the account.
The app checks them and shows the account and its validators.

### Supported mappings

- **Namespace delegation**: the target key must be the device public key.
- **Party-to-key mapping**: the party must be the device party ID. There must be exactly
  1 signing key, and it must be the device public key. The threshold must be 1.
- **Party-to-participant mapping**: the party must be the device party ID. There must be
  exactly 2 participants, and they must be the Ledger and Kiln validators of one network.
  The threshold must be 2. The mapping must not contain party signing keys.

The app accepts the public key in raw format or in DER X.509 format.
Each mapping can appear at most once.

### Displayed fields

Title: "Review transaction to add account". Finish: "Sign transaction to add account?".

| Label | Value |
| --- | --- |
| Add account | The party ID |
| Associate to validator 1 | The name of the first participant, for example "Ledger Validator" on MainNet |
| Associate to validator 2 | The name of the second participant, for example "Kiln Validator" on MainNet |
| Validators threshold | "2 out of 2" |

The participant IDs and names for MainNet, TestNet and DevNet are in
`src/transaction/untyped_versioned_msg.c`.

### Clear signing availability

The app clear signs only if it received all three mappings.
If a mapping is missing, the app falls back to blind signing.
If all three mappings are present but the "Add account" or "Associate to validator 1" field is
missing, the app refuses the transaction with `SW_TOPOLOGY_MANDATORY_FIELD_MISSING` (0xC403).
If a check above fails, the app refuses the transaction with a `0xC1xx` to `0xC4xx` status word
(see [APDU.md](./APDU.md)).

### Device challenge

The host can send a challenge: a length byte (0x18), then 16 bytes of challenge and
8 bytes of deadline.
The app signs the multi-hash and the challenge with the attestation key, which is separate from
the account key. The response contains this signature after the transaction signature.

---

## Blind signing

If the app cannot clear sign, it shows "Review transaction hash" and the hash in hexadecimal.
The user can sign only if the "Blind signing" setting is enabled.
If the setting is disabled, the app shows an error and returns `0x6A80` (`SWO_INCORRECT_DATA`).
