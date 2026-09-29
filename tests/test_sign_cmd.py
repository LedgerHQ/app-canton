import json
import pytest
from ragger.backend.interface import BackendInterface
from ragger.error import ExceptionRAPDU
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import NavInsID, Navigator
from ledgered.devices import Device

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

from signing_flows import (
    ROOT_SCREENSHOT_PATH,
    enable_blind_signing,
    sign_and_verify_prepared_transaction,
)


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

    enable_blind_signing(device, navigator, f"{test_name}_enable_bs")

    rapdu = client.get_public_key(path=path)
    _, public_key, _, _ = unpack_get_public_key_response(rapdu.data)

    print(f"Public key returned from device: {public_key.hex()}")

    with client.sign_tx(path=path, transaction=tx_hash, p1=P1SignType.P1_SIGN_HASH):
        scenario_navigator.review_approve_with_warning(path=ROOT_SCREENSHOT_PATH, test_name=test_name)

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
    navigator.navigate_until_text_and_compare(
        navigate_instruction=NavInsID.USE_CASE_CHOICE_CONFIRM,
        validation_instructions=[NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT],
        text="^Blind signing$",
        path=ROOT_SCREENSHOT_PATH,
        test_case_name=test_name,
    )


def test_blind_signing_disabled_go_to_menu(backend: BackendInterface, navigator: Navigator, test_name: str) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts("tests/tx_examples/external_sign_ping.json")
    if backend.device.is_nano:
        validation_instructions = [NavInsID.BOTH_CLICK]
        pattern = "Blind signing"
    else:
        validation_instructions = [NavInsID.USE_CASE_CHOICE_REJECT]
        pattern = "Enable blind signing"
    _check_blind_signing_rejection(backend, serialized_parts)
    navigator.navigate_until_text_and_compare(
        navigate_instruction=None,
        validation_instructions=validation_instructions,
        text=pattern,
        path=ROOT_SCREENSHOT_PATH,
        test_case_name=test_name,
    )


def test_sign_hash_32(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    navigator: Navigator,
    device: Device,
) -> None:
    tx_hash = Transaction.get_hash_from_json("tests/tx_examples/external_sign_ping.json")
    _sign_and_verify_hash(
        backend,
        device,
        navigator,
        scenario_navigator,
        tx_hash,
        test_name="test_sign_hash_32",
    )


def test_sign_hash_34(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    navigator: Navigator,
    device: Device,
) -> None:
    tx_hash = b"\x00\x01" + Transaction.get_hash_from_json("tests/tx_examples/external_sign_ping.json")
    _sign_and_verify_hash(
        backend,
        device,
        navigator,
        scenario_navigator,
        tx_hash,
        test_name="test_sign_hash_34",
    )


def test_sign_ping(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    device: Device,
    navigator: Navigator,
    test_name: str,
) -> None:
    sign_and_verify_prepared_transaction(
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
    tx_data["json"]["transaction"]["nodes"][5]["v1"]["exercise"]["children"] = [
        "12",
        "13",
        "14",
        "15",
        "16",
        "17",
        "18",
        "19",
        "20",
        "21",
        "22",
        "23",
        "24",
        "25",
        "26",
        "27",
        "28",
        "22",
        "23",
        "24",
        "25",
        "26",
        "27",
        "28",
        "12",
        "13",
        "14",
        "15",
        "16",
        "17",
        "18",
        "19",
        "29",
    ]
    tx_json_invalid = json.dumps(tx_data, indent=4)
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(tx_json_invalid)
    path = "m/44'/6767'/0'/0'/0'"
    client = CantonCommandSender(backend)
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_tx_in_parts(path, *serialized_parts):
            pass
    assert e.value.status == Errors.SW_TX_HASH_FAIL


# Node trees that the device refuses during the tree check itself
TREE_CHECK_REFUSED_TX = [
    "tree_err_node_id_out_of_range",
    "tree_err_duplicate_node",
    "tree_err_child_out_of_range",
    "tree_err_self_child",
    "tree_err_root_claimed",
    "tree_err_duplicate_claim",
]

# Node trees that the device cannot display, so it falls back to blind signing
TREE_CHECK_BLIND_SIGNING_TX = [
    "tree_err_root_count",
    "tree_err_orphan_node",
    "tree_err_too_many_nodes",
]


@pytest.mark.parametrize("tx_name", TREE_CHECK_REFUSED_TX, ids=TREE_CHECK_REFUSED_TX)
def test_sign_invalid_node_tree_error(backend: BackendInterface, tx_name: str) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(f"tests/tx_examples/{tx_name}.json")
    path = "m/44'/6767'/0'/0'/0'"
    client = CantonCommandSender(backend)
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_tx_in_parts(path, *serialized_parts):
            pass
    assert e.value.status == Errors.SW_TX_INVALID_NODE_TREE


@pytest.mark.parametrize("tx_name", TREE_CHECK_BLIND_SIGNING_TX, ids=TREE_CHECK_BLIND_SIGNING_TX)
def test_sign_invalid_node_tree_blind_signing_disabled(backend: BackendInterface, tx_name: str) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(f"tests/tx_examples/{tx_name}.json")
    _check_blind_signing_rejection(backend, serialized_parts)


# Nodes 4 and 5 claim each other. Every node still has exactly one parent, so the tree check finds
# nothing wrong, and neither node hangs off the root. A parent hashes its children's hashes, and a
# cycle has no order in which both children arrive before their parent, so the child hash lookup
# fails and the transaction is refused. The tree check does not have to prove reachability itself.
def test_sign_node_tree_cycle(backend: BackendInterface) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts("tests/tx_examples/tree_err_cycle.json")
    path = "m/44'/6767'/0'/0'/0'"
    client = CantonCommandSender(backend)
    with pytest.raises(ExceptionRAPDU) as e:
        with client.sign_tx_in_parts(path, *serialized_parts):
            pass
    assert e.value.status == Errors.SW_TX_HASH_FAIL


def test_sign_native_transfer(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/native_transfer.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer.json",
        custom_screen_text="Sign transaction to",
    )


# Consolidating your own holdings is a transfer to yourself. It writes two holdings to the one
# account, the amount moved and the change, so the value check has to allow that case.
def test_sign_token_transfer_consolidate(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_consolidate.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_consolidate_v2(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_consolidate_v2.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_v2(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_v2.json",
        custom_screen_text="Sign transaction to",
    )


# A Token Standard V2 transfer that creates the transfer instruction in one transaction and settles
# it in another. The displayed action is still TransferFactory_Transfer.
def test_sign_token_transfer_multistep_v2(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_multistep_v2.json",
        custom_screen_text="Sign transaction to",
    )


# A CBTC send: the registry runs the transfer on its own factory contract and parks the money in
# its own TransferOffer, rather than using Canton Coin's contracts. Reconstructed from a device
# APDU log after a real transfer was refused on 3.4.0.
def test_sign_token_transfer_cbtc_send(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_cbtc_send.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_cip107(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_cip107.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_lower_case(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_lower_case.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_with_memo(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_with_memo.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_32_node_children(
    backend: BackendInterface, scenario_navigator: NavigateWithScenario
) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_32_children.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_proxy_token_transfer_blind_signing_disabled(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    device: Device,
    test_name: str,
    navigator: Navigator,
) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        navigator=navigator,
        device=device,
        test_name=test_name,
        tx_json="tests/tx_examples/token_transfer_proxy.json",
        blind_sign=True,
    )


def test_sign_token_transfer_accept(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_accept.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_accept_v2(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_accept_v2.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_usdcx_send(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_usdcx_send.json",
        custom_screen_text="Sign transaction to",
    )


# A memo too long to copy for the screen must stop clear signing, not vanish from the screen.
# 700 characters decodes but does not fit the copy; retune if heap use changes.
def test_sign_memo_too_long_blind_signing_disabled(backend: BackendInterface) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(
        "tests/tx_examples/token_transfer_memo_too_long.json"
    )
    _check_blind_signing_rejection(backend, serialized_parts)


def test_sign_token_transfer_usdcx_accept(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_usdcx_accept.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_cbtc_accept(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_cbtc_accept.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_accept_wrong_metadata_contract_id_blind_signing_disabled(
    backend: BackendInterface, navigator: Navigator, test_name: str
) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(
        "tests/tx_examples/token_transfer_accept_wrong_metadata_contract_id.json"
    )
    if backend.device.is_nano:
        validation_instructions = [NavInsID.BOTH_CLICK]
        pattern = "Blind signing"
    else:
        validation_instructions = [NavInsID.USE_CASE_CHOICE_REJECT]
        pattern = "Enable blind signing"
    _check_blind_signing_rejection(backend, serialized_parts)
    navigator.navigate_until_text_and_compare(
        navigate_instruction=None,
        validation_instructions=validation_instructions,
        text=pattern,
        path=ROOT_SCREENSHOT_PATH,
        test_case_name=test_name,
    )


def test_sign_transfer_accept_with_empty_strings(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    device: Device,
    navigator: Navigator,
) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        navigator=navigator,
        device=device,
        tx_json="tests/tx_examples/token_transfer_accept_with_empty_strings.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_withdraw_sbc(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    device: Device,
    navigator: Navigator,
) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        navigator=navigator,
        device=device,
        tx_json="tests/tx_examples/token_transfer_withdraw_sbc.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_reject(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_reject.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_reject_v2(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_reject_v2.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_withdraw(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_withdraw.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_withdraw_v2(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/token_transfer_withdraw_v2.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_token_transfer_wrong_token_admin_blind_signing_disabled(
    backend: BackendInterface, navigator: Navigator, test_name: str
) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(
        "tests/tx_examples/token_transfer_unknown_token_admin.json"
    )
    if backend.device.is_nano:
        validation_instructions = [NavInsID.BOTH_CLICK]
        pattern = "Blind signing"
    else:
        validation_instructions = [NavInsID.USE_CASE_CHOICE_REJECT]
        pattern = "Enable blind signing"
    _check_blind_signing_rejection(backend, serialized_parts)
    navigator.navigate_until_text_and_compare(
        navigate_instruction=None,
        validation_instructions=validation_instructions,
        text=pattern,
        path=ROOT_SCREENSHOT_PATH,
        test_case_name=test_name,
    )


def test_sign_token_transfer_wrong_token_id_blind_signing_enabled(
    backend: BackendInterface,
    scenario_navigator: NavigateWithScenario,
    device: Device,
    navigator,
) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        device=device,
        navigator=navigator,
        tx_json="tests/tx_examples/token_transfer_unknown_token_id.json",
        blind_sign=True,
        test_name="test_sign_token_transfer_wrong_token_id_blind_signing_enabled",
    )


# Canton keeps no balance: each amount owned is its own contract naming an owner and an amount, and
# the app sees one as a create node. Each fixture below is token_transfer.json with exactly one line
# changed, so the screen still promises 20 CC to bob while the holdings say otherwise. Clear signing
# has to drop, which leaves blind signing as the only route and makes the transaction refusable.
@pytest.mark.parametrize(
    "tx_name",
    [
        # bob's holding has 21 while the screen shows 20
        "values_err_receiver_amount",
        # bob's holding was issued by a party the displayed ticker was not resolved from
        "values_err_holding_admin",
        # the holding went to a third account, so nothing is written for the receiver shown
        "values_err_no_holding",
        # the change holding goes to bob as well, so bob is paid 20 CC and 67.79 CC on top. The
        # screen still says 20, and the amounts are digests that cannot be added up, so a second
        # holding for the receiver has to drop clear signing.
        "values_err_extra_holding",
        # One create is turned into a pre-approval proposal, which the display parser reaches before
        # the transfer node, so the screen offers a pre-approval while the transfer still pays bob
        # 20 CC. A pre-approval moves nothing, so any holding at all has to drop clear signing.
        "values_err_preapproval_holding",
        # bob's holding pays 21 and hides decoy owner, dso and amount fields one level down, behind a
        # record field that carries no label. A label is only pushed onto the path when it is there,
        # so the pop that follows the field must not remove a level the push never added, or the
        # decoys are read as the holding's own fields and 20 looks correct.
        "values_err_unlabelled_field",
        # The same decoys, this time behind a label too long to fit in the path. A label that cannot
        # be written down can never equal a configured path, so the holding has to read as unknown.
        "values_err_overlong_label",
        # bob still gets 20 CC, but the change goes to carol instead of alice. The screen names only
        # alice and bob, so a holding for anyone else is value the user was never shown.
        "values_err_third_party_holding",
        # token_transfer_cbtc_send.json with every created holding set to instrument CBTX while the
        # screen still says CBTC. Both come from the same issuer, so only the instrument id tells
        # them apart.
        "values_err_holding_instrument",
        # A third holding, same owner, different amount, on top of a real consolidation
        "values_err_consolidate_extra_holding",
        # A native CC transfer with an extra token holding spliced in for the same receiver
        "values_err_native_hides_token_holding",
    ],
)
def test_sign_values_mismatch_blind_signing_disabled(backend: BackendInterface, tx_name: str) -> None:
    serialized_parts = Transaction.serialize_from_json_into_tx_parts(f"tests/tx_examples/{tx_name}.json")
    _check_blind_signing_rejection(backend, serialized_parts)


def test_sign_preapproval_proposal(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    sign_and_verify_prepared_transaction(
        backend,
        scenario_navigator,
        tx_json="tests/tx_examples/preapproval_proposal.json",
        custom_screen_text="Sign transaction to",
    )


def test_sign_withdraw_then_send(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    for _ in range(2):
        sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/token_transfer_withdraw.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False,
        )
        sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/token_transfer.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False,
        )
        sign_and_verify_prepared_transaction(
            backend,
            scenario_navigator,
            tx_json="tests/tx_examples/token_transfer_accept.json",
            custom_screen_text="Sign transaction to",
            snapshot_check=False,
        )
