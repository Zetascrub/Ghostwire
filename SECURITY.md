# Security policy

## Supported version

Security fixes are made against the current `main` branch. Releases should be
built from a tagged commit and verified against the published SHA-256 file.

## Reporting a vulnerability

Do not publish a working exploit for Ghostwire users or disclose captured
credentials, packet captures, identifiers, or location logs in a public issue.
Contact the maintainer privately through the repository owner's published
contact channel. Include the affected commit, reproduction conditions, impact,
and the smallest safe proof of concept.

## Device security model

Ghostwire is a field tool, not a hardened credential vault. Unless a future
release explicitly enables secure boot, flash encryption, and encrypted NVS,
an attacker with physical access may read or replace firmware and preferences.
Saving Wi-Fi credentials is therefore disabled by default. SSH uses
trust-on-first-use SHA-256 host-key pinning and refuses changed keys, but its
first connection still requires the operator to validate the displayed
fingerprint through a trusted channel.

microSD logs are plaintext and may contain MAC addresses, network names,
location, packet data, and RFID identifiers. Protect, minimize, and securely
erase them according to the rules governing the assessment.
