# Clear Signing Mechanisms for Canton Transactions

The Canton app implements two distinct clear signing mechanisms for different
transaction types:

1. **Prepared Transactions** - Standard Canton transactions (transfers, pre-approvals)
2. **Topology Transactions** - Account onboarding and participant management

---

## 1. Prepared Transactions Clear Signing (`pb_node_display_parser.c`)

### Overview

Parses protobuf transaction nodes to extract key transaction information for
user review and displays them through the `ui_display_transaction()` interface.

### How Fields Are Displayed

The clear signing mechanism populates the global `transaction_ctx_t` structure
with parsed field data, which is then consumed by the UI display system:

1. **Field Extraction**: Values are parsed from protobuf and stored
in `tx_field_t` structures
2. **Display Population**: Field values are copied to `display_items_strings`
array with corresponding labels
3. **UI Display**: `ui_display_transaction()` reads from the populated
transaction context to show fields to the user
4. **User Confirmation**: User reviews the displayed fields and confirms
or rejects the transaction

### Supported Transaction Types (lines 113-123)

#### Token Transfers

- **Identifier**: `Splice.Api.Token.TransferInstructionV1.TransferFactory_Transfer`

#### Native Coin Transfers

- **Identifier**: `Splice.ExternalPartyAmuletRules.ExternalPartyAmuletRules_CreateTransferCommand`

#### Pre-approval Proposals

- **Identifier**: `Splice.Wallet.TransferPreapproval.TransferPreapprovalProposal`

### Protobuf Callback Parsing

The parsing mechanism uses nanopb callbacks to extract field values during protobuf
decoding:

- **Callback Function**: `decode_value_var()` - registered as nanopb callback
to capture field values

- **Context Tracking**: `pb_callback_context_t` maintains parsing state, field
paths, and extracted values

- **Path Building**: Field paths are built dynamically as the parser traverses
nested protobuf structures

- **Value Extraction**: When target field paths are matched, values are
extracted and stored for display

### Field Extraction Process

1. **Path Tracking**: Builds field paths by traversing protobuf structure
   - Dots in field names escaped with backslashes
   - Example: `transfer.meta.values.splice\.lfdecentralizedtrust\.org/reason`
2. **Node Parsing**: Processes Exercise/Create nodes from transaction structure
3. **Field Matching**: Maps extracted values to display configurations by
comparing field paths against predefined field configuration arrays
4. **Formatting**: Applies amount formatting and ticker resolution via callback functions
5. **Display Assignment**: Populates `transaction_ctx_t.display_items` with label-value pairs for UI consumption

### Displayed Fields Configuration (lines 166-178)

The following fields are extracted from transactions and displayed to users for confirmation:

#### Token Transfers (max 5 fields)

- **From**: `transfer.sender` (mandatory) - Shows sender's party ID
- **Amount**: `transfer.amount` (mandatory, with ticker if recognized) - Shows transfer amount with currency symbol
- **To**: `transfer.receiver` (mandatory) - Shows recipient's party ID
- **Token**: `transfer.instrumentId.id` (mandatory, hidden if ticker shown) - Shows token identifier
- **Memo**: `transfer.meta.values.splice\.lfdecentralizedtrust\.org/reason` (optional) - Shows transaction description

#### Native Coin Transfers (max 4 fields)

- **From**: `sender` (mandatory) - Shows sender's party ID
- **Amount**: `amount` (mandatory, auto-appends "CC") - Shows amount in Canton Coin
- **To**: `receiver` (mandatory) - Shows recipient's party ID
- **Memo**: `description` (optional) - Shows transaction description

#### Pre-approval Proposals (max 3 fields)

- **Pre-approve for account**: `receiver` (mandatory) - Shows account to pre-approve
- **For asset**: Static "Canton Coin (CC)" (mandatory) - Shows asset type being pre-approved
- **By validator**: `provider` (mandatory) - Shows validating participant

### Amount Formatting & Field Arrays

- Removes trailing zeros after decimal point
- Removes decimal point if whole number
- Appends tickers: `"Amulet"/"amulet"` → `"CC"`
- Field configurations stored in arrays for each transaction type

---

## 2. Topology Transactions Clear Signing (`untyped_versioned_msg.c`)

### Overview

Processes account onboarding transactions that establish party-to-participant
relationships and displays account setup information through the same
`ui_display_transaction()` interface.

### How Fields Are Displayed

Similar to prepared transactions, topology transactions populate the transaction
context:

1. **Mapping Processing**: Parses namespace delegation, party-to-key
and party-to-participant mappings
2. **Field Population**: Extracts party IDs, participant UIDs, and threshold information
3. **Display Assignment**: Populates display fields with account setup details
4. **UI Display**: `ui_display_transaction()` shows the onboarding information
for user confirmation

### Transaction Flow

1. **Hash Collection**: Each topology transaction is hashed individually
2. **Multi-Hash Computation**: All hashes are sorted and combined into final hash
3. **Challenge Signing**: Optional challenge+deadline signature with
attestation key
4. **Display Population**: Extracts and validates account setup information

### Supported Mapping Types

#### Namespace Delegation

- Validates target key matches derived public key
- Establishes namespace authority

#### Party-to-Key Mapping

- Validates party ID derivation from public key
- Confirms signing key ownership
- Supports RAW and DER X.509 key formats

#### Party-to-Participant Mapping

- **Core display logic** - populates user-facing fields
- Maps party to validator participants
- Shows threshold requirements for multi-validator setups

### Displayed Fields Configuration

The following account setup information is displayed to users for confirmation:

#### Account Setup (max 5 fields)

- **Add account**: Party ID (mandatory) - Shows the party ID being created
- **Associate to validator 1**: First participant UID (mandatory) - Shows primary validator
- **Associate to validator 2**: Second participant UID (optional) - Shows secondary validator
- **Associate to validator 3**: Third participant UID (optional) - Shows tertiary validator
- **Validators threshold**: "X out of Y" format (optional, shown if >1 participant) - Shows signing threshold

### Security Validations

1. **Key Validation**: Ensures topology keys match device-derived keys
2. **Party ID Validation**: Confirms party ID derived from public key
3. **Completeness Check**: All three mapping types must be present
4. **Mandatory Field Check**: Required fields must be found and validated

### Challenge Response

- Optional 16-byte challenge + 8-byte deadline
- Signed with attestation key (separate from transaction signing)
- Proves device possession during onboarding

---

## Common Features

### UI Integration

Both clear signing mechanisms use the same display interface:

- **Display Population**: Field values and labels are stored in
`transaction_ctx_t.display_items`
- **UI Rendering**: `ui_display_transaction()` reads from the transaction context
to show fields
- **User Flow**: Users scroll through displayed fields and confirm/reject the transaction

### Memory Management

- Dynamic allocation with `app_mem_alloc()`
- Proper cleanup on context reset
- Display strings managed in `display_items_strings` arrays

### Error Handling

- Falls back to blind signing if parsing fails
- Validates all mandatory fields present
- Returns specific error codes for debugging

### Clear Signing Availability

Set to `true` when:

- Transaction type is recognized
- All mandatory fields successfully parsed and validated
- No critical parsing errors occurred

When clear signing is available, users see the specific transaction details listed above. When unavailable, users only see a transaction hash for blind signing approval.
