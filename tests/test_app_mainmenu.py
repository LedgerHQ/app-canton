from pathlib import Path
from ledgered.devices import Device, DeviceType
from ragger.navigator import Navigator, NavInsID

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()


# In this test we check the behavior of the device main menu
def test_app_mainmenu(device: Device, navigator: Navigator, test_name: str) -> None:
    # Navigate in the main menu
    instructions = []
    if device.is_nano:
        instructions += [
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.RIGHT_CLICK,
            NavInsID.BOTH_CLICK,
            NavInsID.RIGHT_CLICK,
        ]
    elif device.type in [DeviceType.STAX, DeviceType.FLEX, DeviceType.APEX_P]:
        instructions += [
            NavInsID.USE_CASE_HOME_SETTINGS,
            NavInsID.USE_CASE_SETTINGS_NEXT,
            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
        ]

    assert len(instructions) > 0
    navigator.navigate_and_compare(
        ROOT_SCREENSHOT_PATH,
        test_name,
        instructions,
        screen_change_before_first_instruction=False,
    )
