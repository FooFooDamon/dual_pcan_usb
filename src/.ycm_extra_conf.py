import os, sys

YCM_CONF_DIR = os.path.abspath(os.path.dirname(__file__))
KERNEL_ROOT = os.path.join(os.getenv("HOME"), "src", "linux")

flags = [
    "-Wall"
    , "-std=gnu89"
    , "-x", "c"
    , "-I", YCM_CONF_DIR
    , "-I", os.path.join(YCM_CONF_DIR, "..", "3rdpary", "lazy_coding", "c_and_cpp", "native")
    , "-I", os.path.join(KERNEL_ROOT, "arch", "arm", "include")
    , "-I", os.path.join(KERNEL_ROOT, "arch", "arm", "include", "generated", "uapi")
    , "-I", os.path.join(KERNEL_ROOT, "arch", "arm", "include", "generated")
    , "-I", os.path.join(KERNEL_ROOT, "include")
    , "-I", os.path.join(KERNEL_ROOT, "arch", "arm", "include", "uapi")
    , "-I", os.path.join(KERNEL_ROOT, "include", "uapi")
    , "-I", os.path.join(KERNEL_ROOT, "include", "generated", "uapi")
    , "-include", os.path.join(KERNEL_ROOT, "include", "linux", "kconfig.h")
    , "-D__KERNEL__"
    #, "-D__ASSEMBLY__"
    , "-D__LINUX_ARM_ARCH__=7"
    , "-Uarm"
    , "-DCC_HAVE_ASM_GOTO"
    , "-DMODULE"
    , '-DKBUILD_MODNAME="dual_pcan_usb"'
]

SOURCE_EXTENSIONS = [ ".c" ]

def FlagsForFile(filename, **kwargs):
    return { "flags": flags, "do_cache": True }

