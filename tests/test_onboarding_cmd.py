"""Party onboarding flows: topology transactions and attestation."""

import os
from enum import IntEnum
from typing import Optional
import pytest

# pylint: disable=import-error
from generateCryptoData import get_keys_bytes
from ragger.backend.interface import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.canton_transaction import Transaction
from application_client.canton_command_sender import (
    CantonCommandSender,
    Errors,
)
from application_client.canton_response_unpacker import (
    unpack_get_public_key_response,
    unpack_sign_tx_response,
)
from utils import verify_signature

from signing_flows import (
    ROOT_SCREENSHOT_PATH,
    MAINNET_VALIDATOR_PARTY_ID_1,
    MAINNET_VALIDATOR_PARTY_ID_2,
    TESTNET_VALIDATOR_PARTY_ID_1,
    TESTNET_VALIDATOR_PARTY_ID_2,
    DEVNET_VALIDATOR_PARTY_ID_1,
    DEVNET_VALIDATOR_PARTY_ID_2,
    sign_and_verify_prepared_transaction,
)


def _onboard_party(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    validator_uids: Optional[list[str]] = None,
    attestation_keys: Optional[tuple[bytes, bytes]] = None,
    der_key_format: bool = True,
    snapshot_check: bool = True,
) -> None:
    client = CantonCommandSender(backend)

    if validator_uids is None:
        validator_uids = [MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_2]

    # Get public key
    _, raw_key, _, _ = unpack_get_public_key_response(client.get_public_key(path="m/44'/6767'/0'/0'/0'").data)

    # Convert to DER format for inclusion in topology transactions
    public_key = b"\x30\x2a\x30\x05\x06\x03\x2b\x65\x70\x03\x21\x00" + raw_key if der_key_format else raw_key

    # Create and hash transactions
    txs = [
        Transaction.namespace_delegation(public_key, der_key_format),
        Transaction.party_to_key(public_key, der_key_format),
        Transaction.party_to_participant_from_uid(public_key, validator_uids),
    ]
    multi_hash = Transaction.compute_multi_transaction_hash(
        [Transaction.compute_topology_transaction_hash(tx) for tx in txs]
    )

    # Sign transactions
    challenge = os.urandom(24) if attestation_keys else None
    with client.sign_topology_tx(path="m/44'/6767'/0'/0'/0'", transactions=txs, challenge=challenge):
        scenario_navigator.review_approve(
            path=ROOT_SCREENSHOT_PATH,
            custom_screen_text="Sign transaction to",
            do_comparison=snapshot_check,
        )

    # Verify signatures
    _, der_sig, _, challenge_sig_len, challenge_sig = unpack_sign_tx_response(client.get_async_response().data)
    verify_signature(raw_key, multi_hash, der_sig)

    if attestation_keys:
        _verify_attestation(attestation_keys[1], multi_hash, challenge, challenge_sig, challenge_sig_len)
    else:
        assert challenge is None
        assert challenge_sig is None
        assert challenge_sig_len is None


class WhichPartyTx(IntEnum):
    PARTY_TO_KEY = 1
    PARTY_TO_PARTICIPANT = 2


def _onboard_party_expect_error(
    backend: BackendInterface,
    validator_uids: Optional[list[str]] = None,
    der_key_format: bool = True,
    threshold: Optional[int] = None,
    party_id: Optional[str] = None,
    which_party_tx: Optional[WhichPartyTx] = None,
    party_to_key_signing_keys_count: int = 1,
    party_to_key_threshold: Optional[int] = None,
    has_party_keys_in_party_to_participant: bool = False,
    expected_error: int = Errors.SW_WRONG_RESPONSE_LENGTH,
) -> None:
    client = CantonCommandSender(backend)

    if validator_uids is None:
        validator_uids = [MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_2]

    # Get public key
    _, raw_key, _, _ = unpack_get_public_key_response(client.get_public_key(path="m/44'/6767'/0'/0'/0'").data)

    # Convert to DER format for inclusion in topology transactions
    public_key = b"\x30\x2a\x30\x05\x06\x03\x2b\x65\x70\x03\x21\x00" + raw_key if der_key_format else raw_key

    if which_party_tx is None:
        which_party_tx = WhichPartyTx.PARTY_TO_PARTICIPANT

    # Create transactions
    txs = [
        Transaction.namespace_delegation(public_key, der_key_format),
        Transaction.party_to_key(
            public_key,
            der_key_format,
            party_id if which_party_tx == WhichPartyTx.PARTY_TO_KEY else None,
            signing_keys_count=party_to_key_signing_keys_count,
            threshold=party_to_key_threshold,
        ),
        Transaction.party_to_participant_from_uid(
            public_key,
            validator_uids,
            threshold,
            party_id if which_party_tx != WhichPartyTx.PARTY_TO_KEY else None,
            has_party_signing_keys=has_party_keys_in_party_to_participant,
        ),
    ]

    # Sign transactions and expect error
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_topology_tx(path="m/44'/6767'/0'/0'/0'", transactions=txs):
            pass
    assert e.value.status == expected_error


def test_sign_onboarding_expect_error_unexpected_participant_id(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        validator_uids=[MAINNET_VALIDATOR_PARTY_ID_1, "invalid_validator_id_2"],
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID,
    )


def test_sign_onboarding_expect_error_unexpected_number_of_participants_single(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        validator_uids=[MAINNET_VALIDATOR_PARTY_ID_1],
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_NUMBER_OF_PARTICIPANTS,
    )


def test_sign_onboarding_expect_error_unexpected_party_to_key_signing_keys_count(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        party_to_key_signing_keys_count=0,
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_PARTY_TO_KEY_SIGNING_KEYS_COUNT,
    )


def test_sign_onboarding_expect_error_unexpected_party_to_key_threshold_value(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        party_to_key_threshold=2,
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_PARTY_TO_KEY_THRESHOLD_VALUE,
    )


def test_sign_onboarding_expect_error_unexpected_threshold(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        threshold=3,
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_THRESHOLD_VALUE,
    )


def test_sign_onboarding_expect_error_unexpected_number_of_participants_three(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        validator_uids=[
            MAINNET_VALIDATOR_PARTY_ID_1,
            MAINNET_VALIDATOR_PARTY_ID_2,
            "extra_validator_id_3",
        ],
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_NUMBER_OF_PARTICIPANTS,
    )


def test_sign_onboarding_expect_error_duplicate_participants(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        validator_uids=[MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_1],
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_DUPLICATE_PARTICIPANT,
    )


def test_sign_onboarding_expect_error_wrong_party_id_in_party_to_key(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        party_id="invalid_party_id_in_party_to_key",
        which_party_tx=WhichPartyTx.PARTY_TO_KEY,
        expected_error=Errors.SW_TOPOLOGY_PARTY_ID_MISMATCH,
    )


def test_sign_onboarding_expect_error_wrong_party_id_in_party_to_participant(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        party_id="invalid_party_id_in_party_to_participant",
        which_party_tx=WhichPartyTx.PARTY_TO_PARTICIPANT,
        expected_error=Errors.SW_TOPOLOGY_PARTY_ID_MISMATCH,
    )


def test_sign_onboarding_expect_error_unexpected_party_signing_keys(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error(
        backend,
        has_party_keys_in_party_to_participant=True,
        expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_PARTY_SIGNING_KEYS,
    )


class TopologyTxKind(IntEnum):
    NAMESPACE_DELEGATION = 1
    PARTY_TO_KEY = 2
    PARTY_TO_PARTICIPANT = 3


def _onboard_party_expect_error_for_sequence(
    backend: BackendInterface,
    sequence: list[TopologyTxKind],
    expected_error: int,
) -> None:
    """Send an arbitrary sequence of topology messages and expect one status word.

    The helper above always sends one message of each kind. This one lets a test repeat a kind, or
    send more messages than the app can hold, which is what the sequencing checks reject.
    """
    client = CantonCommandSender(backend)

    _, raw_key, _, _ = unpack_get_public_key_response(client.get_public_key(path="m/44'/6767'/0'/0'/0'").data)
    public_key = b"\x30\x2a\x30\x05\x06\x03\x2b\x65\x70\x03\x21\x00" + raw_key

    txs = []
    for kind in sequence:
        if kind == TopologyTxKind.NAMESPACE_DELEGATION:
            txs.append(Transaction.namespace_delegation(public_key, True))
        elif kind == TopologyTxKind.PARTY_TO_KEY:
            txs.append(Transaction.party_to_key(public_key, True))
        else:
            txs.append(
                Transaction.party_to_participant_from_uid(
                    public_key,
                    [MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_2],
                )
            )

    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_topology_tx(path="m/44'/6767'/0'/0'/0'", transactions=txs):
            pass
    assert e.value.status == expected_error


def test_sign_onboarding_expect_error_multiple_namespace_delegations(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error_for_sequence(
        backend,
        [TopologyTxKind.NAMESPACE_DELEGATION, TopologyTxKind.NAMESPACE_DELEGATION],
        Errors.SW_TOPOLOGY_MULTIPLE_NAMESPACE_DELEGATIONS,
    )


def test_sign_onboarding_expect_error_multiple_party_to_key_mappings(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error_for_sequence(
        backend,
        [
            TopologyTxKind.NAMESPACE_DELEGATION,
            TopologyTxKind.PARTY_TO_KEY,
            TopologyTxKind.PARTY_TO_KEY,
        ],
        Errors.SW_TOPOLOGY_MULTIPLE_PARTY_TO_KEY_MAPPINGS,
    )


def test_sign_onboarding_expect_error_multiple_party_to_participants(
    backend: BackendInterface,
) -> None:
    _onboard_party_expect_error_for_sequence(
        backend,
        [
            TopologyTxKind.NAMESPACE_DELEGATION,
            TopologyTxKind.PARTY_TO_PARTICIPANT,
            TopologyTxKind.PARTY_TO_PARTICIPANT,
        ],
        Errors.SW_TOPOLOGY_MULTIPLE_PARTY_TO_PARTICIPANTS,
    )


def test_sign_onboarding_expect_error_too_many_messages(
    backend: BackendInterface,
) -> None:
    # A fourth message has nowhere to store its hash, so it is refused before it is even parsed.
    _onboard_party_expect_error_for_sequence(
        backend,
        [
            TopologyTxKind.NAMESPACE_DELEGATION,
            TopologyTxKind.PARTY_TO_KEY,
            TopologyTxKind.PARTY_TO_PARTICIPANT,
            TopologyTxKind.NAMESPACE_DELEGATION,
        ],
        Errors.SW_TOPOLOGY_TOO_MANY_MESSAGES,
    )


def _verify_attestation(
    attest_pub_key: bytes,
    multi_hash: bytes,
    challenge: Optional[bytes],
    challenge_sig: Optional[bytes],
    challenge_sig_len: int | None,
) -> None:
    assert challenge_sig is not None
    assert challenge_sig_len is not None
    assert challenge_sig_len == 64 == len(challenge_sig)
    assert challenge is not None
    verify_signature(attest_pub_key, multi_hash + challenge, challenge_sig)


def test_sign_onboarding_attested(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(backend, scenario_navigator, attestation_keys=(attest_key, attest_pub_key))


def test_sign_onboarding_attested_devnet_multi(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(
        backend,
        scenario_navigator,
        attestation_keys=(attest_key, attest_pub_key),
        validator_uids=[DEVNET_VALIDATOR_PARTY_ID_1, DEVNET_VALIDATOR_PARTY_ID_2],
    )


def test_sign_onboarding_attested_testnet_multi(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(
        backend,
        scenario_navigator,
        attestation_keys=(attest_key, attest_pub_key),
        validator_uids=[TESTNET_VALIDATOR_PARTY_ID_1, TESTNET_VALIDATOR_PARTY_ID_2],
    )


def test_sign_onboarding_raw_format_key(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    _onboard_party(backend, scenario_navigator, der_key_format=False)


def test_sign_onboard_then_preapprove(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    for _ in range(10):
        _onboard_party(backend, scenario_navigator, snapshot_check=False)
        sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/preapproval_proposal.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False,
        )
