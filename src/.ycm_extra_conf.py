# SPDX-License-Identifier: GPL-2.0

import os, sys
YCM_CONF_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(YCM_CONF_DIR, "..", "3rdpary", "lazy_coding", "python"))
from ycm_conf_for_c_and_cpp import flags as app_flags
import ycm_conf_for_linux_driver

APP_SRC_BASENAMES = [
    "setting_app",
]

def FlagsForFile(filename: str, **kwargs):
#{#
    base_name = os.path.splitext(os.path.basename(filename))[0]
    driver_flags = ycm_conf_for_linux_driver.flags

    return { "flags": app_flags if base_name in APP_SRC_BASENAMES else driver_flags }
#}#

ycm_conf_for_linux_driver.FlagsForFile = FlagsForFile

Settings = ycm_conf_for_linux_driver.Settings

