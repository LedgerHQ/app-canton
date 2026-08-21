#pragma once

/**
 * Status word for success.
 */
#define SW_OK 0x9000
/**
 * Status word for denied by user.
 */
#define SW_DENY 0x6985
/**
 * Status word for incorrect P1 or P2.
 */
#define SW_WRONG_P1P2 0x6A86
/**
 * Status word for either wrong Lc or length of APDU command less than 5.
 */
#define SW_WRONG_DATA_LENGTH 0x6A87
/**
 * Status word for unknown command with this INS.
 */
#define SW_INS_NOT_SUPPORTED 0x6D00
/**
 * Status word for instruction class is different than CLA.
 */
#define SW_CLA_NOT_SUPPORTED 0x6E00
/**
 * Status word for wrong response length (buffer too small or too big).
 */
#define SW_WRONG_RESPONSE_LENGTH 0xB000
/**
 * Status word for fail to display BIP32 path.
 */
#define SW_DISPLAY_BIP32_PATH_FAIL 0xB001
/**
 * Status word for fail to display address.
 */
#define SW_DISPLAY_ADDRESS_FAIL 0xB002
/**
 * Status word for fail to display amount.
 */
#define SW_DISPLAY_AMOUNT_FAIL 0xB003
/**
 * Status word for wrong transaction length.
 */
#define SW_WRONG_TX_LENGTH 0xB004
/**
 * Status word for fail of transaction parsing.
 */
#define SW_TX_PARSING_FAIL 0xB005
/**
 * Status word for fail of transaction hash.
 */
#define SW_TX_HASH_FAIL 0xB006
/**
 * Status word for bad state.
 */
#define SW_BAD_STATE 0xB007
/**
 * Status word for signature fail.
 */
#define SW_SIGNATURE_FAIL 0xB008
/**
 * Status word for challenge signature fail.
 */
#define SW_CHALLENGE_SIGNATURE_FAIL 0xB009

/**
 * Topology transaction error codes (0xC100-0xC4FF range)
 */

// Namespace delegation errors (0xC100 range)
/**
 * Status word for multiple namespace delegations found.
 */
#define SW_TOPOLOGY_MULTIPLE_NAMESPACE_DELEGATIONS 0xC101
/**
 * Status word for NULL namespace delegation.
 */
#define SW_TOPOLOGY_NULL_NAMESPACE_DELEGATION 0xC102
/**
 * Status word for missing target key in namespace delegation.
 */
#define SW_TOPOLOGY_MISSING_TARGET_KEY 0xC103
/**
 * Status word for party key mismatch.
 */
#define SW_TOPOLOGY_PARTY_KEY_MISMATCH 0xC104
/**
 * Status word for party key wrong format.
 */
#define SW_TOPOLOGY_PARTY_KEY_WRONG_FORMAT 0xC105
/**
 * Status word for missing party key.
 */
#define SW_TOPOLOGY_MISSING_PARTY_KEY 0xC106

// Party to key mapping errors (0xC200 range)
/**
 * Status word for party ID mismatch.
 */
#define SW_TOPOLOGY_PARTY_ID_MISMATCH 0xC201
/**
 * Status word for no signing keys found.
 */
#define SW_TOPOLOGY_NO_SIGNING_KEYS 0xC202

// Party to participant errors (0xC300 range)
/**
 * Status word for missing party field.
 */
#define SW_TOPOLOGY_MISSING_PARTY 0xC301
/**
 * Status word for unexpected number of participants.
 */
#define SW_TOPOLOGY_UNEXPECTED_NUMBER_OF_PARTICIPANTS 0xC302

/**
 * Status word for unexpected duplicate participant.
 */
#define SW_TOPOLOGY_UNEXPECTED_DUPLICATE_PARTICIPANT 0xC303

/**
 * Status word for missing participant data.
 */
#define SW_TOPOLOGY_MISSING_PARTICIPANT_DATA 0xC304

/**
 * status word for unexpected participant party ID.
 *
 */
#define SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID 0xC305

/**
 * Status word for unexpected threshold value.
 */
#define SW_TOPOLOGY_UNEXPECTED_THRESHOLD_VALUE 0xC306

// General topology transaction errors (0xC400 range)
/**
 * Status word for unknown mapping type.
 */
#define SW_TOPOLOGY_UNKNOWN_MAPPING_TYPE 0xC401
/**
 * Status word for unsupported topology operation.
 */
#define SW_TOPOLOGY_UNSUPPORTED_OPERATION 0xC402
/**
 * Status word for mandatory field missing.
 */
#define SW_TOPOLOGY_MANDATORY_FIELD_MISSING 0xC403
