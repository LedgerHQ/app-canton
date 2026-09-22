"""Helpers shared by the signing and onboarding test modules."""

from pathlib import Path
from typing import Optional

from ragger.backend.interface import BackendInterface
from ragger.navigator.navigation_scenario import NavigateWithScenario
from ragger.navigator import NavInsID, Navigator, NavIns
from ledgered.devices import Device, DeviceType

from application_client.canton_transaction import Transaction
from application_client.canton_command_sender import CantonCommandSender
from application_client.canton_response_unpacker import (
    unpack_get_public_key_response,
    unpack_sign_tx_response,
)
from utils import verify_signature

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()

MAINNET_VALIDATOR_PARTY_ID_1 = (
    "ledger-ledgerops-2::12207a4859ad414f4f47c2d773ddf4ea88de8c3a1aab19abaa197e504acdbf679d3c"
)
MAINNET_VALIDATOR_PARTY_ID_2 = "Ledger-Kiln-2::1220e2225d5a297fae4000be2e3ca560ce802461da04c8e3e40c9fbbf0547f4fe8e3"

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


def nanoenable_blind_signing() -> list[NavInsID]:
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


def enable_blind_signing(device: Device, navigator: Navigator, snapshots_name: str) -> None:
    if device.is_nano:
        nav = nanoenable_blind_signing()
    else:
        if device.type is DeviceType.APEX_P:
            coordinates = (263, 95)
        else:
            coordinates = (348, 132)
        nav = [
            NavInsID.USE_CASE_HOME_SETTINGS,
            NavIns(NavInsID.TOUCH, coordinates),
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
        ]
    navigator.navigate_and_compare(
        ROOT_SCREENSHOT_PATH,
        snapshots_name,
        nav,
        screen_change_before_first_instruction=False,
    )


def sign_and_verify_prepared_transaction(
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
        enable_blind_signing(device, navigator, f"{test_name}_enable_bs")

    with client.sign_tx_in_parts(path, *serialized_parts) as _:
        if blind_sign:
            scenario_navigator.review_approve_with_warning(
                path=ROOT_SCREENSHOT_PATH,
                custom_screen_text=custom_screen_text,
                do_comparison=snapshot_check,
            )
        else:
            scenario_navigator.review_approve(
                path=ROOT_SCREENSHOT_PATH,
                custom_screen_text=custom_screen_text,
                do_comparison=snapshot_check,
            )

    _, der_sig, _, _, _ = unpack_sign_tx_response(client.get_async_response().data)
    verify_signature(public_key, tx_hash, der_sig)
