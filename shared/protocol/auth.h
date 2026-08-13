#pragma once

// Constants for the Grove pairing handshake (X25519 ECDH -> HKDF-SHA256
// session key) and the time-windowed command authentication tag it derives.
// Both firmwares must agree on every value here exactly, or a session key
// derived on one side won't validate on the other.
//
// Pairing (documented fully in shared/protocol/README.md): the Cardputer
// sends its ephemeral X25519 public key over Grove (frame type 'P'), the P4
// replies with its own (frame type 'Q'). Each side computes the X25519
// shared secret against the other's public key, then derives a 32-byte
// session key via HKDF-SHA256(shared_secret, salt=GHOSTWIRE_AUTH_HKDF_SALT,
// info=GHOSTWIRE_AUTH_HKDF_INFO). The session key itself is never
// transmitted -- only the ephemeral public keys cross the wire, which is
// the entire point of doing a real key exchange instead of just sending a
// shared secret directly.
#define GHOSTWIRE_AUTH_HKDF_SALT "ghostwire-companion-pairing-v1"
#define GHOSTWIRE_AUTH_HKDF_INFO "ghostwire-companion-session-key-v1"
#define GHOSTWIRE_AUTH_KEY_BYTES 32

// Time-windowed command authentication tag (TOTP-style), the primitive a
// later slice (the Companion Mode command channel) will use to authenticate
// individual commands over the session key established by pairing. Not
// wired to any real command yet -- see docs/relay-phase-1-validation.md and
// shared/protocol/README.md for status.
//
// tag = truncate(HMAC-SHA256(session_key, command_bytes || window_be64),
//                GHOSTWIRE_AUTH_TAG_BYTES)
// where window = floor(unix_utc_seconds / GHOSTWIRE_AUTH_WINDOW_SECONDS),
// encoded as an 8-byte big-endian integer immediately appended to the
// command bytes before HMAC'ing.
//
// A verifier accepts a tag if it matches the HMAC recomputed for the
// current window or either adjacent window (+/- GHOSTWIRE_AUTH_WINDOW_
// TOLERANCE), tolerating up to that many window-widths of clock skew
// between the two devices' NTP-synced clocks. A captured command's tag
// stops validating once its window (plus tolerance) has passed, which is
// what makes replaying a sniffed command impossible after ~60-90s rather
// than granting standing access.
#define GHOSTWIRE_AUTH_WINDOW_SECONDS 30
#define GHOSTWIRE_AUTH_WINDOW_TOLERANCE 1
#define GHOSTWIRE_AUTH_TAG_BYTES 8
