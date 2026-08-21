from pathlib import Path
import pytest
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
from ragger.error import ExceptionRAPDU
from ragger.backend.interface import BackendInterface
from ragger.navigator.navigation_scenario import NavigateWithScenario

from application_client.canton_command_sender import CantonCommandSender, Errors
from application_client.canton_response_unpacker import unpack_get_public_key_response

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()


# In this test we check that the GET_PUBLIC_KEY works in non-confirmation mode
def test_get_public_key_no_confirm(backend: BackendInterface) -> None:
    path_list = [
        "m/44'/6767'/0'/0'/0'",
        "m/44'/6767'/911'/0'/0'",
        "m/44'/6767'/255'/255'/255'",
        "m/44'/6767'/2147483647'/0'/0'/0'/0'/0'/0'",
    ]
    for path in path_list:
        client = CantonCommandSender(backend)
        response = client.get_public_key(path=path).data
        _, public_key, _, chain_code = unpack_get_public_key_response(response)

        ref_public_key, ref_chain_code = calculate_public_key_and_chaincode(CurveChoice.Ed25519Slip, path=path)
        assert public_key.hex() == ref_public_key[2:]
        assert chain_code.hex() == ref_chain_code


# In this test we check that the GET_PUBLIC_KEY works in confirmation mode
def test_get_public_key_confirm_accepted(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    client = CantonCommandSender(backend)
    path = "m/44'/6767'/0'/0'/0'"
    with client.get_public_key_with_confirmation(path=path):
        scenario_navigator.address_review_approve()

    response = client.get_async_response().data
    _, public_key, _, chain_code = unpack_get_public_key_response(response)

    ref_public_key, ref_chain_code = calculate_public_key_and_chaincode(CurveChoice.Ed25519Slip, path=path)
    assert public_key.hex() == ref_public_key[2:]
    assert chain_code.hex() == ref_chain_code


# In this test we check that the GET_PUBLIC_KEY in confirmation mode replies an error if the user refuses
def test_get_public_key_confirm_refused(backend: BackendInterface, scenario_navigator: NavigateWithScenario) -> None:
    client = CantonCommandSender(backend)
    path = "m/44'/6767'/0'/0'/0'"

    with pytest.raises(ExceptionRAPDU) as e:
        with client.get_public_key_with_confirmation(path=path):
            scenario_navigator.address_review_reject()

    # Assert that we have received a refusal
    assert e.value.status == Errors.SW_DENY
    assert len(e.value.data) == 0
