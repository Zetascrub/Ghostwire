"""PlatformIO pre-build step: weaken one symbol in the precompiled
libnet80211.a so src/wifi_raw_frame_override.cpp's definition of it links
in instead of the closed-source library's. See that file for the full
rationale and the exact contract this override implements.

Idempotent: only patches the archive once (guarded by a flag file next to
it), so repeated builds don't re-weaken an already-weakened symbol or
re-copy an already-patched original aside.
"""

Import("env")  # type: ignore

from hashlib import sha256
from os.path import isfile, join

EXPECTED_ORIGINAL_SHA256 = (
    "6aae206c85e9009a24cd1f28ce535e2a2d578d9687fe2dc77d4697d45ebc8d0f"
)


def _sha256(path):
    digest = sha256()
    with open(path, "rb") as archive:
        for block in iter(lambda: archive.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
mcu = env.BoardConfig().get("build.mcu", "esp32s3")
lib_dir = join(framework_dir, "tools", "sdk", mcu, "lib")
original = join(lib_dir, "libnet80211.a")
patch_flag = join(lib_dir, ".ghostwire_wifi_patched")
backup = original + ".ghostwire_orig"

# platformio.ini deliberately keeps packages under this checkout. Refuse to
# mutate a user-wide framework installation if somebody removes that safety
# setting and invokes this script elsewhere.
project_dir = env.subst("$PROJECT_DIR")
if not framework_dir.startswith(join(project_dir, ".pio-core")):
    raise RuntimeError(
        "[wifi patch] refusing to patch non-project framework package: %s"
        % framework_dir
    )

if not isfile(patch_flag):
    if not isfile(original):
        print("[wifi patch] libnet80211.a not found at %s; skipping" % original)
    else:
        if _sha256(original) != EXPECTED_ORIGINAL_SHA256:
            raise RuntimeError(
                "[wifi patch] libnet80211.a checksum differs from the "
                "verified Arduino-ESP32 2.0.16 ESP32-S3 archive"
            )
        patched = original + ".ghostwire_patched"
        result = env.Execute(
            "pio pkg exec -p toolchain-xtensa-%s -- "
            "xtensa-%s-elf-objcopy --weaken-symbol=ieee80211_raw_frame_sanity_check "
            '"%s" "%s"' % (mcu, mcu, original, patched)
        )
        if result == 0 and isfile(patched):
            env.Execute('mv "%s" "%s.ghostwire_orig"' % (original, original))
            env.Execute('mv "%s" "%s"' % (patched, original))

            def _touch(path):
                with open(path, "w") as flag_file:
                    flag_file.write("")

            env.Execute(lambda *args, **kwargs: _touch(patch_flag))
            print("[wifi patch] weakened ieee80211_raw_frame_sanity_check in %s" % original)
        else:
            raise RuntimeError(
                "[wifi patch] objcopy failed; libnet80211.a left unmodified"
            )
else:
    if not isfile(backup) or _sha256(backup) != EXPECTED_ORIGINAL_SHA256:
        raise RuntimeError("[wifi patch] verified original archive is missing")
