# =====================================================================
# Cubi 2 - ONE FILE WITH BOTH PROGRAMS for the app's Update. (24/10/2026)
#
# It puts the built programs into  firmware/cubi2_firmware.upd  (next to the program folders):
#   cubi2_brain.hex   the Teensy's program      (Build in cubi2_brain)
#   cubi2_radio.bin   the Wi-Fi board's program (Build in cubi2_radio)
#   about.txt         the version of each one, and when it was built
# The file is a plain zip (it opens with any zip program). In the Cubi 2 app: Live Cubi > Update >
# choose cubi2_firmware.upd > Send - the Teensy first, then the Wi-Fi board.
#
# Two ways to run it (5/11/2026):
#   - by itself: PlatformIO runs it after every Build (platformio.ini: extra_scripts = post:../make_update_file.py);
#   - by hand:   python make_update_file.py  - it takes what was built last in both folders.
# Build both folders once, then only the one you changed.
# =====================================================================
import glob
import os
import re
import time
import zipfile

PROGRAMS = {"cubi2_brain": ".hex", "cubi2_radio": ".bin"}   # each program folder, and the file its Build makes
UPDATE_NAME = "cubi2_firmware.upd"


def firmware_version(firmware_folder, name):
    """the FIRMWARE_VERSION written in the program (cubi2_brain.cpp or cubi2_radio.cpp)"""
    with open(os.path.join(firmware_folder, name, name + ".cpp"), encoding="utf-8") as program:
        found = re.search(r'FIRMWARE_VERSION = "(\d+)"', program.read())
    return found.group(1) if found else "?"


def put_into_update_file(firmware_folder, name, built):
    """puts one built program into the update file; the other one stays as it was"""
    update_file = os.path.join(firmware_folder, UPDATE_NAME)
    entry = name + PROGRAMS[name]
    files, about = {}, {}
    if os.path.exists(update_file):
        try:
            with zipfile.ZipFile(update_file) as old:
                for file_name in old.namelist():
                    files[file_name] = old.read(file_name)
            for line in files.pop("about.txt", b"").decode("utf-8", "replace").splitlines():
                if " " in line:
                    file_name, what = line.split(" ", 1)
                    about[file_name] = what
        except Exception:
            files, about = {}, {}
    with open(built, "rb") as program:
        files[entry] = program.read()
    built_at = time.strftime("%Y-%m-%d %H:%M", time.localtime(os.path.getmtime(built)))   # when it was compiled
    # the line starts with the time: the app shows it after "built"
    about[entry] = "%s  %d bytes  version %s" % (built_at, len(files[entry]), firmware_version(firmware_folder, name))
    half_written = update_file + ".tmp"
    with zipfile.ZipFile(half_written, "w", zipfile.ZIP_STORED) as new:
        for file_name in sorted(files):
            new.writestr(file_name, files[file_name])
        new.writestr("about.txt", "".join("%s %s\n" % (file_name, about[file_name]) for file_name in sorted(about)))
    os.replace(half_written, update_file)          # the old file is replaced only when the new one is whole
    print("%s: %s put in (%s) - %s" % (UPDATE_NAME, entry, about[entry], update_file))


def run_by_platformio(env):
    """after a Build: PlatformIO gives 'env' (the build) - the folder, and where the program was made"""
    project = os.path.normpath(env.subst("$PROJECT_DIR"))
    firmware_folder, name = os.path.dirname(project), os.path.basename(project)
    if name not in PROGRAMS:
        return
    built = env.subst("$BUILD_DIR/${PROGNAME}" + PROGRAMS[name])

    def after_build(source, target, env):
        if os.path.exists(built):
            put_into_update_file(firmware_folder, name, built)

    # after a Build that made a new program ... (VerboseAction: the line PlatformIO shows while it does it)
    env.AddPostAction(built, env.VerboseAction(after_build, "Putting the program into %s" % UPDATE_NAME))
    after_build(None, None, env)                # ... and also when nothing had to be compiled again


def run_by_hand():
    """python make_update_file.py: the programs built last in both folders go into the update file"""
    firmware_folder = os.path.dirname(os.path.abspath(__file__))
    for name, ending in PROGRAMS.items():
        builds = sorted(glob.glob(os.path.join(firmware_folder, name, ".pio", "build", "*", "firmware" + ending)))
        if builds:
            put_into_update_file(firmware_folder, name, builds[0])
        else:
            print("%s: not built yet - open the folder in VS Code and press Build once" % name)


# PlatformIO gives this file Import and env; plain Python does not (so the editor underlines them - that is fine)
try:
    Import("env")
except NameError:
    run_by_hand()
else:
    run_by_platformio(env)
