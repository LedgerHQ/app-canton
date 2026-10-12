# Canton Transactions

## Overview

The app signs three kinds of data. The `SIGN_TX` APDU selects the kind with `P1`
(see [APDU.md](./APDU.md)):

| P1 | Data | Use |
| --- | --- | --- |
| 0x00 | A hash computed by the host (32 or 34 bytes) | Blind signing only |
| 0x01 | Topology transactions (`UntypedVersionedMessage`) | Account onboarding |
| 0x02 | A prepared transaction (`PreparedTransaction`) | Transfers and other ledger actions |

All signatures are Ed25519 signatures with the account key.
The app derives this key with SLIP-10 on the path that the host sends.
The derivation path must start with `44'/6767'`.

A `Hash` in this document is a Canton hash: the 2-byte multihash prefix `0x12 0x20`
(SHA-256, 32 bytes) followed by a SHA-256 digest.
A Canton hash with purpose `p` is `0x12 || 0x20 || sha256(u32_be(p) || data)`.

## Party ID

The app shows the account as a party ID:

```text
party_id = "ldg::" || lowercase_hex(canton_hash(purpose = 12, data = public_key))
```

`public_key` is the raw 32-byte Ed25519 public key.

## Prepared transactions (P1 = 0x02)

### Components

A `PreparedTransaction` is too large for the device memory, so the host splits it into
components and sends them one by one.
[SPLIT_TRANSACTION.md](./SPLIT_TRANSACTION.md) gives the split and the sending order:

1. The DAML transaction, without its nodes. It contains the number of nodes.
2. The nodes, one component for each node.
3. The metadata, without its input contracts. It contains the number of input contracts.
4. The input contracts, one component for each contract.

The app decodes each component with nanopb and hashes it as it arrives.
The app does not keep the complete transaction in memory.
One component can be at most 12 KiB.

### Hash

The app computes the hash of the Canton interactive submission hashing scheme, version 2:

```text
tx_hash   = sha256(u32_be(0x30) || encoding of the DAML transaction and its nodes)
meta_hash = sha256(u32_be(0x30) || encoding of the metadata and its input contracts)
hash      = sha256(u32_be(0x30) || 0x02 || tx_hash || meta_hash)
```

The app signs this 32-byte hash.
The encoding of each field is in `src/transaction/canonical_hash.c`.
If a field that the encoding needs is missing, the app refuses the transaction with `SW_TX_HASH_FAIL`.

### Checks before clear signing

The app shows the transaction details only if all the checks below pass.
If a check fails, the app falls back to blind signing.
In blind signing, the app shows only the transaction hash.
If blind signing is disabled in the settings, the app refuses the transaction.

- **Known action**: one node must match a supported action
  (see [CLEARSIGN.md](./CLEARSIGN.md)). The node must act on a template that the app expects
  for that action.
- **Displayed fields**: all mandatory fields of the action must be present, and the app must be
  able to store and format them.
- **Known token**: for token standard actions, the instrument (admin and ID) must be in the
  app's list of known tokens.
- **Node tree**: the nodes must form one tree with exactly one root.
  Each node must arrive once and have exactly one parent, and each child must arrive.
  If the tree cannot come from a correct host (for example, a duplicate node or a node that is
  its own child), the app refuses the transaction with `SW_TX_INVALID_NODE_TREE`.
  For other tree errors, the app falls back to blind signing.
- **Values**: the holdings that the transaction creates must agree with the screen.
  - Each holding must belong to the account that receives the value, or go back to the sender
    (change).
  - Exactly one holding must belong to the account that receives the value
    (one or two for a transfer to self).
    When the amount can be compared, one of these holdings must have the displayed amount.
  - For a token standard action, all holdings must be issued by the displayed instrument admin.
    A holding that names an instrument ID must name the displayed one.
  - A transaction behind a pre-approval screen must create no holding.
  - Each create node must use a template that the app knows.
  - Each disclosed input contract must be used by a node.

## Topology transactions for onboarding (P1 = 0x01)

### Messages

To onboard an account, the host sends the topology transactions of the new party.
Each message is a serialized `UntypedVersionedMessage` that contains one `TopologyTransaction`.
The host can send at most 3 messages.

The operation of each transaction that contains a mapping must be `ADD_REPLACE`.
A transaction without a mapping is hashed, but the app cannot clear sign it.
The app accepts these mappings:

| Mapping | Checks |
| --- | --- |
| `NamespaceDelegation` | The target key is the device public key. |
| `PartyToKeyMapping` | The party is the device party ID. There is exactly 1 signing key, and it is the device public key. The threshold is 1. |
| `PartyToParticipant` | The party is the device party ID. There are exactly 2 participants. The threshold is 2. The party has no signing keys of its own. |

The app accepts the public key in raw format (32 bytes) or in DER X.509 format (44 bytes).
Each mapping can appear at most once. Any other mapping type is refused.

### Expected participants

The 2 participants must be the 2 validators of one network, in any order:

| Network | Validators |
| --- | --- |
| MainNet | Ledger Validator, Kiln Validator |
| TestNet | Ledger Testnet Validator, Kiln Testnet Validator |
| DevNet | Ledger Devnet Validator, Kiln Devnet Validator |

The participant IDs are in `src/transaction/untyped_versioned_msg.c`.
If a participant is not in the list, the app refuses the transaction with
`SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID`.

### Hash

1. The app hashes each message (the complete `UntypedVersionedMessage` bytes) as a Canton hash
   with purpose 11.
2. The app sorts these 34-byte hashes by their hex encoding.
3. The app builds `u32_be(count)`, followed by `u32_be(34) || hash` for each hash.
4. The app computes the Canton hash of this data with purpose 55. This is the multi-hash.

The app signs the 34-byte multi-hash.

The app clear signs only if it received all three mappings.
Otherwise, it falls back to blind signing.

### Device challenge

The host can add a challenge to the first APDU, after the derivation path.
The challenge starts with a length byte (0x18), followed by 24 bytes:
16 bytes of random challenge and 8 bytes of deadline.
If the length byte is not 0x18 or the data is too short, the app returns `SW_WRONG_DATA_LENGTH`.
The app signs `multi_hash || challenge` with Ed25519 and a fixed attestation key.
The attestation key is in the app binary, not derived from the seed.
A production build sets it with the `PROD_CANTON_PRIVATE_KEY` make variable
(C define `PROD_PRIVATE_KEY`).
Other builds use `TEST_ATTESTATION_KEY`.
The host can check this signature to confirm that a Ledger device signed the onboarding.
The app returns the challenge signature after the transaction signature.

## Hash signing (P1 = 0x00)

The host sends a hash of 32 or 34 bytes. The app cannot check what the hash covers.
The app shows the hash and signs it only if blind signing is enabled in the settings.
