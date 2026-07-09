import json
import os
from enum import IntEnum
from pathlib import Path
from typing import Optional
import pytest
from ragger.backend.interface import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import NavInsID, Navigator, NavIns
from ledgered.devices import Device, DeviceType

from application_client.canton_transaction import Transaction
from application_client.canton_command_sender import (
    CantonCommandSender,
    P1SignType,
    Errors,
)
from application_client.canton_response_unpacker import (
    unpack_get_public_key_response,
    unpack_sign_tx_response,
)
from utils import verify_signature

# pylint: disable=import-error
from generateCryptoData import get_keys_bytes

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()

MAINNET_VALIDATOR_PARTY_ID_1 = (
    "ledger-ledgerops-2::12207a4859ad414f4f47c2d773ddf4ea88de8c3a1aab19abaa197e504acdbf679d3c"
)
MAINNET_VALIDATOR_PARTY_ID_2 = "Ledger-Kiln-1::12200386019c89269f5541595286cf5ebf24fe7884d8c6b05ce042c999f9161cb9d0"

TESTNET_VALIDATOR_PARTY_ID_1 = (
    "ledger-ledgeropstestnet-0::122095f38f5c73cc18fbeb3290f8c17f7a1ff190f66fe159c671cf1fb0dc634eedaf"
)
TESTNET_VALIDATOR_PARTY_ID_2 = (
    "Ledger-KilnTestnet-2::1220fa9df3caa84092023bf7edf28de1d28f96caf9b7d130385bfe6e284be6e0fbd7"
)

DEVNET_VALIDATOR_PARTY_ID_1 = (
    "ledger-ledgeropsdevnet-0::12208f74f551f8c28b68414fc3bb4b8466178055845485878a1af8ac1fe96f88fad2"
)
DEVNET_VALIDATOR_PARTY_ID_2 = (
    "Ledger-KilnDevnet-2::12203b77e5d74eb787ff0251fd76949379a625368646302a203fea7f7db1dd5402bf"
)


def _nano_enable_blind_signing() -> list[NavInsID]:
    # initial: go to settings
    seq = [NavInsID.RIGHT_CLICK, NavInsID.BOTH_CLICK]
    # enable
    seq += [NavInsID.BOTH_CLICK]
    # go to "back" screen
    seq += [NavInsID.RIGHT_CLICK]
    # back to main menu
    seq += [NavInsID.BOTH_CLICK]
    # back to home screen
    seq += [NavInsID.LEFT_CLICK]
    return seq

def _enable_blind_signing(device: Device, navigator: Navigator, snapshots_name: str) -> None:
    if device.is_nano:
        nav = _nano_enable_blind_signing()
    else:
        if device.type is DeviceType.APEX_P:
            coordinates = (263,95)
        else:
            coordinates = (348,132)
        nav = [NavInsID.USE_CASE_HOME_SETTINGS,
               NavIns(NavInsID.TOUCH, coordinates),
               NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]
    navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                   snapshots_name,
                                   nav,
                                   screen_change_before_first_instruction=False)


def _sign_and_verify_hash(
    backend: BackendInterface,
    device: Device,
    navigator: Navigator,
    scenario_navigator: NavigateWithScenario,
    tx_hash: bytes,
    test_name: str,
) -> None:
    client = CantonCommandSender(backend)
    path = "m/44'/6767'/0'/0'/0'"

    _enable_blind_signing(device, navigator, f"{test_name}_enable_bs")

    rapdu = client.get_public_key(path=path)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    print(f"Public key returned from device: {public_key.hex()}")

    with client.sign_tx(path=path, transaction=tx_hash, p1=P1SignType.P1_SIGN_HASH):
        scenario_navigator.review_approve_with_warning(
            path=ROOT_SCREENSHOT_PATH, test_name=test_name
        )

    response = client.get_async_response().data
    _, der_sig, _, _, _ = unpack_sign_tx_response(response)
    verify_signature(public_key, tx_hash, der_sig)

def _check_blind_signing_rejection(backend: BackendInterface, serialized_parts: list[bytes]) -> None:
    path = "m/44'/6767'/0'/0'/0'"
    client = CantonCommandSender(backend)
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_tx_in_parts(path, *serialized_parts):
            pass
    assert e.value.status == Errors.SW_INCORRECT_DATA

def test_blind_signing_disabled_go_to_settings(backend: BackendInterface, navigator: Navigator, test_name: str) -> None:
    if backend.device.is_nano:
        pytest.skip("This feature does not exist on Nano devices")
    serialized_parts = Transaction.serialize_from_json_into_tx_parts("tests/tx_examples/external_sign_ping.json")
    _check_blind_signing_rejection(backend, serialized_parts)
    navigator.navigate_until_text_and_compare(navigate_instruction=NavInsID.USE_CASE_CHOICE_CONFIRM,
                                                validation_instructions=[NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT],
                                                text="^Blind signing$",
                                                path=ROOT_SCREENSHOT_PATH,
                                                test_case_name=test_name)

def test_blind_signing_disabled_go_to_menu(
    backend: BackendInterface, navigator: Navigator, test_name: str
) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts("tests/tx_examples/external_sign_ping.json")
    if backend.device.is_nano:
        validation_instructions=[NavInsID.BOTH_CLICK]
        pattern = "Blind signing"
    else:
        validation_instructions=[NavInsID.USE_CASE_CHOICE_REJECT]
        pattern = "Enable blind signing"
    _check_blind_signing_rejection(backend, serialized_parts)
    navigator.navigate_until_text_and_compare(navigate_instruction=None,
                                                validation_instructions=validation_instructions,
                                                text=pattern,
                                                path=ROOT_SCREENSHOT_PATH,
                                                test_case_name=test_name)


def test_sign_hash_32(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario, navigator: Navigator, device: Device
) -> None:
    tx_hash = Transaction.get_hash_from_json(
        "tests/tx_examples/external_sign_ping.json"
    )
    _sign_and_verify_hash(
        backend, device, navigator, scenario_navigator, tx_hash, test_name="test_sign_hash_32"
    )


def test_sign_hash_34(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario, navigator: Navigator, device: Device
) -> None:
    tx_hash = b"\x00\x01" + Transaction.get_hash_from_json(
        "tests/tx_examples/external_sign_ping.json"
    )
    _sign_and_verify_hash(
        backend, device, navigator, scenario_navigator, tx_hash, test_name="test_sign_hash_34"
    )

def _sign_and_verify_prepared_transaction(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    tx_json: str,
    device: Optional[Device] = None,
    navigator: Optional[Navigator] = None,
    test_name: Optional[str] = None,
    custom_screen_text: Optional[str] = None,
    blind_sign: bool = False,
    snapshot_check: bool = True,
) -> None:
    client = CantonCommandSender(backend)
    path: str = "m/44'/6767'/0'/0'/0'"

    _, public_key, _, _ = unpack_get_public_key_response(client.get_public_key(path=path).data)

    serialized_parts = Transaction.serialize_from_json_into_tx_parts(tx_json)
    tx_hash = Transaction.get_hash_from_json(tx_json)
    print(f"Transaction hash: {tx_hash.hex()}")
    print(f"Serialized transaction length: {sum(len(part) for part in serialized_parts)} bytes")

    if blind_sign:
        _enable_blind_signing(device, navigator, f"{test_name}_enable_bs")

    with client.sign_tx_in_parts(path, *serialized_parts) as _:
        if blind_sign:
            scenario_navigator.review_approve_with_warning(
                path=ROOT_SCREENSHOT_PATH, custom_screen_text=custom_screen_text, do_comparison=snapshot_check)
        else:
            scenario_navigator.review_approve(
                path=ROOT_SCREENSHOT_PATH, custom_screen_text=custom_screen_text, do_comparison=snapshot_check)

    _, der_sig, _, _, _ = unpack_sign_tx_response(client.get_async_response().data)
    verify_signature(public_key, tx_hash, der_sig)

def test_sign_ping(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    device: Device,
    navigator: Navigator,
    test_name: str
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        device=device,
        navigator=navigator,
        tx_json="tests/tx_examples/external_sign_ping.json",
        blind_sign=True,
        test_name=test_name,
    )

def test_sign_hex_string_hash_error(backend: BackendInterface) -> None:
    # Load json
    with open("tests/tx_examples/external_sign_ping.json", "r", encoding="utf-8") as f:
        tx_json = f.read()
    # Load json as data object
    tx_data = json.loads(tx_json)
    # Replace contract_id value (odd length hex string)
    tx_data["prepared_transaction"]["transaction"]["nodes"][0]["v1"]["create"]["contract_id"] = (
    "004c3409aa2e8f8e22604d58ea6211f667df2bae4abc7984a95d76b3d120b8bd8ff"
    )
    tx_json_invalid = json.dumps(tx_data, indent=4)
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(tx_json_invalid)
    path = "m/44'/6767'/0'/0'/0'"
    client = CantonCommandSender(backend)
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_tx_in_parts(path, *serialized_parts):
            pass
    assert e.value.status == Errors.SW_TX_HASH_FAIL

def test_sign_max_nodes_hash_error(backend: BackendInterface) -> None:
    # Load json
    with open("tests/tx_examples/token_transfer_32_children.json", "r", encoding="utf-8") as f:
        tx_json = f.read()
    # Load json as data object
    tx_data = json.loads(tx_json)
    # Replace children value (more than 32 children)
    tx_data["json"]["transaction"]["nodes"][5]["v1"]["exercise"]["children"] = (
        ["12", "13", "14", "15", "16", "17", "18", "19", "20", "21",
         "22", "23", "24", "25", "26", "27", "28", "22", "23", "24",
         "25", "26", "27", "28", "12", "13", "14", "15", "16", "17",
         "18", "19", "29"]
    )
    tx_json_invalid = json.dumps(tx_data, indent=4)
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(tx_json_invalid)
    path = "m/44'/6767'/0'/0'/0'"
    client = CantonCommandSender(backend)
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_tx_in_parts(path, *serialized_parts):
            pass
    assert e.value.status == Errors.SW_TX_HASH_FAIL

def test_sign_native_transfer(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/native_transfer.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_cip107(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_cip107.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_lower_case(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_lower_case.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_with_memo(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_with_memo.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_32_node_children(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_32_children.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_proxy_token_transfer(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_proxy.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_proxy_cbtc_token_transfer(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_cbtc_proxy.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_accept(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_accept.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_proxy_token_transfer_accept(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_accept_proxy.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_transfer_accept_with_empty_strings(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario, device: Device, navigator: Navigator
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        navigator=navigator,
        device=device,
        tx_json="tests/tx_examples/token_transfer_accept_with_empty_strings.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_withdraw_sbc(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario, device: Device, navigator: Navigator
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        navigator=navigator,
        device=device,
        tx_json="tests/tx_examples/token_transfer_withdraw_sbc.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_reject(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_reject.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_reject_proxy(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_reject_proxy.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_withdraw(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_withdraw.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_withdraw_proxy(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_withdraw_proxy.json",
        custom_screen_text="Sign transaction to",
    )

def test_sign_token_transfer_wrong_token_admin_blind_signing_disabled(
    backend: BackendInterface, navigator: Navigator, test_name: str
) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(
        "tests/tx_examples/token_transfer_unknown_token_admin.json")
    if backend.device.is_nano:
        validation_instructions=[NavInsID.BOTH_CLICK]
        pattern = "Blind signing"
    else:
        validation_instructions=[NavInsID.USE_CASE_CHOICE_REJECT]
        pattern = "Enable blind signing"
    _check_blind_signing_rejection(backend, serialized_parts)
    navigator.navigate_until_text_and_compare(navigate_instruction=None,
                                                validation_instructions=validation_instructions,
                                                text=pattern,
                                                path=ROOT_SCREENSHOT_PATH,
                                                test_case_name=test_name)

def test_sign_token_transfer_wrong_token_id_blind_signing_enabled(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario, device: Device, navigator
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        device=device,
        navigator=navigator,
        tx_json="tests/tx_examples/token_transfer_unknown_token_id.json",
        blind_sign=True,
        test_name="test_sign_token_transfer_wrong_token_id_blind_signing_enabled",
    )


def test_sign_preapproval_proposal(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    _sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/preapproval_proposal.json",
        custom_screen_text="Sign transaction to",
    )


def _onboard_party(backend: BackendInterface,
                   scenario_navigator: NavigateWithScenario,
                   validator_uids: Optional[list[str]] = None,
                   attestation_keys: Optional[tuple[bytes,bytes]] = None,
                   der_key_format: bool = True,
                   snapshot_check: bool = True) -> None:
    client = CantonCommandSender(backend)

    if validator_uids is None:
        validator_uids = [MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_2]

    # Get public key
    _, raw_key, _, _ = unpack_get_public_key_response(
        client.get_public_key(path="m/44'/6767'/0'/0'/0'").data)

    # Convert to DER format for inclusion in topology transactions
    public_key = b"\x30\x2A\x30\x05\x06\x03\x2B\x65\x70\x03\x21\x00" + raw_key if der_key_format else raw_key

    # Create and hash transactions
    txs = [
        Transaction.namespace_delegation(public_key, der_key_format),
        Transaction.party_to_key(public_key, der_key_format),
        Transaction.party_to_participant_from_uid(public_key, validator_uids)
    ]
    multi_hash = Transaction.compute_multi_transaction_hash(
        [Transaction.compute_topology_transaction_hash(tx) for tx in txs]
    )

    # Sign transactions
    challenge = os.urandom(24) if attestation_keys else None
    with client.sign_topology_tx(path="m/44'/6767'/0'/0'/0'", transactions=txs, challenge=challenge):
        scenario_navigator.review_approve(path=ROOT_SCREENSHOT_PATH,
                                          custom_screen_text="Sign transaction to", do_comparison=snapshot_check)

    # Verify signatures
    _, der_sig, _, challenge_sig_len, challenge_sig = unpack_sign_tx_response(
        client.get_async_response().data
    )
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

def _onboard_party_expect_error(backend: BackendInterface,
                              validator_uids: Optional[list[str]] = None,
                              der_key_format: bool = True,
                              threshold: Optional[int] = None,
                              party_id: Optional[str] = None,
                              which_party_tx: Optional[WhichPartyTx] = None,
                              expected_error: int = Errors.SW_WRONG_RESPONSE_LENGTH) -> None:
    client = CantonCommandSender(backend)

    if validator_uids is None:
        validator_uids = [MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_2]

    # Get public key
    _, raw_key, _, _ = unpack_get_public_key_response(
        client.get_public_key(path="m/44'/6767'/0'/0'/0'").data)

    # Convert to DER format for inclusion in topology transactions
    public_key = b"\x30\x2A\x30\x05\x06\x03\x2B\x65\x70\x03\x21\x00" + raw_key if der_key_format else raw_key

    if which_party_tx is None:
        which_party_tx = WhichPartyTx.PARTY_TO_PARTICIPANT

    party_to_key_party_id = None
    party_to_participant_party_id = None

    if which_party_tx == WhichPartyTx.PARTY_TO_KEY:
        party_to_key_party_id = party_id
    else:
        party_to_participant_party_id = party_id

    # Create transactions
    txs = [
        Transaction.namespace_delegation(public_key, der_key_format),
        Transaction.party_to_key(public_key, der_key_format, party_to_key_party_id),
        Transaction.party_to_participant_from_uid(public_key, validator_uids, threshold, party_to_participant_party_id)
    ]

    # Sign transactions and expect error
    path = "m/44'/6767'/0'/0'/0'"
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_topology_tx(path=path, transactions=txs):
            pass
    assert e.value.status == expected_error

def test_sign_onboarding_expect_error_unexpected_participant_id(backend: BackendInterface) -> None:
    _onboard_party_expect_error(backend,
                                validator_uids=[MAINNET_VALIDATOR_PARTY_ID_1, "invalid_validator_id_2"],
                                expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID)

def test_sign_onboarding_expect_error_unexpected_participant_id_single(backend: BackendInterface) -> None:
    _onboard_party_expect_error(backend,
                                validator_uids=[DEVNET_VALIDATOR_PARTY_ID_2],
                                expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID)

def test_sign_onboarding_expect_error_unexpected_threshold(backend: BackendInterface) -> None:
    _onboard_party_expect_error(backend,
                                threshold=3,
                                expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_THRESHOLD_VALUE)

def test_sign_onboarding_expect_error_unexpected_number_of_participants(backend: BackendInterface) -> None:
    _onboard_party_expect_error(backend,
                                validator_uids=[MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_2, "extra_validator_id_3"],
                                expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_NUMBER_OF_PARTICIPANTS)

def test_sign_onboarding_expect_error_duplicate_participants(backend: BackendInterface) -> None:
    _onboard_party_expect_error(backend,
                                validator_uids=[MAINNET_VALIDATOR_PARTY_ID_1, MAINNET_VALIDATOR_PARTY_ID_1],
                                expected_error=Errors.SW_TOPOLOGY_UNEXPECTED_DUPLICATE_PARTICIPANT)

def test_sign_onboarding_expect_error_wrong_party_id_in_party_to_key(backend: BackendInterface) -> None:
    _onboard_party_expect_error(backend,
                                party_id="invalid_party_id_in_party_to_key",
                                which_party_tx=WhichPartyTx.PARTY_TO_KEY,
                                expected_error=Errors.SW_TOPOLOGY_PARTY_ID_MISMATCH)

def test_sign_onboarding_expect_error_wrong_party_id_in_party_to_participant(backend: BackendInterface) -> None:
    _onboard_party_expect_error(backend,
                                party_id="invalid_party_id_in_party_to_participant",
                                which_party_tx=WhichPartyTx.PARTY_TO_PARTICIPANT,
                                expected_error=Errors.SW_TOPOLOGY_PARTY_ID_MISMATCH)

def _verify_attestation(attest_pub_key: bytes, multi_hash: bytes, challenge: Optional[bytes],
                        challenge_sig: Optional[bytes], challenge_sig_len: int | None) -> None:
    assert challenge_sig is not None
    assert challenge_sig_len is not None
    assert challenge_sig_len == 64 == len(challenge_sig)
    assert challenge is not None
    verify_signature(attest_pub_key, multi_hash + challenge, challenge_sig)

def test_sign_onboarding_attested(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(backend, scenario_navigator, attestation_keys=(attest_key, attest_pub_key))

def test_sign_onboarding_attested_devnet_single(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(backend, scenario_navigator, attestation_keys=(attest_key, attest_pub_key), validator_uids=[DEVNET_VALIDATOR_PARTY_ID_1])

def test_sign_onboarding_attested_devnet_multi(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(backend, scenario_navigator, attestation_keys=(attest_key, attest_pub_key), validator_uids=[DEVNET_VALIDATOR_PARTY_ID_1, DEVNET_VALIDATOR_PARTY_ID_2])

def test_sign_onboarding_attested_testnet_single(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(backend, scenario_navigator, attestation_keys=(attest_key, attest_pub_key), validator_uids=[TESTNET_VALIDATOR_PARTY_ID_1])

def test_sign_onboarding_attested_testnet_multi(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    attest_key, attest_pub_key = get_keys_bytes("attestations/data/test/priv-key.pem")
    _onboard_party(backend, scenario_navigator, attestation_keys=(attest_key, attest_pub_key), validator_uids=[TESTNET_VALIDATOR_PARTY_ID_1, TESTNET_VALIDATOR_PARTY_ID_2])

def test_sign_onboarding_raw_format_key(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    _onboard_party(backend, scenario_navigator, der_key_format=False)

def test_sign_onboard_then_preapprove(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    for _ in range(10):
        _onboard_party(backend, scenario_navigator, snapshot_check=False)
        _sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/preapproval_proposal.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False
        )

def test_sign_withdraw_then_send(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    for _ in range(2):
        _sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/token_transfer_withdraw.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False,
        )
        _sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/token_transfer.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False,
        )
        _sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/token_transfer_accept.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False,
        )
