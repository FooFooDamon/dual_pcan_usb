# SPDX-License-Identifier: GPL-2.0

import os, sys
YCM_CONF_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(YCM_CONF_DIR, "..", "3rdpary", "lazy_coding", "python"))
from ycm_conf_for_linux_driver import *
flags.extend([ "-I", YCM_CONF_DIR, '-DKBUILD_MODNAME="dual_pcan_usb"' ])

