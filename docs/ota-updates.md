# Firmware OTA updates

Ghostwire can check the public GitHub repository for a newer signed release
and install it over Wi-Fi, from **Settings > Firmware Update**. This
document covers how the mechanism works, what it does and doesn't protect
against, and what to do if it needs adjusting later. See
`docs/roadmap.md` item 11 for the product-level scope and rationale.

## How it works

1. **Check** (`OtaService::checkForUpdate`): queries
   `api.github.com/repos/Zetascrub/Ghostwire/releases/latest`, compares the
   release tag against the running `Branding::version`
   (`include/ota_version.h` does the numeric comparison; a `-dev` suffix on
   the running version is ignored, so "0.4.5-dev" and a "v0.4.5" release
   compare equal, not newer).
2. **Confirm**: the operator reviews the version/size and explicitly
   confirms before anything is downloaded. There is no silent/unattended
   path.
3. **Install** (`OtaService::downloadAndInstall`): streams the release's
   `firmware.bin` asset straight into the inactive OTA partition via
   `Update.write()`, computing a running SHA-256 digest as bytes arrive.
   Nothing is buffered in RAM (the Cardputer ADV has no PSRAM) and nothing
   is committed to flash as bootable yet.
4. **Verify**: the release's small `.sig` asset (an ECDSA-P256 signature
   over the SHA-256 digest) is checked against the public key embedded in
   firmware (`include/ota_signing_key.h`) via mbedTLS. Only if this passes
   does `Update.end(true)` commit the partition and mark it bootable. An
   unsigned or tampered image is rejected before that point -- the bytes
   may have been written to the inactive partition's flash region, but the
   bootloader is never pointed at it.
5. **Reboot and self-check**: see "Boot safety" below.

## Why the partition table needed no changes

`platformio.ini` already builds against `default_8MB.csv`, which provisions
`app0`/`app1` (`ota_0`/`ota_1`, 0x330000 bytes each) and `otadata`. Every
device flashed via USB already has a free, unused OTA slot sitting there --
this is why OTA didn't need the kind of one-time migration reflash the
earlier SD/device-encryption proposal would have.

## Release signing

- Signing key: ECDSA P-256 (not Ed25519 -- the bundled mbedTLS build
  (2.28.7) has no `MBEDTLS_ED25519`/`MBEDTLS_EDDSA` support compiled in,
  confirmed by checking `tools/sdk/esp32s3/.../mbedtls/config.h` directly;
  P-256 is solidly supported since it's what TLS itself uses).
- The private key lives only as the `RELEASE_SIGNING_PRIVATE_KEY` GitHub
  Actions secret on the `Zetascrub/Ghostwire` repo, used by
  `.github/workflows/release.yml`'s "Sign firmware for OTA" step. It is not
  in this repository. The maintainer holds an offline backup.
- The public key (`include/ota_signing_key.h`) is the DER-encoded
  SubjectPublicKeyInfo, safe to commit -- it's public by design.
- **Rotating the key** requires shipping a new firmware build over USB with
  the new public key embedded. There is deliberately no OTA-based way to
  change it: an OTA-driven key rotation would let a compromised key rotate
  itself.

## Pinned root CAs

`include/ota_root_certs.h` pins two root CAs (not leaf/intermediate certs,
so Let's Encrypt's frequent intermediate rotation doesn't break this) as one
combined trust bundle:

- `api.github.com` (the Releases API) chains to Sectigo's USERTrust ECC
  root.
- The API's asset-download redirect target (`release-assets.githubusercontent.com`
  at the time this was written) chains to Let's Encrypt's ISRG Root X1.

Both were verified live (`openssl s_client -connect <host>:443 -showcerts`)
on 2026-08-05. If update checks start failing with a TLS trust error,
re-verify with the same command -- a root CA migration (GitHub has changed
release-asset hosting providers before) is the one thing pinning specific
roots doesn't protect against on its own, unlike intermediate rotation.

## Boot safety

Two independent, layered checks run in `setup()`
(`verifyOtaBootOrRollback()`/`markBootHealthy()`):

1. **ESP-IDF's native pending-verify mechanism**, if the bootloader has app
   rollback enabled. This board's bootloader configuration for that hasn't
   been explicitly confirmed either way -- the code checks
   `esp_ota_get_state_partition()` at runtime and only acts if the running
   partition actually reports `ESP_OTA_IMG_PENDING_VERIFY`, so it's a safe
   no-op either way. If enabled, this is the strongest guarantee: it
   catches a crash at essentially any point after boot.
2. **An app-level boot-attempt counter in NVS**, independent of whether (1)
   is active. Increments on every boot; a boot that reaches
   `markBootHealthy()` (SD/display initialized, main menu drawn) resets it
   to zero. After `kMaxBootAttemptsBeforeRollback` (3) consecutive boots
   that never reached that point, `setup()` points the bootloader at the
   other OTA partition and restarts. This catches the realistic common
   failure mode -- new firmware boots far enough to run `setup()` but
   hangs, watchdog-resets, or panics before reaching a healthy checkpoint.

**What this doesn't fully solve:** if new firmware panics before a single
instruction of `verifyOtaBootOrRollback()` runs (e.g. a corrupted image that
crashes in the ROM bootloader itself), only mechanism (1) can catch it, and
only if bootloader rollback is actually enabled on this board -- which
hasn't been verified with a real hardware test yet. **Before relying on
this in the field, deliberately flash a broken build and confirm the device
recovers on its own** rather than trusting this document's description of
the intended behavior.

## Known gaps / follow-up work

- The automatic "check once Wi-Fi connects" hook described in roadmap item
  10 is not wired up yet -- today the check only runs from the explicit
  **Settings > Firmware Update** menu action. `setup()` already has an
  `autoConnectWifi` block that's the natural place to add it once item 10
  itself is built.
- No release has been published yet (`git ls-remote --tags origin` is
  empty), so the full check-confirm-download-verify-flash path has only
  been exercised as far as the "no releases published yet" response --
  cutting a real `vX.Y.Z` tag is the next step to test the rest of it for
  real, including the boot-safety recovery test above.
