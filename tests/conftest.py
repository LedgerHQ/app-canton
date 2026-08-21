###########################
### CONFIGURATION START ###
###########################

import sys
from pathlib import Path

sys.path.append(f"{Path(__file__).parent.parent.resolve()}/proto")
sys.path.append(f"{Path(__file__).parent.parent.resolve()}/scripts")
sys.path.append(f"{Path(__file__).parent.parent.resolve()}/attestations")

# You can configure optional parameters by overriding the value of ragger.configuration.OPTIONAL_CONFIGURATION
# Please refer to ragger/conftest/configuration.py for their descriptions and accepted values

#########################
### CONFIGURATION END ###
#########################

# Pull all features from the base ragger conftest using the overridden configuration
pytest_plugins = ("ragger.conftest.base_conftest",)
