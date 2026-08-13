#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/temperature_sensor.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/errno.h"
#include "lwip/sockets.h"
#include "cJSON.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ping/ping_sock.h"
#include "auth.h"
#include "grove_link.h"
#include "scan_ports.h"

#define COMPANION_FIRMWARE_VERSION "0.4.1"
#define COMPANION_HOSTNAME "ghostwire-poe-p4"
#define COMPANION_INSTANCE "Ghostwire Unit PoE-P4"
#define COMPANION_HTTP_PORT 8765

#define POE_P4_PHY_ADDR 1
#define POE_P4_PHY_RESET_GPIO 51
#define POE_P4_MDC_GPIO 31
#define POE_P4_MDIO_GPIO 52

#define POE_P4_LED_GREEN_GPIO 15
#define POE_P4_LED_BLUE_GPIO 16
#define POE_P4_LED_RED_GPIO 17
#define INTERNET_PROBE_INTERVAL_MS 30000
#define GHOSTWIRE_ACTIVE_WINDOW_MS 30000
#define POE_P4_GROVE_TX_GPIO 53
#define POE_P4_GROVE_RX_GPIO 54
#define GROVE_LINK_TIMEOUT_MS 3000

// User button. Confirmed against M5Stack's own reference firmware for this
// board (m5stack/M5Unit-PoE-P4-UserDemo, main/app_gpio/user_key.cpp): active
// low with an internal pull-up. Their firmware's "hold 3s for download mode"
// behavior lives in their application code, not in hardware/ROM, so it does
// not apply to this firmware -- GPIO45 is ours to define.
#define POE_P4_BUTTON_GPIO 45
#define PAYLOAD_SHORT_PRESS_MAX_MS 1500
#define PAYLOAD_LONG_PRESS_MAX_MS 5000
#define PAYLOAD_RESULT_HOLD_MS 3000
// Caps an uploaded slot script (Grove or Wi-Fi) -- generous for the
// line-oriented DuckyScript-style vocabulary this interpreter supports,
// tiny against the 24KB NVS partition (2 slots x this = 1KB, next to the
// 32-byte session key already stored there).
#define PAYLOAD_SCRIPT_MAX_BYTES 512

#define MAX_WS_CLIENTS 4
#define STATUS_JSON_SIZE 1280
#define EVENT_JSON_SIZE 256

typedef struct {
    bool started;
    bool link_up;
    bool has_ip;
    bool internet_reachable;
    bool firmware_ready;
    uint32_t link_speed_mbps;
    bool full_duplex;
    int64_t last_ghostwire_contact_us;
    int64_t last_grove_ack_us;
    uint32_t grove_sequence;
    uint32_t grove_valid_frames;
    uint32_t grove_crc_errors;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char dns[16];
} ethernet_state_t;

static const char *TAG = "ghostwire_poe_p4";
static ethernet_state_t s_ethernet = {0};
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_eth_handle_t s_eth_handle;
static esp_netif_t *s_eth_netif;
static httpd_handle_t s_http_server;
static temperature_sensor_handle_t s_temperature_sensor;
static int s_ws_clients[MAX_WS_CLIENTS] = {-1, -1, -1, -1};
static uint32_t s_event_sequence;
static char s_device_id[32] = "poe-p4-unknown";

static void broadcast_event(const char *type, const char *data_json);

typedef enum {
    LED_STATE_BOOTING,
    LED_STATE_FAULT,
    LED_STATE_READY,
    LED_STATE_LAN,
    LED_STATE_INTERNET,
    LED_STATE_GHOSTWIRE,
} companion_led_state_t;

static const char *led_state_name(companion_led_state_t state)
{
    switch (state) {
        case LED_STATE_BOOTING: return "booting";
        case LED_STATE_FAULT: return "fault";
        case LED_STATE_READY: return "ready";
        case LED_STATE_LAN: return "lan";
        case LED_STATE_INTERNET: return "internet";
        case LED_STATE_GHOSTWIRE: return "ghostwire";
        default: return "unknown";
    }
}

static void copy_ipv4(char destination[16], const esp_ip4_addr_t *address)
{
    snprintf(destination, 16, IPSTR, IP2STR(address));
}

static ethernet_state_t ethernet_state_snapshot(void)
{
    ethernet_state_t snapshot;
    portENTER_CRITICAL(&s_state_lock);
    snapshot = s_ethernet;
    portEXIT_CRITICAL(&s_state_lock);
    return snapshot;
}

static companion_led_state_t current_led_state(const ethernet_state_t *state)
{
    const int64_t contact_age_us = esp_timer_get_time() - state->last_ghostwire_contact_us;
    if (!state->firmware_ready) return LED_STATE_BOOTING;
    if (!state->started) return LED_STATE_FAULT;
    if (state->last_ghostwire_contact_us > 0 &&
        contact_age_us < GHOSTWIRE_ACTIVE_WINDOW_MS * 1000LL) {
        return LED_STATE_GHOSTWIRE;
    }
    if (state->internet_reachable) return LED_STATE_INTERNET;
    if (state->has_ip) return LED_STATE_LAN;
    return LED_STATE_READY;
}

// Payload engine: the button-triggered run state overrides the connectivity
// LED above with a traffic-light pattern (amber while running, green/red on
// completion) for PAYLOAD_RESULT_HOLD_MS, then reverts automatically. The
// registry/dispatch and the payloads themselves are defined further down,
// near probe_internet(); the state itself is declared here, ahead of
// led_task(), so the LED override reads cleanly alongside the connectivity
// state it takes priority over.
typedef enum {
    PAYLOAD_STATE_IDLE,
    PAYLOAD_STATE_RUNNING,
    PAYLOAD_STATE_SUCCESS,
    PAYLOAD_STATE_ERROR,
} payload_run_state_t;

static payload_run_state_t s_payload_state = PAYLOAD_STATE_IDLE;
static int64_t s_payload_hold_until_us = 0;
// Declared here (ahead of grove_send_status(), which reports it) rather
// than alongside the scan_finding_t array it counts, further down near the
// scan payload itself -- the array needs that type in scope, the count
// doesn't.
static size_t s_scan_finding_count;

// Unlike s_scan_finding_count above (reset every PORT_SCAN run, just a
// per-run count for telemetry), this accumulates across every payload run
// since boot -- what "Extract loot" on the Cardputer actually pulls.
// RAM-only by design (not NVS-persisted): it changes on every scan, and the
// 24KB NVS partition/write-wear budget isn't worth spending on data that's
// meant to be extracted promptly, not survive a power cycle. Declared this
// early (rather than alongside scan_finding_t/s_scan_findings, further down
// near the scan payload itself) because process_grove_loot_request() --
// grouped with the other Grove frame handlers, well above the payload
// engine section -- needs it in scope.
#define LOOT_MAX_ENTRIES 64
typedef struct {
    char ip[16];
    uint16_t port;
} loot_entry_t;
static loot_entry_t s_loot_entries[LOOT_MAX_ENTRIES];
static size_t s_loot_entry_count;  // guarded by s_state_lock, like s_scan_finding_count

// Records one open-port finding into the accumulating loot log, deduped
// against everything already recorded since boot. Silently drops once full
// rather than evicting -- LOOT_MAX_ENTRIES is generous for what a couple of
// common-port scans actually turn up, and "extract before it fills up" is a
// simpler contract than an eviction policy nobody asked for.
static void loot_record(const char *ip, uint16_t port)
{
    portENTER_CRITICAL(&s_state_lock);
    for (size_t i = 0; i < s_loot_entry_count; ++i) {
        if (s_loot_entries[i].port == port && strcmp(s_loot_entries[i].ip, ip) == 0) {
            portEXIT_CRITICAL(&s_state_lock);
            return;
        }
    }
    if (s_loot_entry_count < LOOT_MAX_ENTRIES) {
        strncpy(s_loot_entries[s_loot_entry_count].ip, ip,
                sizeof(s_loot_entries[s_loot_entry_count].ip) - 1);
        s_loot_entries[s_loot_entry_count]
            .ip[sizeof(s_loot_entries[s_loot_entry_count].ip) - 1] = '\0';
        s_loot_entries[s_loot_entry_count].port = port;
        ++s_loot_entry_count;
    }
    portEXIT_CRITICAL(&s_state_lock);
}

// Reads the payload run state and, if a completed run's display hold has
// expired, reverts it to IDLE as a side effect -- called once per LED frame
// so the traffic light clears on its own without a dedicated timer task.
static payload_run_state_t payload_state_snapshot_and_expire(void)
{
    payload_run_state_t state;
    portENTER_CRITICAL(&s_state_lock);
    state = s_payload_state;
    if (state != PAYLOAD_STATE_IDLE && state != PAYLOAD_STATE_RUNNING &&
        esp_timer_get_time() >= s_payload_hold_until_us) {
        s_payload_state = PAYLOAD_STATE_IDLE;
        state = PAYLOAD_STATE_IDLE;
    }
    portEXIT_CRITICAL(&s_state_lock);
    return state;
}

static void set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    // The onboard LED is common-anode, so PWM duty is inverted.
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255 - red);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 255 - green);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 255 - blue);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

static void led_task(void *argument)
{
    (void)argument;
    uint8_t pulse = 16;
    int8_t pulse_step = 8;
    while (true) {
        // A running/just-completed payload takes priority over the normal
        // connectivity indicator -- traffic-light amber/green/red.
        const payload_run_state_t payload_state = payload_state_snapshot_and_expire();
        if (payload_state != PAYLOAD_STATE_IDLE) {
            switch (payload_state) {
                case PAYLOAD_STATE_RUNNING: set_rgb(80, 48, 0); break;  // amber
                case PAYLOAD_STATE_SUCCESS: set_rgb(0, 80, 0); break;   // green
                case PAYLOAD_STATE_ERROR: set_rgb(80, 0, 0); break;     // red
                case PAYLOAD_STATE_IDLE: break;
            }
            vTaskDelay(pdMS_TO_TICKS(80));
            continue;
        }
        const ethernet_state_t state = ethernet_state_snapshot();
        switch (current_led_state(&state)) {
            case LED_STATE_BOOTING:
                set_rgb(pulse, pulse / 3, 0);  // pulsing amber
                pulse += pulse_step;
                if (pulse >= 88 || pulse <= 16) pulse_step = -pulse_step;
                break;
            case LED_STATE_FAULT:
                set_rgb(80, 0, 0);  // red
                break;
            case LED_STATE_READY: set_rgb(0, 0, 72); break;       // blue
            case LED_STATE_LAN: set_rgb(0, 64, 72); break;         // cyan
            case LED_STATE_INTERNET: set_rgb(0, 72, 0); break;     // green
            case LED_STATE_GHOSTWIRE: set_rgb(64, 0, 80); break;   // purple
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

static void initialize_status_led(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    const int pins[] = {POE_P4_LED_RED_GPIO, POE_P4_LED_GREEN_GPIO,
                        POE_P4_LED_BLUE_GPIO};
    for (int channel = 0; channel < 3; ++channel) {
        const ledc_channel_config_t output = {
            .gpio_num = pins[channel],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 255,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&output));
    }
    xTaskCreate(led_task, "status_led", 3072, NULL, 3, NULL);
}

static bool grove_link_connected(const ethernet_state_t *state)
{
    return state->grove_valid_frames > 0 &&
           esp_timer_get_time() - state->last_grove_ack_us <
               GROVE_LINK_TIMEOUT_MS * 1000LL;
}

static void process_grove_ack(char *line)
{
    char *crc_separator = strrchr(line, ',');
    if (crc_separator == NULL || strncmp(line, "GW1,A,", 6) != 0) return;
    char *crc_end = NULL;
    const uint32_t received_crc = strtoul(crc_separator + 1, &crc_end, 16);
    if (crc_end == crc_separator + 1 || *crc_end != '\0' ||
        received_crc != ghostwire_grove_crc32(line, crc_separator - line)) {
        portENTER_CRITICAL(&s_state_lock);
        ++s_ethernet.grove_crc_errors;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    char *sequence_end = NULL;
    const uint32_t sequence = strtoul(line + 6, &sequence_end, 10);
    if (sequence_end == line + 6 || sequence_end != crc_separator) return;
    const int64_t acknowledgement_time_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_state_lock);
    if (sequence == s_ethernet.grove_sequence) {
        s_ethernet.last_grove_ack_us = acknowledgement_time_us;
        ++s_ethernet.grove_valid_frames;
    }
    portEXIT_CRITICAL(&s_state_lock);
}

// Forward-declared: defined below, but process_grove_pairing_request()
// (just below) needs to send its "Q" response through it.
static void grove_send_frame(const char *payload, int length);
// Forward-declared: defined with the rest of the payload engine, far below
// (it needs s_payload_slots/the button-event queue in scope), but
// process_grove_command_request() needs it to actually run an accepted
// command through the same path a real button press uses.
static bool payload_trigger_slot(size_t slot);
// Forward-declared: defined with the rest of the payload engine, far below
// (it's how a script upload actually gets committed), but
// process_grove_upload_finish() needs it.
static bool payload_store_slot_script(size_t slot, const char *script);
// Forward-declared: defined near command_handler(), far below, but
// process_grove_upload_finish() reuses the same replay cache the Wi-Fi
// command/upload endpoints use (see its own comment for why).
static bool command_replay_cache_check_and_record(uint8_t slot,
                                                   const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES]);

// --- Grove pairing (X25519 ECDH -> HKDF-SHA256 session key) -------------
// Full design/threat-model writeup: shared/protocol/auth.h. Reuses mbedtls
// (already a transitive ESP-IDF dependency) rather than hand-rolling any
// crypto primitive; the same library is available on the Cardputer's
// Arduino-ESP32 framework, so both sides run effectively the same code
// against the same standard algorithms.
#define PAIRING_NVS_NAMESPACE "ghostwire"
#define PAIRING_NVS_KEY "session_key"

static mbedtls_entropy_context s_entropy;
static mbedtls_ctr_drbg_context s_ctr_drbg;
static bool s_rng_ready;
static uint8_t s_session_key[GHOSTWIRE_AUTH_KEY_BYTES];
static bool s_session_key_valid;

static void initialize_pairing_rng(void)
{
    mbedtls_entropy_init(&s_entropy);
    mbedtls_ctr_drbg_init(&s_ctr_drbg);
    static const char personalization[] = "ghostwire-poe-p4-pairing";
    const int result = mbedtls_ctr_drbg_seed(
        &s_ctr_drbg, mbedtls_entropy_func, &s_entropy,
        (const unsigned char *)personalization, sizeof(personalization) - 1);
    s_rng_ready = result == 0;
    if (!s_rng_ready) {
        ESP_LOGE(TAG, "Pairing RNG seed failed (%d); pairing unavailable", result);
    }
}

static void hex_encode(char *dest, const uint8_t *src, size_t src_len)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < src_len; ++i) {
        dest[i * 2] = digits[src[i] >> 4];
        dest[i * 2 + 1] = digits[src[i] & 0x0F];
    }
    dest[src_len * 2] = '\0';
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool hex_decode(uint8_t *dest, size_t dest_len, const char *src)
{
    if (strlen(src) != dest_len * 2) return false;
    for (size_t i = 0; i < dest_len; ++i) {
        const int hi = hex_nibble(src[i * 2]);
        const int lo = hex_nibble(src[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        dest[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Not secret-dependent-timing-sensitive against a remote attacker (Grove is
// a physical link), but cheap and correct, so used everywhere a derived
// secret or tag is compared.
static bool constant_time_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

static bool pairing_store_session_key(const uint8_t key[GHOSTWIRE_AUTH_KEY_BYTES])
{
    nvs_handle_t handle;
    if (nvs_open(PAIRING_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    const esp_err_t set_result =
        nvs_set_blob(handle, PAIRING_NVS_KEY, key, GHOSTWIRE_AUTH_KEY_BYTES);
    const esp_err_t commit_result =
        set_result == ESP_OK ? nvs_commit(handle) : set_result;
    nvs_close(handle);
    return commit_result == ESP_OK;
}

static void pairing_load_session_key(void)
{
    nvs_handle_t handle;
    if (nvs_open(PAIRING_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    size_t length = sizeof(s_session_key);
    if (nvs_get_blob(handle, PAIRING_NVS_KEY, s_session_key, &length) == ESP_OK &&
        length == sizeof(s_session_key)) {
        s_session_key_valid = true;
        ESP_LOGI(TAG, "Loaded a previously paired Grove session key from NVS");
    }
    nvs_close(handle);
}

// Reusable time-windowed HMAC-SHA256 tag primitive (shared/protocol/auth.h).
// Not wired to any real command yet -- that's the Companion Mode command
// channel slice; this exists now so this slice's crypto has a directly
// testable unit built on top of it.
static bool ghostwire_auth_compute_tag(const uint8_t *key, size_t key_len,
                                       const uint8_t *message, size_t message_len,
                                       int64_t window,
                                       uint8_t tag_out[GHOSTWIRE_AUTH_TAG_BYTES])
{
    uint8_t buffer[128];
    if (message_len + 8 > sizeof(buffer)) return false;
    memcpy(buffer, message, message_len);
    for (int i = 0; i < 8; ++i) {
        buffer[message_len + i] = (uint8_t)(window >> (56 - i * 8));
    }
    uint8_t full_tag[32];
    if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, key_len,
                        buffer, message_len + 8, full_tag) != 0) {
        return false;
    }
    memcpy(tag_out, full_tag, GHOSTWIRE_AUTH_TAG_BYTES);
    return true;
}

static bool ghostwire_auth_verify_tag(const uint8_t *key, size_t key_len,
                                      const uint8_t *message, size_t message_len,
                                      time_t now_utc,
                                      const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES])
{
    const int64_t current_window = (int64_t)now_utc / GHOSTWIRE_AUTH_WINDOW_SECONDS;
    for (int64_t offset = -GHOSTWIRE_AUTH_WINDOW_TOLERANCE;
        offset <= GHOSTWIRE_AUTH_WINDOW_TOLERANCE; ++offset) {
        uint8_t candidate[GHOSTWIRE_AUTH_TAG_BYTES];
        if (!ghostwire_auth_compute_tag(key, key_len, message, message_len,
                                        current_window + offset, candidate)) {
            continue;
        }
        if (constant_time_equal(candidate, tag, GHOSTWIRE_AUTH_TAG_BYTES)) return true;
    }
    return false;
}

// One-shot boot-time sanity check of the auth primitive above: proves a tag
// computed for "now" validates immediately, and a tag computed for a window
// far in the past no longer validates "now" -- exactly the replay-window
// property shared/protocol/auth.h exists for. Doesn't require real pairing
// (uses a fixed test key), so it runs unconditionally at boot.
static void self_test_auth_tag(void)
{
    static const uint8_t test_key[GHOSTWIRE_AUTH_KEY_BYTES] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
    static const uint8_t test_message[] = "ghostwire-auth-self-test";
    const time_t now = time(NULL);
    const int64_t current_window = (int64_t)now / GHOSTWIRE_AUTH_WINDOW_SECONDS;

    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    if (!ghostwire_auth_compute_tag(test_key, sizeof(test_key), test_message,
                                    sizeof(test_message) - 1, current_window, tag)) {
        ESP_LOGE(TAG, "Auth self-test: compute_tag failed");
        return;
    }
    const bool accepts_now = ghostwire_auth_verify_tag(
        test_key, sizeof(test_key), test_message, sizeof(test_message) - 1, now, tag);
    // Far enough outside the tolerance window that it must be rejected --
    // this is the property that makes a captured/replayed command stop
    // working once its window has passed.
    const time_t far_future =
        now + (GHOSTWIRE_AUTH_WINDOW_TOLERANCE + 5) * GHOSTWIRE_AUTH_WINDOW_SECONDS;
    const bool rejects_stale = !ghostwire_auth_verify_tag(
        test_key, sizeof(test_key), test_message, sizeof(test_message) - 1,
        far_future, tag);
    ESP_LOGI(TAG, "Auth self-test: accepts-in-window=%s rejects-outside-window=%s",
             accepts_now ? "yes" : "NO", rejects_stale ? "yes" : "NO");
}

// Handles an incoming "GW1,P,<seq>,<pubkey_hex_64>,<crc>" pairing request:
// generates our own ephemeral X25519 keypair, computes the ECDH shared
// secret against the Cardputer's public key, derives and persists the
// session key, and replies with our own public key ("GW1,Q,...").
static void process_grove_pairing_request(char *line)
{
    char *crc_separator = strrchr(line, ',');
    if (crc_separator == NULL || strncmp(line, "GW1,P,", 6) != 0) return;
    char *crc_end = NULL;
    const uint32_t received_crc = strtoul(crc_separator + 1, &crc_end, 16);
    if (crc_end == crc_separator + 1 || *crc_end != '\0' ||
        received_crc != ghostwire_grove_crc32(line, crc_separator - line)) {
        portENTER_CRITICAL(&s_state_lock);
        ++s_ethernet.grove_crc_errors;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    *crc_separator = '\0';
    char *sequence_end = NULL;
    const uint32_t sequence = strtoul(line + 6, &sequence_end, 10);
    if (sequence_end == line + 6 || *sequence_end != ',') return;
    const char *peer_pubkey_hex = sequence_end + 1;

    if (!s_rng_ready) {
        ESP_LOGW(TAG, "Grove pairing request received but RNG isn't ready; ignoring");
        return;
    }
    uint8_t peer_public[32];
    if (!hex_decode(peer_public, sizeof(peer_public), peer_pubkey_hex)) {
        ESP_LOGW(TAG, "Grove pairing request has a malformed public key; ignoring");
        return;
    }

    mbedtls_ecdh_context ecdh;
    mbedtls_ecdh_init(&ecdh);
    // mbedtls_ecdh_make_public()/read_public() speak the TLS ECPoint wire
    // format (RFC 8422: a 1-byte length prefix followed by the point), not
    // a bare public key -- for Curve25519 that's 1 + 32 = 33 bytes, with
    // byte 0 always 32. Stripped/re-added here so the actual Grove wire
    // format stays the clean 32-byte raw key (64 hex chars) documented in
    // shared/protocol/README.md. mbedtls_ecdh_calc_secret() below is NOT
    // wrapped this way -- it writes the raw shared secret directly.
    uint8_t own_public_tls[33];
    uint8_t shared_secret[32];
    uint8_t session_key[GHOSTWIRE_AUTH_KEY_BYTES];
    size_t own_public_tls_len = 0;
    size_t shared_len = 0;
    bool ok = mbedtls_ecdh_setup(&ecdh, MBEDTLS_ECP_DP_CURVE25519) == 0;
    if (ok) {
        ok = mbedtls_ecdh_make_public(&ecdh, &own_public_tls_len, own_public_tls,
                                      sizeof(own_public_tls), mbedtls_ctr_drbg_random,
                                      &s_ctr_drbg) == 0 &&
             own_public_tls_len == sizeof(own_public_tls) && own_public_tls[0] == 32;
    }
    const uint8_t *own_public = own_public_tls + 1;
    if (ok) {
        uint8_t peer_public_tls[33];
        peer_public_tls[0] = sizeof(peer_public);
        memcpy(peer_public_tls + 1, peer_public, sizeof(peer_public));
        ok = mbedtls_ecdh_read_public(&ecdh, peer_public_tls,
                                      sizeof(peer_public_tls)) == 0;
    }
    if (ok) {
        ok = mbedtls_ecdh_calc_secret(&ecdh, &shared_len, shared_secret,
                                      sizeof(shared_secret), mbedtls_ctr_drbg_random,
                                      &s_ctr_drbg) == 0 &&
             shared_len == sizeof(shared_secret);
    }
    if (ok) {
        ok = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                          (const unsigned char *)GHOSTWIRE_AUTH_HKDF_SALT,
                          strlen(GHOSTWIRE_AUTH_HKDF_SALT), shared_secret,
                          shared_len, (const unsigned char *)GHOSTWIRE_AUTH_HKDF_INFO,
                          strlen(GHOSTWIRE_AUTH_HKDF_INFO), session_key,
                          sizeof(session_key)) == 0;
    }
    mbedtls_ecdh_free(&ecdh);

    if (!ok) {
        ESP_LOGE(TAG, "Grove pairing key derivation failed");
        return;
    }

    memcpy(s_session_key, session_key, sizeof(s_session_key));
    s_session_key_valid = true;
    if (!pairing_store_session_key(session_key)) {
        ESP_LOGW(TAG, "Grove pairing succeeded but NVS persist failed "
                      "(key usable this boot only)");
    }
    char fingerprint[9];
    hex_encode(fingerprint, session_key, 4);  // non-secret identifier, never the key itself
    ESP_LOGI(TAG, "Grove pairing complete, session key fingerprint %s...", fingerprint);

    char own_public_hex[65];
    // own_public is now a pointer into own_public_tls (see above), not an
    // array, so sizeof() here would silently give the pointer's size (4)
    // instead of the key's -- 32 is the actual, fixed Curve25519 key size.
    hex_encode(own_public_hex, own_public, 32);
    // Sized to GHOSTWIRE_GROVE_MAX_LINE, not just the ~72 bytes this
    // actually needs -- see the matching comment on the Cardputer side
    // (src/grove_companion_link.cpp) for why an undersized buffer here is
    // more than a truncation risk.
    // The Cardputer has no RTC/NTP of its own -- piggybacking our
    // NTP-synced Unix time here lets it derive a clock offset from this
    // already-trusted exchange instead of needing Wi-Fi just to compute
    // command auth tags. See shared/protocol/auth.h.
    char response_payload[GHOSTWIRE_GROVE_MAX_LINE];
    const int response_length = snprintf(
        response_payload, sizeof(response_payload), "GW1,Q,%lu,%s,%llu",
        (unsigned long)sequence, own_public_hex,
        (unsigned long long)time(NULL));
    grove_send_frame(response_payload, response_length);
}

// Handles an incoming "GW1,C,<seq>,<slot>,<tag_hex16>,<crc>" command request:
// verifies the CRC, requires an established session key, verifies the
// time-windowed auth tag (shared/protocol/auth.h) against the single slot
// byte as the authenticated message, and replies
// "GW1,K,<seq>,<accepted>,<crc>" immediately either way -- accepted is 0/1,
// never a reason, so a rejected command reveals nothing about *why* (bad
// tag, unknown slot, already busy) to whatever's listening on the wire. On
// acceptance, the actual trigger goes through payload_trigger_slot(), the
// same queue a real button press uses, rather than duplicating dispatch
// logic here. The real completion result (running -> success/error) rides
// the next 1 Hz "S" status frame, not this one.
static void process_grove_command_request(char *line)
{
    char *crc_separator = strrchr(line, ',');
    if (crc_separator == NULL || strncmp(line, "GW1,C,", 6) != 0) return;
    char *crc_end = NULL;
    const uint32_t received_crc = strtoul(crc_separator + 1, &crc_end, 16);
    if (crc_end == crc_separator + 1 || *crc_end != '\0' ||
        received_crc != ghostwire_grove_crc32(line, crc_separator - line)) {
        portENTER_CRITICAL(&s_state_lock);
        ++s_ethernet.grove_crc_errors;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    *crc_separator = '\0';
    char *sequence_end = NULL;
    const uint32_t sequence = strtoul(line + 6, &sequence_end, 10);
    if (sequence_end == line + 6 || *sequence_end != ',') return;
    char *slot_end = NULL;
    const unsigned long slot_value = strtoul(sequence_end + 1, &slot_end, 10);
    if (slot_end == sequence_end + 1 || *slot_end != ',') return;
    const char *tag_hex = slot_end + 1;

    bool accepted = false;
    if (!s_session_key_valid) {
        ESP_LOGW(TAG, "Grove command received but not paired; rejecting");
    } else {
        uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
        if (slot_value > 1 || !hex_decode(tag, sizeof(tag), tag_hex)) {
            ESP_LOGW(TAG, "Grove command has a malformed slot/tag; rejecting");
        } else {
            const uint8_t slot_byte = (uint8_t)slot_value;
            if (!ghostwire_auth_verify_tag(s_session_key, sizeof(s_session_key),
                                           &slot_byte, 1, time(NULL), tag)) {
                ESP_LOGW(TAG, "Grove command failed auth verification; rejecting");
            } else {
                accepted = payload_trigger_slot((size_t)slot_value);
                if (!accepted) {
                    ESP_LOGW(TAG, "Grove command authenticated but payload engine is "
                                  "busy; rejecting");
                }
            }
        }
    }

    char response_payload[GHOSTWIRE_GROVE_MAX_LINE];
    const int response_length = snprintf(
        response_payload, sizeof(response_payload), "GW1,K,%lu,%d",
        (unsigned long)sequence, accepted ? 1 : 0);
    grove_send_frame(response_payload, response_length);
}

// Grove chunked script upload ("U" begin -> N x "D" data -> "F" finish,
// replying "V" then "K"). Only grove_uart_task ever touches this -- every
// process_grove_* handler runs synchronously in that same task from the
// receive loop below, so unlike s_payload_slots/s_loot_entries this needs
// no s_state_lock. No chunk index or per-chunk ack: Grove is a reliable
// ordered UART stream, not a lossy link (measured zero CRC errors over
// extended runs earlier this project), so chunks just accumulate in
// receive order and a length mismatch at "F" rejects the whole upload --
// simpler than partial-recovery, and the Cardputer can just retry.
typedef struct {
    bool active;
    uint8_t slot;
    size_t total_len;
    size_t received_len;
    char buffer[PAYLOAD_SCRIPT_MAX_BYTES];
} grove_upload_state_t;
static grove_upload_state_t s_grove_upload;

// "GW1,U,<seq>,<slot>,<total_len>,<crc>" -- begin a script upload. Replies
// "GW1,V,<seq>,<accepted>,<crc>": accepted means there's room, the slot
// isn't busy, and no other upload is already in progress (one at a time).
static void process_grove_upload_begin(char *line)
{
    char *crc_separator = strrchr(line, ',');
    if (crc_separator == NULL || strncmp(line, "GW1,U,", 6) != 0) return;
    char *crc_end = NULL;
    const uint32_t received_crc = strtoul(crc_separator + 1, &crc_end, 16);
    if (crc_end == crc_separator + 1 || *crc_end != '\0' ||
        received_crc != ghostwire_grove_crc32(line, crc_separator - line)) {
        portENTER_CRITICAL(&s_state_lock);
        ++s_ethernet.grove_crc_errors;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    *crc_separator = '\0';
    char *sequence_end = NULL;
    const uint32_t sequence = strtoul(line + 6, &sequence_end, 10);
    if (sequence_end == line + 6 || *sequence_end != ',') return;
    char *slot_end = NULL;
    const unsigned long slot_value = strtoul(sequence_end + 1, &slot_end, 10);
    if (slot_end == sequence_end + 1 || *slot_end != ',') return;
    char *len_end = NULL;
    const unsigned long total_len = strtoul(slot_end + 1, &len_end, 10);
    if (len_end == slot_end + 1 || *len_end != '\0') return;

    bool accepted = false;
    if (!s_session_key_valid) {
        ESP_LOGW(TAG, "Grove upload begin received but not paired; rejecting");
    } else if (s_grove_upload.active) {
        ESP_LOGW(TAG, "Grove upload begin received mid-upload; rejecting");
    } else if (slot_value > 1 || total_len == 0 || total_len >= sizeof(s_grove_upload.buffer)) {
        ESP_LOGW(TAG, "Grove upload begin has an invalid slot/length; rejecting");
    } else {
        portENTER_CRITICAL(&s_state_lock);
        const bool busy = s_payload_state == PAYLOAD_STATE_RUNNING;
        portEXIT_CRITICAL(&s_state_lock);
        if (busy) {
            ESP_LOGW(TAG, "Grove upload begin authenticated but slot %lu is running; "
                          "rejecting", slot_value);
        } else {
            s_grove_upload.active = true;
            s_grove_upload.slot = (uint8_t)slot_value;
            s_grove_upload.total_len = total_len;
            s_grove_upload.received_len = 0;
            accepted = true;
        }
    }

    char response_payload[GHOSTWIRE_GROVE_MAX_LINE];
    const int response_length = snprintf(
        response_payload, sizeof(response_payload), "GW1,V,%lu,%d",
        (unsigned long)sequence, accepted ? 1 : 0);
    grove_send_frame(response_payload, response_length);
}

// "GW1,D,<seq>,<chunk_hex>,<crc>" -- one raw data chunk, hex-encoded (the
// script may contain characters, e.g. a PORT_SCAN port list's commas, that
// would otherwise break this comma-delimited framing). No response frame;
// silently ignored if no upload is active or the chunk would overflow the
// declared total_len, since process_grove_upload_finish() below is what
// actually validates the whole transfer.
static void process_grove_upload_chunk(char *line)
{
    char *crc_separator = strrchr(line, ',');
    if (crc_separator == NULL || strncmp(line, "GW1,D,", 6) != 0) return;
    char *crc_end = NULL;
    const uint32_t received_crc = strtoul(crc_separator + 1, &crc_end, 16);
    if (crc_end == crc_separator + 1 || *crc_end != '\0' ||
        received_crc != ghostwire_grove_crc32(line, crc_separator - line)) {
        portENTER_CRITICAL(&s_state_lock);
        ++s_ethernet.grove_crc_errors;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    *crc_separator = '\0';
    char *sequence_end = NULL;
    strtoul(line + 6, &sequence_end, 10);
    if (sequence_end == line + 6 || *sequence_end != ',') return;
    const char *chunk_hex = sequence_end + 1;

    if (!s_grove_upload.active) return;
    const size_t hex_len = strlen(chunk_hex);
    if (hex_len % 2 != 0) return;
    const size_t chunk_bytes = hex_len / 2;
    if (s_grove_upload.received_len + chunk_bytes > sizeof(s_grove_upload.buffer)) return;
    if (!hex_decode((uint8_t *)s_grove_upload.buffer + s_grove_upload.received_len,
                    chunk_bytes, chunk_hex)) {
        return;
    }
    s_grove_upload.received_len += chunk_bytes;
}

// "GW1,F,<seq>,<nonce>,<tag_hex16>,<crc>" -- finish an upload. Verifies the
// received length matches what "U" declared, then the auth tag over
// slot||nonce||sha256(received bytes) -- see payload_handler()'s comment
// for why a hash rather than the raw bytes. Replies "GW1,K,<seq>,<accepted>,
// <crc>", reusing the same frame type the command channel's accept/reject
// uses. Ends the upload attempt (active=false) whether accepted or not.
static void process_grove_upload_finish(char *line)
{
    char *crc_separator = strrchr(line, ',');
    if (crc_separator == NULL || strncmp(line, "GW1,F,", 6) != 0) return;
    char *crc_end = NULL;
    const uint32_t received_crc = strtoul(crc_separator + 1, &crc_end, 16);
    if (crc_end == crc_separator + 1 || *crc_end != '\0' ||
        received_crc != ghostwire_grove_crc32(line, crc_separator - line)) {
        portENTER_CRITICAL(&s_state_lock);
        ++s_ethernet.grove_crc_errors;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    *crc_separator = '\0';
    char *sequence_end = NULL;
    const uint32_t sequence = strtoul(line + 6, &sequence_end, 10);
    if (sequence_end == line + 6 || *sequence_end != ',') return;
    char *nonce_end = NULL;
    const uint32_t nonce = strtoul(sequence_end + 1, &nonce_end, 10);
    if (nonce_end == sequence_end + 1 || *nonce_end != ',') return;
    const char *tag_hex = nonce_end + 1;

    bool accepted = false;
    if (!s_grove_upload.active) {
        ESP_LOGW(TAG, "Grove upload finish received with no upload in progress; ignoring");
    } else {
        uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
        if (!s_session_key_valid) {
            ESP_LOGW(TAG, "Grove upload finish received but not paired; rejecting");
        } else if (s_grove_upload.received_len != s_grove_upload.total_len ||
                   !hex_decode(tag, sizeof(tag), tag_hex)) {
            ESP_LOGW(TAG, "Grove upload finish has a length mismatch or malformed tag; "
                          "rejecting");
        } else {
            s_grove_upload.buffer[s_grove_upload.received_len] = '\0';
            uint8_t script_hash[32];
            mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      (const unsigned char *)s_grove_upload.buffer,
                      s_grove_upload.received_len, script_hash);
            uint8_t message[1 + 4 + sizeof(script_hash)];
            message[0] = s_grove_upload.slot;
            message[1] = (uint8_t)(nonce >> 24);
            message[2] = (uint8_t)(nonce >> 16);
            message[3] = (uint8_t)(nonce >> 8);
            message[4] = (uint8_t)nonce;
            memcpy(message + 5, script_hash, sizeof(script_hash));
            if (!ghostwire_auth_verify_tag(s_session_key, sizeof(s_session_key), message,
                                           sizeof(message), time(NULL), tag)) {
                ESP_LOGW(TAG, "Grove upload finish failed auth verification; rejecting");
            } else if (!command_replay_cache_check_and_record(s_grove_upload.slot, tag)) {
                ESP_LOGW(TAG, "Grove upload finish replay detected; rejecting");
            } else {
                portENTER_CRITICAL(&s_state_lock);
                const bool busy = s_payload_state == PAYLOAD_STATE_RUNNING;
                portEXIT_CRITICAL(&s_state_lock);
                if (busy) {
                    ESP_LOGW(TAG, "Grove upload finish authenticated but slot %u is "
                                  "running; rejecting", (unsigned)s_grove_upload.slot);
                } else {
                    accepted = payload_store_slot_script(s_grove_upload.slot,
                                                         s_grove_upload.buffer);
                    if (accepted) {
                        ESP_LOGI(TAG, "Slot %u script updated over Grove (%u bytes)",
                                (unsigned)s_grove_upload.slot,
                                (unsigned)s_grove_upload.received_len);
                    }
                }
            }
        }
    }
    s_grove_upload.active = false;

    char response_payload[GHOSTWIRE_GROVE_MAX_LINE];
    const int response_length = snprintf(
        response_payload, sizeof(response_payload), "GW1,K,%lu,%d",
        (unsigned long)sequence, accepted ? 1 : 0);
    grove_send_frame(response_payload, response_length);
}

// "GW1,X,<seq>,<nonce>,<tag_hex16>,<crc>" -- request the accumulated loot
// log. Auth message is nonce_be32 alone (no slot -- this isn't a
// per-slot action); unlike script upload/command triggers, a rejection
// here doesn't get a distinct "no" -- an unauthorized/unpaired request
// just gets an empty log (count 0), the same response an authorized-but-
// genuinely-empty log would get, so the two aren't distinguishable from
// the wire either (matching loot_handler()'s Wi-Fi equivalent). Replies
// "GW1,N,<seq>,<count>,<crc>" followed by exactly <count> "GW1,E,<seq>,
// <ip>,<port>,<crc>" frames -- no per-entry ack needed, same reliable-
// stream reasoning as the upload chunks above.
static void process_grove_loot_request(char *line)
{
    char *crc_separator = strrchr(line, ',');
    if (crc_separator == NULL || strncmp(line, "GW1,X,", 6) != 0) return;
    char *crc_end = NULL;
    const uint32_t received_crc = strtoul(crc_separator + 1, &crc_end, 16);
    if (crc_end == crc_separator + 1 || *crc_end != '\0' ||
        received_crc != ghostwire_grove_crc32(line, crc_separator - line)) {
        portENTER_CRITICAL(&s_state_lock);
        ++s_ethernet.grove_crc_errors;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    *crc_separator = '\0';
    char *sequence_end = NULL;
    const uint32_t sequence = strtoul(line + 6, &sequence_end, 10);
    if (sequence_end == line + 6 || *sequence_end != ',') return;
    char *nonce_end = NULL;
    const uint32_t nonce = strtoul(sequence_end + 1, &nonce_end, 10);
    if (nonce_end == sequence_end + 1 || *nonce_end != ',') return;
    const char *tag_hex = nonce_end + 1;

    bool authorized = false;
    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    if (s_session_key_valid && hex_decode(tag, sizeof(tag), tag_hex)) {
        const uint8_t message[4] = {
            (uint8_t)(nonce >> 24), (uint8_t)(nonce >> 16),
            (uint8_t)(nonce >> 8), (uint8_t)nonce,
        };
        authorized = ghostwire_auth_verify_tag(s_session_key, sizeof(s_session_key),
                                               message, sizeof(message), time(NULL), tag);
    }
    if (!authorized) {
        ESP_LOGW(TAG, "Grove loot request failed auth verification or wasn't paired; "
                      "sending empty log");
    }

    portENTER_CRITICAL(&s_state_lock);
    const size_t entry_count = authorized ? s_loot_entry_count : 0;
    loot_entry_t entries[LOOT_MAX_ENTRIES];
    if (authorized) memcpy(entries, s_loot_entries, entry_count * sizeof(loot_entry_t));
    portEXIT_CRITICAL(&s_state_lock);

    char count_payload[GHOSTWIRE_GROVE_MAX_LINE];
    const int count_length = snprintf(count_payload, sizeof(count_payload), "GW1,N,%lu,%u",
                                      (unsigned long)sequence, (unsigned)entry_count);
    grove_send_frame(count_payload, count_length);
    for (size_t i = 0; i < entry_count; ++i) {
        char entry_payload[GHOSTWIRE_GROVE_MAX_LINE];
        const int entry_length = snprintf(entry_payload, sizeof(entry_payload),
                                          "GW1,E,%lu,%s,%u", (unsigned long)sequence,
                                          entries[i].ip, (unsigned)entries[i].port);
        grove_send_frame(entry_payload, entry_length);
    }
}

// Appends the CRC32 (over `payload[0..length)`) and a trailing newline, then
// writes the frame to the Grove UART. Shared by the heartbeat, status, and
// identity senders below.
static void grove_send_frame(const char *payload, int length)
{
    if (length < 0) return;
    const uint32_t crc = ghostwire_grove_crc32(payload, (size_t)length);
    char frame[GHOSTWIRE_GROVE_MAX_LINE];
    const int frame_length = snprintf(frame, sizeof(frame), "%s,%08lX\n",
                                      payload, (unsigned long)crc);
    if (frame_length < 0 || (size_t)frame_length >= sizeof(frame)) {
        ESP_LOGE(TAG, "Grove frame overflow, dropped");
        return;
    }
    uart_write_bytes(UART_NUM_1, frame, frame_length);
}

static const char *current_reset_reason_name(void)
{
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        default: return "unknown";
    }
}

static char payload_state_code(payload_run_state_t state)
{
    switch (state) {
        case PAYLOAD_STATE_RUNNING: return 'R';
        case PAYLOAD_STATE_SUCCESS: return 'S';
        case PAYLOAD_STATE_ERROR: return 'E';
        case PAYLOAD_STATE_IDLE:
        default: return 'I';
    }
}

static void grove_send_status(uint32_t sequence, int64_t now_us)
{
    const ethernet_state_t state = ethernet_state_snapshot();
    const companion_led_state_t led_state = current_led_state(&state);
    float temperature_c = 0.0f;
    const bool temperature_valid = s_temperature_sensor != NULL &&
        temperature_sensor_get_celsius(s_temperature_sensor, &temperature_c) == ESP_OK;
    const int temp_x10 = temperature_valid
        ? (int)(temperature_c * 10.0f)
        : GHOSTWIRE_GROVE_NO_TEMPERATURE;
    // Not payload_state_snapshot_and_expire() -- that has the side effect of
    // reverting an expired result to IDLE, which belongs to the LED's own
    // display timing, not to what gets reported here.
    portENTER_CRITICAL(&s_state_lock);
    const payload_run_state_t payload_state = s_payload_state;
    const size_t finding_count = s_scan_finding_count;
    portEXIT_CRITICAL(&s_state_lock);

    char payload[96];
    const int length = snprintf(
        payload, sizeof(payload),
        "GW1,S,%lu,%d,%lu,%d,%d,%c,%llu,%c,%d,%lu,%lu,%s,%c,%lu",
        (unsigned long)sequence,
        state.link_up ? 1 : 0,
        (unsigned long)state.link_speed_mbps,
        state.full_duplex ? 1 : 0,
        state.internet_reachable ? 1 : 0,
        ghostwire_grove_indicator_code(led_state_name(led_state)),
        (unsigned long long)(now_us / 1000000),
        ghostwire_grove_reset_code(current_reset_reason_name()),
        temp_x10,
        (unsigned long)(esp_get_free_heap_size() / 1024),
        (unsigned long)(esp_get_minimum_free_heap_size() / 1024),
        state.ip,
        payload_state_code(payload_state),
        (unsigned long)finding_count);
    grove_send_frame(payload, length);
}

static void grove_send_identity(uint32_t sequence)
{
    char payload[80];
    const int length = snprintf(payload, sizeof(payload), "GW1,I,%lu,%s,%s",
                                (unsigned long)sequence, s_device_id,
                                COMPANION_FIRMWARE_VERSION);
    grove_send_frame(payload, length);
}

static void grove_uart_task(void *argument)
{
    (void)argument;
    const uart_config_t config = {
        .baud_rate = GHOSTWIRE_GROVE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 512, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, POE_P4_GROVE_TX_GPIO,
                                 POE_P4_GROVE_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    char line[GHOSTWIRE_GROVE_MAX_LINE] = {0};
    size_t line_length = 0;
    int64_t last_send_us = 0;
    uint32_t cycle = 0;
    while (true) {
        const int64_t now = esp_timer_get_time();
        if (now - last_send_us >= 1000000) {
            char payload[64];
            portENTER_CRITICAL(&s_state_lock);
            const uint32_t sequence = ++s_ethernet.grove_sequence;
            portEXIT_CRITICAL(&s_state_lock);
            const int length = snprintf(payload, sizeof(payload), "GW1,H,%lu,%llu",
                                        (unsigned long)sequence,
                                        (unsigned long long)(now / 1000));
            grove_send_frame(payload, length);
            grove_send_status(sequence, now);
            if (cycle % GHOSTWIRE_GROVE_IDENTITY_EVERY_N_CYCLES == 0) {
                grove_send_identity(sequence);
            }
            ++cycle;
            last_send_us = now;
        }

        uint8_t received[32];
        const int count = uart_read_bytes(UART_NUM_1, received, sizeof(received),
                                          pdMS_TO_TICKS(50));
        for (int index = 0; index < count; ++index) {
            const char value = (char)received[index];
            if (value == '\n') {
                if (line_length > 0) {
                    line[line_length] = '\0';
                    process_grove_ack(line);
                    process_grove_pairing_request(line);
                    process_grove_command_request(line);
                    process_grove_upload_begin(line);
                    process_grove_upload_chunk(line);
                    process_grove_upload_finish(line);
                    process_grove_loot_request(line);
                }
                line_length = 0;
            } else if (value != '\r') {
                if (line_length < sizeof(line) - 1) {
                    line[line_length++] = value;
                } else {
                    line_length = 0;
                    portENTER_CRITICAL(&s_state_lock);
                    ++s_ethernet.grove_crc_errors;
                    portEXIT_CRITICAL(&s_state_lock);
                }
            }
        }
    }
}

static bool probe_internet(void)
{
    const int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket_fd < 0) return false;
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    const struct sockaddr_in destination = {
        .sin_family = AF_INET,
        .sin_port = htons(443),
        .sin_addr.s_addr = inet_addr("1.1.1.1"),
    };
    int result = connect(socket_fd, (const struct sockaddr *)&destination,
                         sizeof(destination));
    if (result < 0 && errno == EINPROGRESS) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(socket_fd, &writable);
        struct timeval timeout = {.tv_sec = 2, .tv_usec = 0};
        result = select(socket_fd + 1, NULL, &writable, NULL, &timeout);
        if (result > 0) {
            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size);
            result = socket_error == 0 ? 0 : -1;
        }
    }
    close(socket_fd);
    return result == 0;
}

// Payload engine: a small DuckyScript-flavored interpreter (line-oriented,
// REM comments, DELAY) over a purpose-built set of network verbs -- real
// DuckyScript's actual command set (STRING/GUI/CTRL/ALT) is keystroke-
// injection specific and doesn't apply to a headless device like this one.
// Scripts are compile-time constants for this slice (no consumer for
// runtime-editable ones exists yet), but the parser is written defensively
// since a later slice (the Companion Mode command channel) will feed in
// externally-supplied scripts without needing to revisit this interpreter.
static void payload_set_result(bool success)
{
    portENTER_CRITICAL(&s_state_lock);
    s_payload_state = success ? PAYLOAD_STATE_SUCCESS : PAYLOAD_STATE_ERROR;
    s_payload_hold_until_us =
        esp_timer_get_time() + PAYLOAD_RESULT_HOLD_MS * 1000LL;
    portEXIT_CRITICAL(&s_state_lock);
}

// Host discovery + common-port scan across the P4's own subnet. Mirrors the
// Cardputer's NetworkHostScanService (single ICMP ping at a time, 300ms
// timeout) and scanNetworkPorts() (GHOSTWIRE_COMMON_PORTS, sequential
// blocking connect, 300ms timeout each) so a "scan" means the same thing
// from either device. Results reporting over HTTP/Grove is a later slice --
// for now findings are just logged and kept in a small in-RAM buffer.
#define SCAN_MAX_HOSTS 254
#define SCAN_PING_TIMEOUT_MS 300
#define SCAN_PORT_TIMEOUT_MS 300
#define SCAN_MAX_FINDINGS 32
#define SCAN_MAX_DISCOVERED_HOSTS 32

typedef struct {
    char ip[16];
    uint16_t port;
} scan_finding_t;

static scan_finding_t s_scan_findings[SCAN_MAX_FINDINGS];
// s_scan_finding_count itself is declared earlier, alongside s_payload_state.

// Hosts found by the most recent PING_SWEEP command in a script run, for a
// following PORT_SCAN command to consume.
static char s_scan_hosts[SCAN_MAX_DISCOVERED_HOSTS][16];
static size_t s_scan_host_count;

typedef struct {
    volatile bool done;
    volatile bool found;
} scan_ping_result_t;

static void scan_ping_success_cb(esp_ping_handle_t hdl, void *args)
{
    (void)hdl;
    ((scan_ping_result_t *)args)->found = true;
}

static void scan_ping_end_cb(esp_ping_handle_t hdl, void *args)
{
    (void)hdl;
    ((scan_ping_result_t *)args)->done = true;
}

// Blocking wrapper over esp_ping -- one host at a time, same as
// NetworkHostScanService, since payload scripts run synchronously in their
// own dedicated task and don't need an update()-driven service class.
static bool scan_ping_host(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    scan_ping_result_t result = {0};
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = 1;
    config.timeout_ms = SCAN_PING_TIMEOUT_MS;
    config.interval_ms = SCAN_PING_TIMEOUT_MS;
    config.data_size = 32;
    IP_ADDR4(&config.target_addr, a, b, c, d);

    esp_ping_callbacks_t callbacks = {
        .cb_args = &result,
        .on_ping_success = scan_ping_success_cb,
        .on_ping_end = scan_ping_end_cb,
    };
    esp_ping_handle_t handle = NULL;
    if (esp_ping_new_session(&config, &callbacks, &handle) != ESP_OK) return false;
    esp_ping_start(handle);
    const int64_t deadline_us =
        esp_timer_get_time() + (SCAN_PING_TIMEOUT_MS + 500) * 1000LL;
    while (!result.done && esp_timer_get_time() < deadline_us) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    esp_ping_stop(handle);
    esp_ping_delete_session(handle);
    return result.found;
}

// Mirrors scanNetworkPorts()'s sequential blocking-connect approach (one
// port at a time, 300ms timeout) rather than the Cardputer's concurrent
// full-range scanner -- that concurrency is reserved for its 1-65535 sweep,
// not the common-ports quick scan this matches.
static bool scan_port_open(const char *ip, uint16_t port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip),
    };
    int result = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (result < 0 && errno == EINPROGRESS) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        struct timeval timeout = {.tv_sec = 0,
                                  .tv_usec = SCAN_PORT_TIMEOUT_MS * 1000};
        result = select(fd + 1, NULL, &writable, NULL, &timeout);
        if (result > 0) {
            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size);
            result = socket_error == 0 ? 0 : -1;
        } else {
            result = -1;
        }
    }
    close(fd);
    return result == 0;
}

// --- Script command handlers -----------------------------------------

static bool script_cmd_internet_check(void)
{
    const bool reachable = probe_internet();
    ESP_LOGI(TAG, "Payload script: INTERNET_CHECK -> %s",
             reachable ? "reachable" : "unreachable");
    return reachable;
}

static bool script_cmd_ping_sweep(void)
{
    const ethernet_state_t link_state = ethernet_state_snapshot();
    if (!link_state.link_up) {
        ESP_LOGW(TAG, "Payload script: PING_SWEEP needs Ethernet link, but it's down");
        return false;
    }
    esp_netif_ip_info_t ip_info;
    if (s_eth_netif == NULL ||
        esp_netif_get_ip_info(s_eth_netif, &ip_info) != ESP_OK ||
        ip_info.ip.addr == 0) {
        ESP_LOGW(TAG, "Payload script: PING_SWEEP needs an IP, but there isn't one yet");
        return false;
    }

    // IP2STR/copy_ipv4 elsewhere in this file already rely on esp_ip4_addr_t
    // giving dotted-decimal-order bytes via a direct cast; reused here to
    // build the arithmetic (network+1..broadcast-1) range the same way
    // NetworkHostScanService's ipToUint32()/uint32ToIp() do.
    const uint8_t *ip_bytes = (const uint8_t *)&ip_info.ip.addr;
    const uint8_t *mask_bytes = (const uint8_t *)&ip_info.netmask.addr;
    const uint32_t ip_value = ((uint32_t)ip_bytes[0] << 24) |
                              ((uint32_t)ip_bytes[1] << 16) |
                              ((uint32_t)ip_bytes[2] << 8) | ip_bytes[3];
    const uint32_t mask_value = ((uint32_t)mask_bytes[0] << 24) |
                                ((uint32_t)mask_bytes[1] << 16) |
                                ((uint32_t)mask_bytes[2] << 8) | mask_bytes[3];
    const uint32_t network = ip_value & mask_value;
    const uint32_t broadcast = network | ~mask_value;
    const uint32_t available =
        broadcast > network + 1 ? broadcast - network - 1 : 0;
    const uint32_t host_count =
        available < SCAN_MAX_HOSTS ? available : SCAN_MAX_HOSTS;

    s_scan_host_count = 0;
    ESP_LOGI(TAG, "Payload script: PING_SWEEP scanning %lu candidate hosts",
             (unsigned long)host_count);
    for (uint32_t i = 0; i < host_count; ++i) {
        const uint32_t candidate = network + 1 + i;
        if (candidate == ip_value) continue;  // skip self
        const uint8_t a = (candidate >> 24) & 0xFF;
        const uint8_t b = (candidate >> 16) & 0xFF;
        const uint8_t c = (candidate >> 8) & 0xFF;
        const uint8_t d = candidate & 0xFF;
        if (!scan_ping_host(a, b, c, d)) continue;
        ESP_LOGI(TAG, "Payload script: host up %u.%u.%u.%u", a, b, c, d);
        if (s_scan_host_count < SCAN_MAX_DISCOVERED_HOSTS) {
            snprintf(s_scan_hosts[s_scan_host_count],
                     sizeof(s_scan_hosts[s_scan_host_count]), "%u.%u.%u.%u",
                     a, b, c, d);
            ++s_scan_host_count;
        }
    }
    ESP_LOGI(TAG, "Payload script: PING_SWEEP complete, %u hosts up",
             (unsigned)s_scan_host_count);
    return true;
}

// `args`: optional comma-separated port list overriding GHOSTWIRE_COMMON_PORTS
// (e.g. "22,80,443"), or empty/NULL for the default.
static bool script_cmd_port_scan(const char *args)
{
    const uint16_t *ports = GHOSTWIRE_COMMON_PORTS;
    size_t port_count = GHOSTWIRE_COMMON_PORT_COUNT;
    uint16_t custom_ports[GHOSTWIRE_COMMON_PORT_COUNT];
    size_t custom_count = 0;
    if (args != NULL && args[0] != '\0') {
        char buffer[96];
        snprintf(buffer, sizeof(buffer), "%s", args);
        char *saveptr = NULL;
        char *token = strtok_r(buffer, ",", &saveptr);
        while (token != NULL && custom_count < GHOSTWIRE_COMMON_PORT_COUNT) {
            custom_ports[custom_count++] = (uint16_t)strtoul(token, NULL, 10);
            token = strtok_r(NULL, ",", &saveptr);
        }
        if (custom_count > 0) {
            ports = custom_ports;
            port_count = custom_count;
        }
    }

    // s_scan_finding_count is read from grove_uart_task's context too (it's
    // reported live in the S status frame while a scan is in progress), so
    // every update -- not just the payload-state transitions -- goes
    // through s_state_lock, matching the rest of this file's convention for
    // cross-task state.
    portENTER_CRITICAL(&s_state_lock);
    s_scan_finding_count = 0;
    portEXIT_CRITICAL(&s_state_lock);
    ESP_LOGI(TAG, "Payload script: PORT_SCAN across %u discovered host(s)",
             (unsigned)s_scan_host_count);
    for (size_t h = 0; h < s_scan_host_count; ++h) {
        const char *ip_string = s_scan_hosts[h];
        for (size_t p = 0; p < port_count; ++p) {
            const uint16_t port = ports[p];
            if (!scan_port_open(ip_string, port)) continue;
            ESP_LOGI(TAG, "Payload script: open port %s:%u", ip_string, port);
            if (s_scan_finding_count < SCAN_MAX_FINDINGS) {
                // ip_string always fits (both arrays are sized for a
                // dotted-quad IPv4 string); a plain strncpy sidesteps
                // -Wformat-truncation's inability to prove that through the
                // const char* parameter type.
                strncpy(s_scan_findings[s_scan_finding_count].ip, ip_string,
                        sizeof(s_scan_findings[s_scan_finding_count].ip) - 1);
                s_scan_findings[s_scan_finding_count]
                    .ip[sizeof(s_scan_findings[s_scan_finding_count].ip) - 1] =
                    '\0';
                s_scan_findings[s_scan_finding_count].port = port;
                portENTER_CRITICAL(&s_state_lock);
                ++s_scan_finding_count;
                portEXIT_CRITICAL(&s_state_lock);
            }
            loot_record(ip_string, port);
        }
    }
    ESP_LOGI(TAG, "Payload script: PORT_SCAN complete, %u open port(s) found",
             (unsigned)s_scan_finding_count);
    return true;
}

// --- Interpreter --------------------------------------------------------

static bool script_run_line(char *line)
{
    while (*line == ' ' || *line == '\t') ++line;
    if (line[0] == '\0') return true;  // blank line, no-op

    char *args = strchr(line, ' ');
    if (args != NULL) {
        *args = '\0';
        ++args;
        while (*args == ' ' || *args == '\t') ++args;
    }

    if (strcmp(line, "REM") == 0) return true;
    if (strcmp(line, "DELAY") == 0) {
        vTaskDelay(pdMS_TO_TICKS(args ? atoi(args) : 0));
        return true;
    }
    if (strcmp(line, "LOG") == 0) {
        ESP_LOGI(TAG, "Payload script: %s", args ? args : "");
        return true;
    }
    if (strcmp(line, "INTERNET_CHECK") == 0) return script_cmd_internet_check();
    if (strcmp(line, "PING_SWEEP") == 0) return script_cmd_ping_sweep();
    if (strcmp(line, "PORT_SCAN") == 0) return script_cmd_port_scan(args);

    ESP_LOGW(TAG, "Payload script: unknown command '%s', skipping", line);
    return true;  // an unknown command doesn't fail the whole script
}

static void payload_run_script(const char *script)
{
    bool overall_ok = true;
    char buffer[PAYLOAD_SCRIPT_MAX_BYTES];
    snprintf(buffer, sizeof(buffer), "%s", script);
    char *saveptr = NULL;
    // strtok_r already null-terminates each line in place within `buffer`
    // (our own stack copy, not the original script constant), so
    // script_run_line() -- which further splits command from args in place
    // -- can operate directly on it without another copy.
    char *line = strtok_r(buffer, "\n", &saveptr);
    while (line != NULL) {
        if (!script_run_line(line)) overall_ok = false;
        line = strtok_r(NULL, "\n", &saveptr);
    }
    payload_set_result(overall_ok);
}

typedef struct {
    char name[16];
    char script[PAYLOAD_SCRIPT_MAX_BYTES];
} payload_descriptor_t;

// Slot 0 (short press) and slot 1 (long press). These defaults are what a
// factory-fresh P4 runs; either slot can be overwritten at runtime via
// "Upload script" (Grove or Wi-Fi, authenticated the same way command
// triggers are -- see process_grove_upload_finish()/payload_handler()) and
// persists across reboots via NVS (payload_store_slot_script()/
// payload_load_slot_scripts()).
static payload_descriptor_t s_payload_slots[2] = {
    {"internet_check", "INTERNET_CHECK\n"},
    {"port_scan", "PING_SWEEP\nPORT_SCAN\n"},
};

#define PAYLOAD_SLOT_NVS_KEY_0 "slot0_script"
#define PAYLOAD_SLOT_NVS_KEY_1 "slot1_script"

static const char *payload_slot_nvs_key(size_t slot)
{
    return slot == 0 ? PAYLOAD_SLOT_NVS_KEY_0 : PAYLOAD_SLOT_NVS_KEY_1;
}

// Persists a new script for `slot` to both RAM (s_payload_slots, read by
// payload_button_task on every trigger) and NVS (survives reboot). Caller
// is responsible for the busy-check (reject while that slot is running) and
// auth verification -- this just commits an already-accepted script.
static bool payload_store_slot_script(size_t slot, const char *script)
{
    if (slot > 1) return false;
    strncpy(s_payload_slots[slot].script, script,
            sizeof(s_payload_slots[slot].script) - 1);
    s_payload_slots[slot].script[sizeof(s_payload_slots[slot].script) - 1] = '\0';

    nvs_handle_t handle;
    if (nvs_open(PAIRING_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    const esp_err_t set_result =
        nvs_set_blob(handle, payload_slot_nvs_key(slot), s_payload_slots[slot].script,
                    sizeof(s_payload_slots[slot].script));
    const esp_err_t commit_result =
        set_result == ESP_OK ? nvs_commit(handle) : set_result;
    nvs_close(handle);
    return commit_result == ESP_OK;
}

static void payload_load_slot_scripts(void)
{
    nvs_handle_t handle;
    if (nvs_open(PAIRING_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    for (size_t slot = 0; slot < 2; ++slot) {
        size_t length = sizeof(s_payload_slots[slot].script);
        char loaded[PAYLOAD_SCRIPT_MAX_BYTES];
        if (nvs_get_blob(handle, payload_slot_nvs_key(slot), loaded, &length) == ESP_OK &&
            length == sizeof(loaded)) {
            memcpy(s_payload_slots[slot].script, loaded, sizeof(loaded));
            // NVS blobs aren't guaranteed null-terminated by construction --
            // payload_store_slot_script() always writes a terminated buffer,
            // but defend against a corrupt/foreign entry anyway.
            s_payload_slots[slot].script[PAYLOAD_SCRIPT_MAX_BYTES - 1] = '\0';
            ESP_LOGI(TAG, "Loaded a saved slot %u script from NVS", (unsigned)slot);
        }
    }
    nvs_close(handle);
}

typedef struct {
    int64_t press_time_us;
    int64_t release_time_us;
    // -1 for a real button press (slot derived below from how long it was
    // held); 0 or 1 for a Grove command-triggered run, where the slot is
    // given directly since there's no press duration to derive it from.
    // Lets a command run through the exact same dispatch/busy-check/
    // execution path as a real press instead of duplicating it.
    int forced_slot;
} button_event_t;

static QueueHandle_t s_button_queue;

static void IRAM_ATTR button_isr_handler(void *argument)
{
    (void)argument;
    static int64_t press_time_us = 0;
    const int64_t now_us = esp_timer_get_time();
    if (gpio_get_level(POE_P4_BUTTON_GPIO) == 0) {
        // Falling edge: button pressed (active low).
        press_time_us = now_us;
        return;
    }
    if (press_time_us == 0) return;  // rising edge with no matching press seen
    const button_event_t event = {.press_time_us = press_time_us,
                                   .release_time_us = now_us,
                                   .forced_slot = -1};
    press_time_us = 0;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_button_queue, &event, &woken);
    if (woken) portYIELD_FROM_ISR();
}

// Called from grove_uart_task (a normal task, not an ISR) when an
// authenticated command requests a slot -- queues it through the same path
// a real press uses. Returns false if the payload engine is currently busy
// or the slot is invalid, without blocking the caller.
static bool payload_trigger_slot(size_t slot)
{
    if (slot > 1) return false;
    portENTER_CRITICAL(&s_state_lock);
    const bool busy = s_payload_state == PAYLOAD_STATE_RUNNING;
    portEXIT_CRITICAL(&s_state_lock);
    if (busy) return false;
    const button_event_t event = {
        .press_time_us = 0, .release_time_us = 0, .forced_slot = (int)slot};
    return xQueueSend(s_button_queue, &event, 0) == pdTRUE;
}

static void payload_button_task(void *argument)
{
    (void)argument;
    button_event_t event;
    while (true) {
        if (xQueueReceive(s_button_queue, &event, portMAX_DELAY) != pdTRUE) continue;

        portENTER_CRITICAL(&s_state_lock);
        const bool busy = s_payload_state == PAYLOAD_STATE_RUNNING;
        portEXIT_CRITICAL(&s_state_lock);
        if (busy) continue;  // ignore presses/commands while already running

        size_t slot;
        const char *trigger;
        if (event.forced_slot >= 0) {
            slot = (size_t)event.forced_slot;
            trigger = "Grove command";
        } else {
            const int64_t held_ms =
                (event.release_time_us - event.press_time_us) / 1000;
            // Sub-30ms events are mechanical bounce, not a real press;
            // anything past the long-press ceiling is an accidental
            // extended hold.
            if (held_ms < 30 || held_ms > PAYLOAD_LONG_PRESS_MAX_MS) continue;
            slot = held_ms < PAYLOAD_SHORT_PRESS_MAX_MS ? 0 : 1;
            trigger = slot == 0 ? "short press" : "long press";
        }

        portENTER_CRITICAL(&s_state_lock);
        s_payload_state = PAYLOAD_STATE_RUNNING;
        portEXIT_CRITICAL(&s_state_lock);
        ESP_LOGI(TAG, "Payload slot %u (%s) triggered by %s", (unsigned)slot,
                 s_payload_slots[slot].name, trigger);
        payload_run_script(s_payload_slots[slot].script);
    }
}

static void initialize_payload_button(void)
{
    s_button_queue = xQueueCreate(4, sizeof(button_event_t));
    const gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << POE_P4_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(
        gpio_isr_handler_add(POE_P4_BUTTON_GPIO, button_isr_handler, NULL));
    xTaskCreate(payload_button_task, "payload_button", 4096, NULL, 4, NULL);
}

static void internet_probe_task(void *argument)
{
    (void)argument;
    while (true) {
        const ethernet_state_t before = ethernet_state_snapshot();
        const bool reachable = before.has_ip && probe_internet();
        bool changed;
        portENTER_CRITICAL(&s_state_lock);
        changed = s_ethernet.internet_reachable != reachable;
        s_ethernet.internet_reachable = reachable;
        portEXIT_CRITICAL(&s_state_lock);
        if (changed) {
            ESP_LOGI(TAG, "Internet reachability: %s", reachable ? "yes" : "no");
            broadcast_event("internet.reachability",
                            reachable ? "{\"reachable\":true}" :
                                        "{\"reachable\":false}");
        }
        vTaskDelay(pdMS_TO_TICKS(before.has_ip ? INTERNET_PROBE_INTERVAL_MS : 1000));
    }
}

static size_t build_status_json(char *buffer, size_t size)
{
    const ethernet_state_t state = ethernet_state_snapshot();
    const companion_led_state_t led_state = current_led_state(&state);
    const bool grove_connected = grove_link_connected(&state);
    float temperature_c = 0.0f;
    const bool temperature_valid = s_temperature_sensor != NULL &&
        temperature_sensor_get_celsius(s_temperature_sensor, &temperature_c) == ESP_OK;
    const char *reset_reason = current_reset_reason_name();
    char temperature_json[24];
    if (temperature_valid) {
        snprintf(temperature_json, sizeof(temperature_json), "%.1f", temperature_c);
    } else {
        snprintf(temperature_json, sizeof(temperature_json), "null");
    }
    // Not payload_state_snapshot_and_expire() -- see the matching comment on
    // grove_send_status(); this is telemetry, not the LED's own timing.
    portENTER_CRITICAL(&s_state_lock);
    const payload_run_state_t payload_state = s_payload_state;
    const size_t finding_count = s_scan_finding_count;
    portEXIT_CRITICAL(&s_state_lock);
    const char *payload_state_name;
    switch (payload_state) {
        case PAYLOAD_STATE_RUNNING: payload_state_name = "running"; break;
        case PAYLOAD_STATE_SUCCESS: payload_state_name = "success"; break;
        case PAYLOAD_STATE_ERROR: payload_state_name = "error"; break;
        case PAYLOAD_STATE_IDLE:
        default: payload_state_name = "idle"; break;
    }
    const int written = snprintf(
        buffer,
        size,
        "{\"protocol\":\"ghostwire-companion\","
        "\"protocol_version\":1,"
        "\"device\":{\"id\":\"%s\",\"model\":\"M5Stack Unit PoE-P4\","
        "\"firmware\":\"%s\"},"
        "\"capabilities\":[\"status\",\"events\",\"command\",\"payload_upload\",\"loot\"],"
        "\"ethernet\":{\"started\":%s,\"link\":%s,\"ip\":\"%s\","
        "\"netmask\":\"%s\",\"gateway\":\"%s\",\"dns\":\"%s\","
        "\"speed_mbps\":%lu,\"full_duplex\":%s},"
        "\"internet\":{\"reachable\":%s},"
        "\"ghostwire\":{\"connected\":%s},"
        "\"indicator\":{\"state\":\"%s\"},"
        "\"grove\":{\"connected\":%s,\"last_sequence\":%lu,"
        "\"valid_frames\":%lu,\"crc_errors\":%lu},"
        "\"payload\":{\"state\":\"%s\",\"finding_count\":%lu},"
        "\"system\":{\"uptime_ms\":%llu,\"reset_reason\":\"%s\","
        "\"temperature_c\":%s,\"free_heap_bytes\":%lu,"
        "\"minimum_free_heap_bytes\":%lu}}",
        s_device_id,
        COMPANION_FIRMWARE_VERSION,
        state.started ? "true" : "false",
        state.link_up ? "true" : "false",
        state.ip,
        state.netmask,
        state.gateway,
        state.dns,
        (unsigned long)state.link_speed_mbps,
        state.full_duplex ? "true" : "false",
        state.internet_reachable ? "true" : "false",
        led_state == LED_STATE_GHOSTWIRE ? "true" : "false",
        led_state_name(led_state),
        grove_connected ? "true" : "false",
        (unsigned long)state.grove_sequence,
        (unsigned long)state.grove_valid_frames,
        (unsigned long)state.grove_crc_errors,
        payload_state_name,
        (unsigned long)finding_count,
        (unsigned long long)(esp_timer_get_time() / 1000),
        reset_reason,
        temperature_json,
        (unsigned long)esp_get_free_heap_size(),
        (unsigned long)esp_get_minimum_free_heap_size());

    if (written < 0 || (size_t)written >= size) {
        return 0;
    }
    return (size_t)written;
}

static void forget_ws_client(int fd)
{
    for (size_t index = 0; index < MAX_WS_CLIENTS; ++index) {
        if (s_ws_clients[index] == fd) {
            s_ws_clients[index] = -1;
        }
    }
}

static void remember_ws_client(int fd)
{
    forget_ws_client(fd);
    for (size_t index = 0; index < MAX_WS_CLIENTS; ++index) {
        if (s_ws_clients[index] < 0) {
            s_ws_clients[index] = fd;
            return;
        }
    }
    ESP_LOGW(TAG, "WebSocket client limit reached; fd=%d is untracked", fd);
}

static esp_err_t send_ws_text(int fd, const char *payload)
{
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)payload,
        .len = strlen(payload),
    };
    return httpd_ws_send_frame_async(s_http_server, fd, &frame);
}

static void broadcast_event(const char *type, const char *data_json)
{
    char message[EVENT_JSON_SIZE];
    const uint32_t sequence = ++s_event_sequence;
    const int written = snprintf(message,
                                 sizeof(message),
                                 "{\"protocol_version\":1,\"type\":\"%s\","
                                 "\"sequence\":%lu,\"data\":%s}",
                                 type,
                                 (unsigned long)sequence,
                                 data_json);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        ESP_LOGE(TAG, "Event payload overflow for %s", type);
        return;
    }

    for (size_t index = 0; index < MAX_WS_CLIENTS; ++index) {
        const int fd = s_ws_clients[index];
        if (fd >= 0 && send_ws_text(fd, message) != ESP_OK) {
            s_ws_clients[index] = -1;
        }
    }
}

static esp_err_t status_handler(httpd_req_t *request)
{
    const int64_t contact_time_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_state_lock);
    s_ethernet.last_ghostwire_contact_us = contact_time_us;
    portEXIT_CRITICAL(&s_state_lock);
    char response[STATUS_JSON_SIZE];
    const size_t length = build_status_json(response, sizeof(response));
    if (length == 0) {
        return httpd_resp_send_500(request);
    }

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(request, response, length);
}

// Wi-Fi-only replay defense (shared/protocol/README.md's "Wi-Fi command
// channel" section): the authenticated message for this transport is
// slot||nonce, not just the slot byte Grove uses, so a genuine repeat
// command always gets a fresh nonce/tag -- only an exact replay of a
// previously *accepted* tag collides here. Checked/recorded only after a
// tag has already verified valid, so a flood of garbage requests can't
// poison this and block legitimate future commands.
// process_grove_command_request() deliberately doesn't use this: Grove is
// a physically-wired link, not a realistic sniffing target the way Wi-Fi
// is.
#define COMMAND_REPLAY_CACHE_SIZE 16
typedef struct {
    uint8_t slot;
    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    bool used;
} command_replay_entry_t;
static command_replay_entry_t s_command_replay_cache[COMMAND_REPLAY_CACHE_SIZE];
static size_t s_command_replay_next;

static bool command_replay_cache_check_and_record(uint8_t slot,
                                                   const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES])
{
    portENTER_CRITICAL(&s_state_lock);
    for (size_t i = 0; i < COMMAND_REPLAY_CACHE_SIZE; ++i) {
        if (s_command_replay_cache[i].used && s_command_replay_cache[i].slot == slot &&
            constant_time_equal(s_command_replay_cache[i].tag, tag, GHOSTWIRE_AUTH_TAG_BYTES)) {
            portEXIT_CRITICAL(&s_state_lock);
            return false;
        }
    }
    s_command_replay_cache[s_command_replay_next].slot = slot;
    memcpy(s_command_replay_cache[s_command_replay_next].tag, tag, GHOSTWIRE_AUTH_TAG_BYTES);
    s_command_replay_cache[s_command_replay_next].used = true;
    s_command_replay_next = (s_command_replay_next + 1) % COMMAND_REPLAY_CACHE_SIZE;
    portEXIT_CRITICAL(&s_state_lock);
    return true;
}

// Handles "POST /v1/command" -- {"slot":0,"nonce":<uint32>,"tag":"<16 hex
// chars>"} -> {"accepted":bool}. Bare boolean, no reason, matching the
// Grove "K" frame's "reveal nothing about why" design (shared/protocol/
// README.md). Malformed requests get a plain 400; anything well-formed but
// rejected (bad tag / not paired / replay / busy) is a normal 200 with
// accepted:false, so the two failure classes aren't distinguishable from
// the response body alone.
#define COMMAND_BODY_MAX 128
static esp_err_t command_handler(httpd_req_t *request)
{
    if (request->content_len <= 0 || request->content_len > COMMAND_BODY_MAX) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }
    char body[COMMAND_BODY_MAX + 1];
    int received = 0;
    while (received < request->content_len) {
        const int result = httpd_req_recv(request, body + received,
                                          request->content_len - received);
        if (result <= 0) {
            if (result == HTTPD_SOCK_ERR_TIMEOUT) continue;
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "read failed");
            return ESP_FAIL;
        }
        received += result;
    }
    body[received] = '\0';

    cJSON *root = cJSON_ParseWithLength(body, (size_t)received);
    if (root == NULL) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }
    const cJSON *slot_item = cJSON_GetObjectItemCaseSensitive(root, "slot");
    const cJSON *nonce_item = cJSON_GetObjectItemCaseSensitive(root, "nonce");
    const cJSON *tag_item = cJSON_GetObjectItemCaseSensitive(root, "tag");
    if (!cJSON_IsNumber(slot_item) || !cJSON_IsNumber(nonce_item) ||
        !cJSON_IsString(tag_item)) {
        cJSON_Delete(root);
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "missing/invalid fields");
        return ESP_FAIL;
    }
    const int slot_value = slot_item->valueint;
    // valueint is a signed 32-bit int -- too narrow for the full uint32_t
    // nonce range. cJSON always stores the parsed number as a double
    // (valuedouble), which represents every uint32_t exactly, so read the
    // nonce from there instead.
    const uint32_t nonce = (uint32_t)nonce_item->valuedouble;
    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    const bool tag_ok = hex_decode(tag, sizeof(tag), tag_item->valuestring);
    cJSON_Delete(root);
    if (!tag_ok) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "malformed tag");
        return ESP_FAIL;
    }

    bool accepted = false;
    if (!s_session_key_valid) {
        ESP_LOGW(TAG, "Wi-Fi command received but not paired; rejecting");
    } else if (slot_value < 0 || slot_value > 1) {
        ESP_LOGW(TAG, "Wi-Fi command has an invalid slot; rejecting");
    } else {
        const uint8_t slot_byte = (uint8_t)slot_value;
        const uint8_t message[5] = {
            slot_byte,
            (uint8_t)(nonce >> 24), (uint8_t)(nonce >> 16),
            (uint8_t)(nonce >> 8), (uint8_t)nonce,
        };
        if (!ghostwire_auth_verify_tag(s_session_key, sizeof(s_session_key), message,
                                       sizeof(message), time(NULL), tag)) {
            ESP_LOGW(TAG, "Wi-Fi command failed auth verification; rejecting");
        } else if (!command_replay_cache_check_and_record(slot_byte, tag)) {
            ESP_LOGW(TAG, "Wi-Fi command replay detected; rejecting");
        } else {
            accepted = payload_trigger_slot((size_t)slot_value);
            if (!accepted) {
                ESP_LOGW(TAG, "Wi-Fi command authenticated but payload engine is "
                              "busy; rejecting");
            }
        }
    }

    char response[32];
    const int response_length =
        snprintf(response, sizeof(response), "{\"accepted\":%s}",
                accepted ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, response, response_length);
}

// Handles "POST /v1/payload" -- {"slot":0,"nonce":<uint32>,
// "tag":"<16 hex chars>","script":"..."} -> {"accepted":bool}. The
// authenticated message is slot||nonce||sha256(script) (37 bytes) rather
// than the raw script bytes -- keeps the existing
// ghostwire_auth_compute_tag()/_verify_tag() internal concatenation buffer
// untouched regardless of script length, while still cryptographically
// binding the accepted script to the tag (a tampered-in-transit script
// hashes differently, so its tag no longer verifies). Same replay cache,
// busy-check, and "bare true/false, no reason" response shape as
// command_handler().
#define PAYLOAD_UPLOAD_BODY_MAX (PAYLOAD_SCRIPT_MAX_BYTES * 2 + 128)
static esp_err_t payload_handler(httpd_req_t *request)
{
    if (request->content_len <= 0 || request->content_len > PAYLOAD_UPLOAD_BODY_MAX) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }
    char *body = malloc(request->content_len + 1);
    if (body == NULL) return httpd_resp_send_500(request);
    int received = 0;
    while (received < request->content_len) {
        const int result = httpd_req_recv(request, body + received,
                                          request->content_len - received);
        if (result <= 0) {
            if (result == HTTPD_SOCK_ERR_TIMEOUT) continue;
            free(body);
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "read failed");
            return ESP_FAIL;
        }
        received += result;
    }
    body[received] = '\0';

    cJSON *root = cJSON_ParseWithLength(body, (size_t)received);
    free(body);
    if (root == NULL) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }
    const cJSON *slot_item = cJSON_GetObjectItemCaseSensitive(root, "slot");
    const cJSON *nonce_item = cJSON_GetObjectItemCaseSensitive(root, "nonce");
    const cJSON *tag_item = cJSON_GetObjectItemCaseSensitive(root, "tag");
    const cJSON *script_item = cJSON_GetObjectItemCaseSensitive(root, "script");
    if (!cJSON_IsNumber(slot_item) || !cJSON_IsNumber(nonce_item) ||
        !cJSON_IsString(tag_item) || !cJSON_IsString(script_item) ||
        strlen(script_item->valuestring) >= PAYLOAD_SCRIPT_MAX_BYTES) {
        cJSON_Delete(root);
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "missing/invalid fields");
        return ESP_FAIL;
    }
    const int slot_value = slot_item->valueint;
    const uint32_t nonce = (uint32_t)nonce_item->valuedouble;
    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    const bool tag_ok = hex_decode(tag, sizeof(tag), tag_item->valuestring);
    char script[PAYLOAD_SCRIPT_MAX_BYTES];
    snprintf(script, sizeof(script), "%s", script_item->valuestring);
    cJSON_Delete(root);
    if (!tag_ok) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "malformed tag");
        return ESP_FAIL;
    }

    bool accepted = false;
    if (!s_session_key_valid) {
        ESP_LOGW(TAG, "Wi-Fi script upload received but not paired; rejecting");
    } else if (slot_value < 0 || slot_value > 1) {
        ESP_LOGW(TAG, "Wi-Fi script upload has an invalid slot; rejecting");
    } else {
        uint8_t script_hash[32];
        mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  (const unsigned char *)script, strlen(script), script_hash);
        const uint8_t slot_byte = (uint8_t)slot_value;
        uint8_t message[1 + 4 + sizeof(script_hash)];
        message[0] = slot_byte;
        message[1] = (uint8_t)(nonce >> 24);
        message[2] = (uint8_t)(nonce >> 16);
        message[3] = (uint8_t)(nonce >> 8);
        message[4] = (uint8_t)nonce;
        memcpy(message + 5, script_hash, sizeof(script_hash));
        if (!ghostwire_auth_verify_tag(s_session_key, sizeof(s_session_key), message,
                                       sizeof(message), time(NULL), tag)) {
            ESP_LOGW(TAG, "Wi-Fi script upload failed auth verification; rejecting");
        } else if (!command_replay_cache_check_and_record(slot_byte, tag)) {
            ESP_LOGW(TAG, "Wi-Fi script upload replay detected; rejecting");
        } else {
            portENTER_CRITICAL(&s_state_lock);
            const bool busy = s_payload_state == PAYLOAD_STATE_RUNNING;
            portEXIT_CRITICAL(&s_state_lock);
            if (busy) {
                ESP_LOGW(TAG, "Wi-Fi script upload authenticated but slot %u is "
                              "running; rejecting", (unsigned)slot_byte);
            } else {
                accepted = payload_store_slot_script(slot_byte, script);
                if (accepted) {
                    ESP_LOGI(TAG, "Slot %u script updated over Wi-Fi (%u bytes)",
                            (unsigned)slot_byte, (unsigned)strlen(script));
                }
            }
        }
    }

    char response[32];
    const int response_length =
        snprintf(response, sizeof(response), "{\"accepted\":%s}",
                accepted ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, response, response_length);
}

// Handles "POST /v1/loot" -- {"nonce":<uint32>,"tag":"<16 hex chars>"} ->
// {"entries":[{"ip":"...","port":80},...]}. Read-only, so no replay-cache
// entry is recorded (nothing to protect against replaying a read), but
// still requires a valid paired session -- scan results are recon output,
// not public telemetry like /v1/status.
#define LOOT_REQUEST_BODY_MAX 128
static esp_err_t loot_handler(httpd_req_t *request)
{
    if (request->content_len <= 0 || request->content_len > LOOT_REQUEST_BODY_MAX) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }
    char body[LOOT_REQUEST_BODY_MAX + 1];
    int received = 0;
    while (received < request->content_len) {
        const int result = httpd_req_recv(request, body + received,
                                          request->content_len - received);
        if (result <= 0) {
            if (result == HTTPD_SOCK_ERR_TIMEOUT) continue;
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "read failed");
            return ESP_FAIL;
        }
        received += result;
    }
    body[received] = '\0';

    cJSON *root = cJSON_ParseWithLength(body, (size_t)received);
    if (root == NULL) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }
    const cJSON *nonce_item = cJSON_GetObjectItemCaseSensitive(root, "nonce");
    const cJSON *tag_item = cJSON_GetObjectItemCaseSensitive(root, "tag");
    if (!cJSON_IsNumber(nonce_item) || !cJSON_IsString(tag_item)) {
        cJSON_Delete(root);
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "missing/invalid fields");
        return ESP_FAIL;
    }
    const uint32_t nonce = (uint32_t)nonce_item->valuedouble;
    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    const bool tag_ok = hex_decode(tag, sizeof(tag), tag_item->valuestring);
    cJSON_Delete(root);
    if (!tag_ok) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "malformed tag");
        return ESP_FAIL;
    }

    bool authorized = false;
    if (!s_session_key_valid) {
        ESP_LOGW(TAG, "Wi-Fi loot request received but not paired; rejecting");
    } else {
        const uint8_t message[4] = {
            (uint8_t)(nonce >> 24), (uint8_t)(nonce >> 16),
            (uint8_t)(nonce >> 8), (uint8_t)nonce,
        };
        authorized = ghostwire_auth_verify_tag(s_session_key, sizeof(s_session_key),
                                               message, sizeof(message), time(NULL), tag);
        if (!authorized) {
            ESP_LOGW(TAG, "Wi-Fi loot request failed auth verification; rejecting");
        }
    }
    if (!authorized) {
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        return httpd_resp_send(request, "{\"entries\":[]}", HTTPD_RESP_USE_STRLEN);
    }

    portENTER_CRITICAL(&s_state_lock);
    const size_t entry_count = s_loot_entry_count;
    loot_entry_t entries[LOOT_MAX_ENTRIES];
    memcpy(entries, s_loot_entries, entry_count * sizeof(loot_entry_t));
    portEXIT_CRITICAL(&s_state_lock);

    // LOOT_MAX_ENTRIES(64) x ~40 bytes/entry comfortably fits; sized with
    // headroom rather than computed exactly.
    char response[LOOT_MAX_ENTRIES * 48 + 32];
    int offset = snprintf(response, sizeof(response), "{\"entries\":[");
    for (size_t i = 0; i < entry_count && offset > 0 && (size_t)offset < sizeof(response); ++i) {
        offset += snprintf(response + offset, sizeof(response) - (size_t)offset,
                           "%s{\"ip\":\"%s\",\"port\":%u}", i == 0 ? "" : ",",
                           entries[i].ip, (unsigned)entries[i].port);
    }
    if (offset < 0 || (size_t)offset >= sizeof(response) - 2) {
        return httpd_resp_send_500(request);
    }
    offset += snprintf(response + offset, sizeof(response) - (size_t)offset, "]}");

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, response, offset);
}

static esp_err_t events_handler(httpd_req_t *request)
{
    const int fd = httpd_req_to_sockfd(request);
    if (request->method == HTTP_GET) {
        char ready[EVENT_JSON_SIZE];
        const int64_t contact_time_us = esp_timer_get_time();
        portENTER_CRITICAL(&s_state_lock);
        s_ethernet.last_ghostwire_contact_us = contact_time_us;
        portEXIT_CRITICAL(&s_state_lock);
        remember_ws_client(fd);
        const uint32_t sequence = ++s_event_sequence;
        snprintf(ready,
                 sizeof(ready),
                 "{\"protocol_version\":1,\"type\":\"companion.ready\","
                 "\"sequence\":%lu,\"data\":{\"device_id\":\"%s\","
                 "\"firmware\":\"%s\"}}",
                 (unsigned long)sequence,
                 s_device_id,
                 COMPANION_FIRMWARE_VERSION);
        return send_ws_text(fd, ready);
    }

    httpd_ws_frame_t incoming = {0};
    esp_err_t error = httpd_ws_recv_frame(request, &incoming, 0);
    if (error != ESP_OK) {
        forget_ws_client(fd);
        return error;
    }
    if (incoming.len > 1024) {
        forget_ws_client(fd);
        return ESP_ERR_INVALID_SIZE;
    }

    if (incoming.len > 0) {
        uint8_t discard[1024];
        incoming.payload = discard;
        error = httpd_ws_recv_frame(request, &incoming, incoming.len);
        if (error != ESP_OK) {
            forget_ws_client(fd);
            return error;
        }
    }

    return send_ws_text(
        fd,
        "{\"protocol_version\":1,\"type\":\"error\",\"sequence\":0,"
        "\"data\":{\"code\":\"unsupported\","
        "\"message\":\"Protocol v1 event sockets are read-only\"}}");
}

static esp_err_t not_found_handler(httpd_req_t *request, httpd_err_code_t error)
{
    (void)error;
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_status(request, "404 Not Found");
    return httpd_resp_sendstr(request, "{\"error\":\"not_found\"}");
}

static void start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = COMPANION_HTTP_PORT;
    config.max_open_sockets = MAX_WS_CLIENTS + 2;
    config.lru_purge_enable = true;
    // HTTPD_DEFAULT_CONFIG()'s 4096 default was already marginal (newlib's
    // %f formatting -- used for build_status_json()'s temperature field --
    // pulls in _dtoa_r/_Balloc, which are stack-hungry) and a real "Stack
    // protection fault" panic-reboot loop was reproduced on hardware once
    // build_status_json() grew (capabilities/payload additions for the
    // Wi-Fi command channel). Headroom, not a tuned minimum.
    config.stack_size = 8192;

    ESP_ERROR_CHECK(httpd_start(&s_http_server, &config));
    ESP_ERROR_CHECK(httpd_register_err_handler(s_http_server, HTTPD_404_NOT_FOUND, not_found_handler));

    const httpd_uri_t status_uri = {
        .uri = "/v1/status",
        .method = HTTP_GET,
        .handler = status_handler,
    };
    const httpd_uri_t events_uri = {
        .uri = "/v1/events",
        .method = HTTP_GET,
        .handler = events_handler,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };
    const httpd_uri_t command_uri = {
        .uri = "/v1/command",
        .method = HTTP_POST,
        .handler = command_handler,
    };
    const httpd_uri_t payload_uri = {
        .uri = "/v1/payload",
        .method = HTTP_POST,
        .handler = payload_handler,
    };
    const httpd_uri_t loot_uri = {
        .uri = "/v1/loot",
        .method = HTTP_POST,
        .handler = loot_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &status_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &events_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &command_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &payload_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &loot_uri));
    ESP_LOGI(TAG, "HTTP/WebSocket server listening on port %d", COMPANION_HTTP_PORT);
}

static void start_mdns(void)
{
    static bool initialized;
    if (initialized) {
        return;
    }

    mdns_txt_item_t records[] = {
        {"proto", "1"},
        {"role", "poe"},
        {"model", "unit-poe-p4"},
        {"path", "/v1/status"},
    };
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(COMPANION_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set(COMPANION_INSTANCE));
    ESP_ERROR_CHECK(mdns_service_add(COMPANION_INSTANCE,
                                     "_ghostwire",
                                     "_tcp",
                                     COMPANION_HTTP_PORT,
                                     records,
                                     sizeof(records) / sizeof(records[0])));
    initialized = true;
    ESP_LOGI(TAG, "Discovery ready: %s.local _ghostwire._tcp", COMPANION_HOSTNAME);
}

static void ethernet_event_handler(void *argument,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void *event_data)
{
    (void)argument;
    (void)event_base;
    (void)event_data;

    switch (event_id) {
        case ETHERNET_EVENT_START:
            portENTER_CRITICAL(&s_state_lock);
            s_ethernet.started = true;
            portEXIT_CRITICAL(&s_state_lock);
            ESP_LOGI(TAG, "Ethernet started");
            broadcast_event("ethernet.started", "{\"started\":true}");
            break;
        case ETHERNET_EVENT_CONNECTED: {
            uint8_t mac[6];
            eth_speed_t speed = ETH_SPEED_10M;
            eth_duplex_t duplex = ETH_DUPLEX_HALF;
            ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, mac));
            ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_G_SPEED, &speed));
            ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_G_DUPLEX_MODE, &duplex));
            portENTER_CRITICAL(&s_state_lock);
            s_ethernet.link_up = true;
            s_ethernet.link_speed_mbps = speed == ETH_SPEED_100M ? 100 : 10;
            s_ethernet.full_duplex = duplex == ETH_DUPLEX_FULL;
            portEXIT_CRITICAL(&s_state_lock);
            ESP_LOGI(TAG,
                     "Ethernet link up: %02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            broadcast_event("ethernet.link", "{\"up\":true}");
            break;
        }
        case ETHERNET_EVENT_DISCONNECTED:
            portENTER_CRITICAL(&s_state_lock);
            s_ethernet.link_up = false;
            s_ethernet.has_ip = false;
            s_ethernet.internet_reachable = false;
            s_ethernet.ip[0] = '\0';
            s_ethernet.netmask[0] = '\0';
            s_ethernet.gateway[0] = '\0';
            s_ethernet.dns[0] = '\0';
            s_ethernet.link_speed_mbps = 0;
            s_ethernet.full_duplex = false;
            portEXIT_CRITICAL(&s_state_lock);
            ESP_LOGW(TAG, "Ethernet link down");
            broadcast_event("ethernet.link", "{\"up\":false}");
            break;
        case ETHERNET_EVENT_STOP:
            portENTER_CRITICAL(&s_state_lock);
            s_ethernet.started = false;
            s_ethernet.link_up = false;
            portEXIT_CRITICAL(&s_state_lock);
            ESP_LOGW(TAG, "Ethernet stopped");
            broadcast_event("ethernet.started", "{\"started\":false}");
            break;
        default:
            break;
    }
}

static void got_ip_event_handler(void *argument,
                                 esp_event_base_t event_base,
                                 int32_t event_id,
                                 void *event_data)
{
    (void)argument;
    (void)event_base;
    (void)event_id;
    const ip_event_got_ip_t *event = event_data;
    char event_json[128];
    esp_netif_dns_info_t dns = {0};
    char dns_address[16] = "";
    if (s_eth_netif != NULL &&
        esp_netif_get_dns_info(s_eth_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK &&
        dns.ip.type == ESP_IPADDR_TYPE_V4) {
        copy_ipv4(dns_address, &dns.ip.u_addr.ip4);
    }

    portENTER_CRITICAL(&s_state_lock);
    copy_ipv4(s_ethernet.ip, &event->ip_info.ip);
    copy_ipv4(s_ethernet.netmask, &event->ip_info.netmask);
    copy_ipv4(s_ethernet.gateway, &event->ip_info.gw);
    snprintf(s_ethernet.dns, sizeof(s_ethernet.dns), "%s", dns_address);
    s_ethernet.has_ip = true;
    portEXIT_CRITICAL(&s_state_lock);

    ESP_LOGI(TAG, "Ethernet address: %s", s_ethernet.ip);
    snprintf(event_json, sizeof(event_json), "{\"ip\":\"%s\"}", s_ethernet.ip);
    broadcast_event("ethernet.ip", event_json);
    start_mdns();
}

static void initialize_device_id(void)
{
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_ETH));
    snprintf(s_device_id,
             sizeof(s_device_id),
             "poe-p4-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void initialize_ethernet(void)
{
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    phy_config.phy_addr = POE_P4_PHY_ADDR;
    phy_config.reset_gpio_num = POE_P4_PHY_RESET_GPIO;
    emac_config.smi_gpio.mdc_num = POE_P4_MDC_GPIO;
    emac_config.smi_gpio.mdio_num = POE_P4_MDIO_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    ESP_ERROR_CHECK(mac == NULL || phy == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &s_eth_handle));

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_config);
    ESP_ERROR_CHECK(s_eth_netif == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle)));

    ESP_ERROR_CHECK(esp_event_handler_register(
        ETH_EVENT, ESP_EVENT_ANY_ID, ethernet_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
}

// UTC wall-clock time is a hard prerequisite for the time-windowed command
// authentication scheme in shared/protocol/auth.h -- esp_timer_get_time()
// alone is only monotonic since boot, not wall-clock. Mirrors the
// Cardputer's own NTP server list (src/main.cpp's TimeStatus screen) for
// consistency. Non-blocking: starts as soon as the netif/event loop exist
// and syncs opportunistically once Ethernet has a route out.
static void sntp_time_sync_cb(struct timeval *tv)
{
    (void)tv;
    const time_t now = time(NULL);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "NTP time synchronized: %s UTC", buffer);
}

static void initialize_time_sync(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        3, ESP_SNTP_SERVER_LIST("time.cloudflare.com", "pool.ntp.org",
                                "time.google.com"));
    config.sync_cb = sntp_time_sync_cb;
    config.wait_for_sync = false;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));
}

void app_main(void)
{
    initialize_status_led();
    const temperature_sensor_config_t temperature_config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    if (temperature_sensor_install(&temperature_config, &s_temperature_sensor) == ESP_OK) {
        if (temperature_sensor_enable(s_temperature_sensor) != ESP_OK) {
            s_temperature_sensor = NULL;
        }
    }
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialize_pairing_rng();
    pairing_load_session_key();
    payload_load_slot_scripts();
    self_test_auth_tag();
    initialize_device_id();
    initialize_payload_button();
    // 8192 rather than the 4096 other tasks here use: Grove pairing's ECDH
    // math (mbedtls, on-demand and rare, not per-loop) needs more headroom
    // than the everyday heartbeat/status/identity frame handling does.
    xTaskCreate(grove_uart_task, "grove_uart", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "Ghostwire companion %s (%s)", COMPANION_FIRMWARE_VERSION, s_device_id);
    start_http_server();
    initialize_ethernet();
    initialize_time_sync();
    portENTER_CRITICAL(&s_state_lock);
    s_ethernet.firmware_ready = true;
    portEXIT_CRITICAL(&s_state_lock);
    xTaskCreate(internet_probe_task, "internet_probe", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "GHOSTWIRE_POE_P4_READY");
}
