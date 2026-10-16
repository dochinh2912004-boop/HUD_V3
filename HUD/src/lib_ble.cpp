#include "lib_ble.h"
#include "lib_led.h"
#include "lib_buzzer.h"
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <NimBLEDevice.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "mbedtls/sha256.h"
#include "mbedtls/gcm.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/bignum.h"
#include "mbedtls/platform_util.h"

/* Fallback for mbedTLS 2.x, which never defines this macro at all (so
 * MBEDTLS_ALLOW_PRIVATE_ACCESS above is simply unused there). Kept from the
 * .ino so one source still builds on either mbedTLS generation. */
#ifndef MBEDTLS_PRIVATE
#define MBEDTLS_PRIVATE(name) name
#endif

/* ============================================================================
 *  PLATFORM SHIMS — Arduino-shaped calls the rest of this file still uses,
 *  implemented directly on ESP-IDF so the protocol code below did not need
 *  to be touched line-by-line.
 * ==========================================================================*/

/* millis()/delay() — same names and semantics as arduino-esp32's, so every
 * deadline/timeout computation below (§13, §12.5, claim window, heartbeat)
 * is unchanged. */
static inline uint32_t millis() {
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
}
static inline void delay(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

/* digitalRead()/pinMode()/INPUT_PULLUP/LOW — just enough of the Arduino GPIO
 * surface for claim_button_poll() and setup below. */
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 1



static inline void pinMode(int pin, int mode) {
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = 1ULL << pin;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = (mode == INPUT_PULLUP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);
}
static inline int digitalRead(int pin) {
  return gpio_get_level((gpio_num_t)pin);
}

/* Preferences — a drop-in for the small slice of Arduino's NVS wrapper the
 * ownership record (§3/§14) uses: begin/isKey/getBytes/putBytes. Implemented
 * directly on ESP-IDF's nvs.h so own_slot_read()/own_slot_write()/
 * ownership_load() below are byte-for-byte what the .ino had. */
class Preferences {
  nvs_handle_t h_ = 0;
  bool open_ = false;
public:
  bool begin(const char *ns, bool readOnly) {
    esp_err_t err = nvs_open(ns, readOnly ? NVS_READONLY : NVS_READWRITE, &h_);
    open_ = (err == ESP_OK);
    return open_;
  }
  bool isKey(const char *key) {
    if (!open_) return false;
    size_t sz = 0;
    return nvs_get_blob(h_, key, nullptr, &sz) == ESP_OK;
  }
  size_t getBytes(const char *key, void *buf, size_t maxLen) {
    if (!open_) return 0;
    size_t sz = maxLen;
    if (nvs_get_blob(h_, key, buf, &sz) != ESP_OK) return 0;
    return sz;
  }
  size_t putBytes(const char *key, const void *buf, size_t len) {
    if (!open_) return 0;
    if (nvs_set_blob(h_, key, buf, len) != ESP_OK) return 0;
    if (nvs_commit(h_) != ESP_OK) return 0;
    return len;
  }
};

/* Forward declarations. Not required by g++/ESP-IDF the way arduino-esp32's
 * ctags-based prototype generation required them in the .ino, but harmless
 * to keep — some functions below reference these types before their full
 * definitions appear further down the file. */
struct CommandPacket;
struct HelloPayload;
struct BindPayload;
struct UnbindPayload;
struct HelloOut;
struct own_rec_t;
enum OwnershipAction : uint8_t;
enum Epoch : uint8_t;
enum Auth : uint8_t;

/* ============================================================================
 *  §0  DEVICE IDENTITY — PORT ME
 * ==========================================================================*/

/* 6-byte TEST device UID. Must match what the test backend provisioned. It is
 * published in DIS Serial Number as 12 UPPERCASE hex chars (§2.1) and hashed
 * into the §5.4 transcript, so a mismatch fails every proof.
 * Temporary customer-test credential issued for this evaluation kit.
 * Do not use on production devices. */
static uint8_t DEVICE_UID[6]; 

static const char *DIS_MODEL        = "WYN-REF-01";
static const char *DIS_FW_REV       = "1.0.0";
static const char *DIS_MANUFACTURER = "WYN";
static const char *ADV_NAME         = "HUD_MCD91";

/* §4 — TEST device_claim_key, 32 bytes. Temporary customer-test credential
 * issued for this evaluation kit. Revoke after integration; production keeps
 * each device's unique claim key in eFuse/secure hardware, where firmware can
 * ask the hardware to HMAC with it but can never read it (see factory_hmac). */
static uint8_t DEVICE_CLAIM_KEY[32] ;

/* §15 — manufacturer ECDSA P-256 trust anchors, uncompressed 0x04||X||Y.
 * Tried in ascending index order; an all-zero slot is unused.
 * Slot 0: the WYN manufacturer anchor from the production guide. Public key
 * material only — no private signing key is ever present in firmware. */
#define MAX_TRUST_KEYS 2
static const uint8_t MANUFACTURER_PUBKEYS[MAX_TRUST_KEYS][65] = {
  { 0x04, 0xda, 0x50, 0x99, 0xa2, 0x04, 0xc9, 0x97, 0xb6, 0x39, 0x8b, 0xcd,
    0xba, 0xc4, 0x00, 0x81, 0xd3, 0xec, 0x19, 0x11, 0x8d, 0x21, 0x4f, 0x44,
    0x21, 0xa0, 0xe4, 0xbb, 0x67, 0x86, 0xfe, 0xc7, 0xa3, 0x3e, 0x12, 0x2c,
    0x4c, 0x33, 0xcd, 0x33, 0x43, 0x76, 0x88, 0x25, 0x94, 0x11, 0xf5, 0x14,
    0xed, 0xf9, 0xf9, 0xf8, 0x66, 0xfa, 0x25, 0x16, 0x45, 0x3b, 0x2a, 0xaa,
    0x84, 0x53, 0xcc, 0xbe, 0x04 },
  { 0x00 },   /* unused slot */
};

/* §4 — the physical claim button. PORT ME (or drive claim_open_window()
 * from your own UI). Active-low with pullup here.
 * BOOT button is GPIO9 on ESP32-C3 (strapping pin), GPIO0 on classic
 * ESP32/S3 — the two devkits wire it differently, so this can't be one
 * constant across boards. */
#if CONFIG_IDF_TARGET_ESP32C3
#define CLAIM_BUTTON_PIN   10
#else
#define CLAIM_BUTTON_PIN   0
#endif
#define CLAIM_HOLD_MS      20
#define CLAIM_WINDOW_MS    60000

/* Reported in EVT_DEVICE_CAP (§11.3) — PORT ME. */
#define CAP_FIRMWARE_VERSION   0x00010000u
#define CAP_HARDWARE_REVISION  "rev-a"
#define CAP_DISPLAY_W          480
#define CAP_DISPLAY_H          480

/* ============================================================================
 *  §1/§2  PROTOCOL CONSTANTS  (do not change — these are the wire contract)
 * ==========================================================================*/

#define UUID_SERVICE_CMD   "57422000-a32c-4f60-bdd1-beefaca01aba"
#define UUID_CHAR_COMMAND  "57422001-a32c-4f60-bdd1-beefaca01aba"   /* WRITE  */
#define UUID_CHAR_EVENT    "57422002-a32c-4f60-bdd1-beefaca01aba"   /* INDIC. */
#define UUID_SERVICE_DIS   "180A"

#define PROTOCOL_VERSION   0x00010001u   /* 1.01 */

/* §11.1 — value 0 is UNSPECIFIED everywhere and is never executable. */
enum CommandType : int32_t {
  CMD_GET_DEVICE_CAP        = 1,
  CMD_UPDATE_INFO           = 4,
  CMD_BULK                  = 5,
  CMD_BIND                  = 7,
  CMD_APPLY_OWNERSHIP_TOKEN = 8,
  CMD_HELLO                 = 9,
};
enum EventType : int32_t { EVT_DEVICE_CAP = 1, EVT_ACK = 2, EVT_HELLO_ACK = 3 };
enum StatusCode : int32_t { STATUS_OK = 1, STATUS_ERROR = 2, STATUS_DENIED = 3 };
enum InfoGroup : int32_t {
  INFO_GROUP_WYN_SIGN = 1, INFO_GROUP_ANDROID_AUTO = 2, INFO_GROUP_MAP_NAVIGATION = 3
};
enum OwnershipAction : uint8_t { OWNERSHIP_ACTION_BIND = 1, OWNERSHIP_ACTION_UNBIND = 2 };

/* §5 crypto sizes */
#define X25519_KEY_LEN     32
#define TRANSCRIPT_LEN     32
#define SESSION_ID_LEN     4
#define GCM_TAG_LEN        16
#define CHALLENGE_LEN      16
#define DEVICE_PROOF_LEN   16
#define OWNER_KEY_LEN      32

/* §5.4 canonical handshake context — fixed 127 bytes, never protobuf. */
#define HS_CONTEXT_LEN         127
#define HS_MAGIC               "WYN-HS-101"
#define HS_OFF_MAGIC            0    /* 10  ASCII                          */
#define HS_OFF_VERSION          10   /*  4  protocol version, big-endian   */
#define HS_OFF_DEVICE_UID       14   /*  6                                 */
#define HS_OFF_CENTRAL_PUB      20   /* 32                                 */
#define HS_OFF_PERIPHERAL_PUB   52   /* 32                                 */
#define HS_OFF_SESSION_ID       84   /*  4                                 */
#define HS_OFF_BOUND            88   /*  1                                 */
#define HS_OFF_PHYS_AUTH        89   /*  1                                 */
#define HS_OFF_BINDING_VER      90   /*  4  big-endian                     */
#define HS_OFF_PERIPH_CHAL      94   /* 16                                 */
#define HS_OFF_CHAL_PRESENT     110  /*  1                                 */
#define HS_OFF_SERVER_CHAL      111  /* 16, zero-filled when absent        */

/* §5.4 / §18 HMAC domain prefixes over device_claim_key */
#define HMAC_PREFIX_DEVICE_PROOF  0x02
#define HMAC_PREFIX_APPLY_PROOF   0x03

/* §18 post_apply_proof input — fixed 45 bytes */
#define APPLY_PROOF_INPUT_LEN  45
#define AP_OFF_DOMAIN      0
#define AP_OFF_ACTION      1
#define AP_OFF_DEVICE_UID  2
#define AP_OFF_VERSION     8
#define AP_OFF_BOUND       12
#define AP_OFF_TRANSCRIPT  13

/* §8 nonce = session_id[4] || direction[1] || uint56_be(counter)[7] */
#define NONCE_LEN            12
#define DIR_C2P              0x00
#define DIR_P2C              0x01
#define RECORD_COUNTER_MAX   0x00FFFFFFFFFFFFFFull

/* §9 token layouts */
/* The token's action byte IS the OwnershipAction value (§9.1/§18) — one set
 * of names, so a mismatch between the two is unrepresentable. */
#define TOKEN_FORMAT             0x01
#define BIND_TOKEN_BODY_LEN      76
#define BIND_TOKEN_LEN           140
#define UNBIND_TOKEN_BODY_LEN    44
#define UNBIND_TOKEN_LEN         108
#define TOK_OFF_FORMAT           0
#define TOK_OFF_ACTION           1
#define TOK_OFF_DEVICE_UID       2
#define TOK_OFF_BINDING_VER      8
#define BIND_TOK_OFF_KEY_HASH    12
#define BIND_TOK_OFF_TRANSCRIPT  44
#define UNBIND_TOK_OFF_TRANSCRIPT 12
#define ECDSA_SIG_LEN            64

/* §12 fragmentation */
enum FragmentMarker : uint8_t { FRAG_FIRST = 0x00, FRAG_CONT = 0x01,
                                FRAG_LAST = 0x02,  FRAG_SINGLE = 0x03 };
#define FRAG_MAX_FRAME     4096
#define FRAG_TIMEOUT_MS    5000
#define MTU_MIN            23
#define ATT_MAX_VALUE_LEN  512

/* §13 deadlines / budgets */
#define HELLO_DEADLINE_MS    10000
#define PRE_OWN_IDLE_MS      30000   /* §20 "ownership command sau HELLO"   */
#define PROBE_IDLE_MS        30000   /* §12.3 probe-only state self-close   */
#define OWNER_CONFIRM_MS     10000
#define MAX_ECDSA_FAILURES   3
#define BULK_MAX_ITEMS       16

/* ============================================================================
 *  CONSOLE TRACE
 *
 *  This board has no display, so the console log IS the user interface: it
 *  has to be enough to tell, from a terminal alone, exactly how far a
 *  handshake got and why it stopped. ESP-IDF routes stdout to the console
 *  UART by default, so plain printf() replaces Serial.print*() here.
 *
 *      WYN_TRACE 0   operational LOGI/LOGE messages only
 *                1   + diagnostic LOGD messages (default for integration)
 *                2   + fragment-level logs and public handshake dumps (noisy)
 *
 *  §9.3/§14: NEVER log owner_key, pre_own_key, device_claim_key, session keys
 *  or raw token bytes. Level 2 prints public/session-specific handshake data
 *  for integration diagnosis; disable it outside controlled test sessions.
 * ==========================================================================*/

#define WYN_TRACE 1

/* Bump on every change to this module — printed in every log line so a
 * console capture can be matched back to the exact source revision. */
#define SKETCH_VERSION "2.0.4-27131-idf"

/* Prefix, no newline — for lines built out of conditional pieces. */
#define LOGRAW(...) printf("[wyn][" SKETCH_VERSION "] " __VA_ARGS__)

#define LOGI(...)  do { printf("[wyn][" SKETCH_VERSION "] "  __VA_ARGS__); printf("\n"); } while (0)
#define LOGE(...)  do { printf("[wyn][" SKETCH_VERSION "] !! " __VA_ARGS__); printf("\n"); } while (0)
#if WYN_TRACE >= 1
#  define LOGD(...) do { printf("[wyn][" SKETCH_VERSION "]    " __VA_ARGS__); printf("\n"); } while (0)
#else
#  define LOGD(...) do { } while (0)
#endif
#if WYN_TRACE >= 2
#  define LOGV(...) do { printf("[wyn][" SKETCH_VERSION "]      " __VA_ARGS__); printf("\n"); } while (0)
#  define DUMP(label, p, n) hexdump(label, p, n)
#else
#  define LOGV(...) do { } while (0)
#  define DUMP(label, p, n) do { } while (0)
#endif

__attribute__((unused))
static void hexdump(const char *label, const uint8_t *p, size_t n) {
  LOGRAW("     %s[%u] ", label, (unsigned)n);
  for (size_t i = 0; i < n; i++) printf("%02x", p[i]);
  printf("\n");
}

/* Short names so the log reads without a spec open next to it. */
static const char *cmd_name(int32_t t) {
  switch (t) {
    case 1: return "GET_DEVICE_CAP"; case 4: return "UPDATE_INFO";
    case 5: return "BULK";           case 7: return "BIND";
    case 8: return "APPLY_OWNERSHIP_TOKEN"; case 9: return "HELLO";
    default: return "UNKNOWN";
  }
}
static const char *evt_name(int32_t t) {
  switch (t) { case 1: return "DEVICE_CAP"; case 2: return "ACK"; case 3: return "HELLO_ACK";
               default: return "UNKNOWN"; }
}
static const char *status_name(int32_t s) {
  switch (s) { case 1: return "OK"; case 2: return "ERROR"; case 3: return "DENIED";
               default: return "UNSPEC"; }
}

/* ============================================================================
 *  §11  MINIMAL PROTOBUF  — encoder/decoder for exactly the fields we use.
 *
 *  Why hand-rolled: the whole message set is a dozen fields and the protocol
 *  REQUIRES rejecting unknown/reserved field numbers (§5.3, §11.1), which a
 *  general-purpose library happily ignores instead. Your port can use nanopb
 *  as long as it keeps the unknown-field rejection.
 * ==========================================================================*/

struct PbOut {
  uint8_t *buf; size_t cap; size_t len; bool ok;
  PbOut(uint8_t *b, size_t c) : buf(b), cap(c), len(0), ok(true) {}
  void raw(uint8_t b)                    { if (len < cap) buf[len++] = b; else ok = false; }
  void varint(uint64_t v)                { do { uint8_t b = v & 0x7f; v >>= 7; raw(b | (v ? 0x80 : 0)); } while (v); }
  void tag(uint32_t f, uint8_t wt)       { varint(((uint64_t)f << 3) | wt); }
  void u32(uint32_t f, uint32_t v)       { if (!v) return; tag(f, 0); varint(v); }
  void boolean(uint32_t f, bool v)       { if (!v) return; tag(f, 0); varint(1); }
  void bytes(uint32_t f, const uint8_t *p, size_t n) {
    if (!p) return;
    tag(f, 2); varint(n);
    if (len + n > cap) { ok = false; return; }
    memcpy(buf + len, p, n); len += n;
  }
  void str(uint32_t f, const char *s)    { if (s && *s) bytes(f, (const uint8_t *)s, strlen(s)); }
};

/* Field-by-field reader. Unknown field numbers are the caller's business —
 * every decoder below flags them and the connection dies (§5.3). */
struct PbIn {
  const uint8_t *p, *end;
  bool bad = false;              /* malformed wire bytes                */
  bool unknown = false;          /* a field number we do not recognise  */
  uint32_t unknown_field = 0;
  uint32_t seen = 0;             /* bitmask of singular fields already read */
  uint32_t field = 0; uint8_t wt = 0;
  uint64_t vint = 0;             /* wt 0                                */
  const uint8_t *data = nullptr; size_t dlen = 0;   /* wt 2             */

  PbIn(const uint8_t *b, size_t n) : p(b), end(b + n) {}

  bool readVarint(uint64_t &out) {
    out = 0; int shift = 0;
    while (p < end) {
      uint8_t b = *p++;
      out |= (uint64_t)(b & 0x7f) << shift;
      if (!(b & 0x80)) return true;
      shift += 7;
      if (shift > 63) break;
    }
    bad = true; return false;
  }
  bool next() {
    if (bad || p >= end) return false;
    uint64_t key;
    if (!readVarint(key)) return false;
    field = (uint32_t)(key >> 3); wt = (uint8_t)(key & 7);
    switch (wt) {
      case 0: return readVarint(vint);
      case 2: {
        uint64_t n;
        if (!readVarint(n) || (size_t)(end - p) < n) { bad = true; return false; }
        data = p; dlen = (size_t)n; p += n; return true;
      }
      case 5: if (end - p < 4) { bad = true; return false; } data = p; dlen = 4; p += 4; return true;
      case 1: if (end - p < 8) { bad = true; return false; } data = p; dlen = 8; p += 8; return true;
      default: bad = true; return false;
    }
  }
  void mark_unknown() { unknown = true; if (!unknown_field) unknown_field = field; }

  /* §5.2 — a singular field may appear at most once. "Last value wins" is
   * explicitly forbidden: it lets a peer hide a second owner_key or token
   * behind a first one that passed the length checks. Duplicates are marked
   * malformed, so every existing `!in.bad` check already rejects them and the
   * connection dies with no response (§19.2). Repeated fields (BulkPayload
   * item 1) simply do not call this. */
  void once() {
    if (field < 32) {
      uint32_t b = 1u << field;
      if (seen & b) bad = true;
      seen |= b;
    }
  }
};

static void be32(uint8_t *o, uint32_t v) { o[0]=v>>24; o[1]=v>>16; o[2]=v>>8; o[3]=v; }

/* ── CommandPacket (§11.1): 2=type, 3=payload; 1/4/5 RESERVED (v2.5.1) ───── */
struct CommandPacket {
  int32_t type = 0;
  const uint8_t *payload = nullptr; size_t payload_len = 0;
  bool has_payload = false, has_unknown = false, ok = false;
  uint32_t unknown_field = 0;   /* 1/4/5 identifies a v2.5.1 central */
};
static CommandPacket CommandPacket_decode(const uint8_t *b, size_t n) {
  CommandPacket c; PbIn in(b, n);
  while (in.next()) {
    switch (in.field) {
      case 2: if (in.wt != 0) { in.bad = true; break; } in.once(); c.type = (int32_t)in.vint; break;
      case 3: if (in.wt != 2) { in.bad = true; break; } in.once();
              c.payload = in.data; c.payload_len = in.dlen; c.has_payload = true; break;
      default: in.mark_unknown(); break;   /* includes reserved 1, 4, 5 */
    }
  }
  c.has_unknown = in.unknown; c.unknown_field = in.unknown_field; c.ok = !in.bad;
  return c;
}

/* ── EventPacket (§11.1): 2=type, 3=status, 4=payload ────────────────────── */
static size_t EventPacket_encode(uint8_t *buf, size_t cap, int32_t type,
                                 int32_t status, const uint8_t *payload, size_t plen) {
  PbOut o(buf, cap);
  o.u32(2, (uint32_t)type);
  o.u32(3, (uint32_t)status);
  if (payload && plen) o.bytes(4, payload, plen);
  return o.ok ? o.len : 0;
}

/* ── HelloPayload (§11.2) ────────────────────────────────────────────────── */
struct HelloPayload {
  const uint8_t *pub = nullptr; size_t pub_len = 0;
  uint32_t protocol_version = 0;
  const uint8_t *challenge = nullptr; size_t challenge_len = 0;
  bool has_unknown = false, ok = false;
};
static HelloPayload HelloPayload_decode(const uint8_t *b, size_t n) {
  HelloPayload h; PbIn in(b, n);
  while (in.next()) {
    switch (in.field) {
      case 1: if (in.wt != 2) { in.bad = true; break; } in.once(); h.pub = in.data; h.pub_len = in.dlen; break;
      case 2: if (in.wt != 0) { in.bad = true; break; } in.once(); h.protocol_version = (uint32_t)in.vint; break;
      case 3: if (in.wt != 2) { in.bad = true; break; } in.once(); h.challenge = in.data; h.challenge_len = in.dlen; break;
      default: in.mark_unknown(); break;
    }
  }
  h.has_unknown = in.unknown; h.ok = !in.bad;
  return h;
}

/* ── HelloAckPayload (§11.2) ─────────────────────────────────────────────── */
static size_t HelloAckPayload_encode(uint8_t *buf, size_t cap,
                                     const uint8_t pub[32], const uint8_t sid[4],
                                     bool bound, uint32_t binding_version,
                                     const uint8_t chal[16], const uint8_t *proof,
                                     bool physical_claim_authorized) {
  PbOut o(buf, cap);
  o.bytes(1, pub, X25519_KEY_LEN);
  o.bytes(2, sid, SESSION_ID_LEN);
  o.u32(3, PROTOCOL_VERSION);
  o.boolean(4, bound);
  o.u32(5, binding_version);
  o.bytes(6, chal, CHALLENGE_LEN);
  if (proof) o.bytes(7, proof, DEVICE_PROOF_LEN);
  o.boolean(8, physical_claim_authorized);
  return o.ok ? o.len : 0;
}

/* ── BindPayload / ApplyOwnershipTokenPayload (§11.8) ────────────────────── */
struct BindPayload {
  const uint8_t *owner_key = nullptr; size_t owner_key_len = 0;
  const uint8_t *token = nullptr;     size_t token_len = 0;
  bool has_unknown = false, ok = false;
};
static BindPayload BindPayload_decode(const uint8_t *b, size_t n) {
  BindPayload r; PbIn in(b, n);
  while (in.next()) {
    switch (in.field) {
      case 1: if (in.wt != 2) { in.bad = true; break; } in.once(); r.owner_key = in.data; r.owner_key_len = in.dlen; break;
      case 2: if (in.wt != 2) { in.bad = true; break; } in.once(); r.token = in.data; r.token_len = in.dlen; break;
      default: in.mark_unknown(); break;
    }
  }
  r.has_unknown = in.unknown; r.ok = !in.bad;
  return r;
}
struct UnbindPayload {
  const uint8_t *token = nullptr; size_t token_len = 0;
  bool has_unknown = false, ok = false;
};
static UnbindPayload UnbindPayload_decode(const uint8_t *b, size_t n) {
  UnbindPayload r; PbIn in(b, n);
  while (in.next()) {
    if (in.field == 1 && in.wt == 2) { in.once(); r.token = in.data; r.token_len = in.dlen; }
    else in.mark_unknown();
  }
  r.has_unknown = in.unknown; r.ok = !in.bad;
  return r;
}

/* ── OwnershipApplyAckPayload (§18) ──────────────────────────────────────── */
static size_t OwnershipApplyAck_encode(uint8_t *buf, size_t cap,
                                       OwnershipAction action, uint32_t version,
                                       bool resulting_bound, const uint8_t proof[16],
                                       const uint8_t *next_sid /* 4 or nullptr */) {
  PbOut o(buf, cap);
  o.u32(1, (uint32_t)action);
  o.u32(2, version);
  o.boolean(3, resulting_bound);
  o.bytes(4, proof, DEVICE_PROOF_LEN);
  if (next_sid) o.bytes(5, next_sid, SESSION_ID_LEN);
  return o.ok ? o.len : 0;
}

/* ── DeviceCapPayload (§11.3) ────────────────────────────────────────────── */
static size_t DeviceCapPayload_encode(uint8_t *buf, size_t cap) {
  PbOut o(buf, cap);
  o.u32(1, CAP_FIRMWARE_VERSION);
  o.str(2, CAP_HARDWARE_REVISION);
  o.u32(3, INFO_GROUP_WYN_SIGN);        /* repeated, unpacked: one tag each */
  o.u32(3, INFO_GROUP_ANDROID_AUTO);
  o.u32(3, INFO_GROUP_MAP_NAVIGATION);
  o.u32(4, BULK_MAX_ITEMS);
  o.u32(5, CAP_DISPLAY_W);
  o.u32(6, CAP_DISPLAY_H);
  return o.ok ? o.len : 0;
}

/* ============================================================================
 *  APPLICATION HOOKS — PORT ME. Everything above is protocol; this is product.
 * ==========================================================================*/

/* Names for the values that actually arrive in traffic. The full enums are in
 * the spec (§11.5/§11.6/§11.7) — extend as your product needs them. */
static const char *sign_name(int32_t t) {
  switch (t) {
    case 1:  return "CURRENT_SPEED";     case 2:  return "SPEED_LIMIT_SIGN";
    case 10: return "CAMERA";            case 11: return "SPEED_CAM";
    case 12: return "TRAFFIC_CAM";       case 13: return "AI_CAM";
    case 20: return "RESIDENT_AREA_START"; case 21: return "RESIDENT_AREA_END";
    case 22: return "TOLL_BOOTH";        case 23: return "TUNNEL";
    case 24: return "REST_AREA";         case 32: return "TRAFFIC_LIGHT";
    case 51: return "NO_PARKING";        case 60: return "LANE_INFO";
    default: return "SIGN";
  }
}
static const char *vprop_name(uint32_t id) {
  switch (id) {
    case 291504905u: return "EV_BATTERY_LEVEL(Wh)";
    case 289410881u: return "EV_CHARGE_STATE";
    case 287310603u: return "EV_CHARGE_PORT_CONNECTED";
    case 289410883u: return "EV_CHARGE_TIME_REMAINING(s)";
    case 291504903u: return "FUEL_LEVEL(mL)";
    case 287310853u: return "FUEL_LEVEL_LOW";
    case 291504904u: return "RANGE_REMAINING(m)";
    case 289408016u: return "TURN_SIGNAL_LIGHT_STATE";
    case 392168201u: return "TIRE_PRESSURE(kPa)";
    default:         return "VEHICLE_PROPERTY";
  }
}
static const char *maneuver_name(int32_t m) {
  switch (m) {
    case 1:  return "DEPART";        case 2:  return "DESTINATION";
    case 5:  return "STRAIGHT";      case 6:  return "TURN_LEFT";
    case 7:  return "TURN_RIGHT";    case 8:  return "KEEP_LEFT";
    case 9:  return "KEEP_RIGHT";    case 17: return "MERGE_LEFT";
    case 18: return "MERGE_RIGHT";   case 61: return "ROUNDABOUT_EXIT_CW";
    case 62: return "ROUNDABOUT_EXIT_CCW"; case 65: return "NAME_CHANGE";
    default: return "MANEUVER";
  }
}

volatile uint32_t g_speed_limit_kmh   = 0;
volatile bool     g_speed_limit_valid = false;

/* Camera-warning LED: nếu SM16206_Camera() được gọi 1 lần rồi thôi (one-shot),
 * icon sẽ bị chính LED_Control() xoá mất ngay ở chu kỳ 10ms kế tiếp, vì
 * LED_Control() memset() toàn bộ framebuffer mỗi lần chạy và không tự vẽ lại
 * icon camera. Cách giữ sáng đúng: lưu lại một "hạn chót" (deadline), rồi để
 * vòng lặp chính (main.cpp) vẽ lại icon MỖI chu kỳ dựa trên hạn chót đó — y
 * hệt cách g_speed_limit_valid/SM16206_Speed_Limit đã làm ở trên. */
   /* muốn giữ sáng bao lâu thì chỉnh số này */

volatile bool g_camera_active = false;   // true = có cảnh báo camera, LED sáng

bool camera_led_should_be_on(void) {
  return g_camera_active;
}

/* main.cpp gọi hàm này mỗi vòng lặp để biết icon camera có nên sáng không —
 * xuất ra dạng hàm thay vì để main.cpp tự so millis(), vì millis() ở đây là
 * static inline (internal linkage), main.cpp không nhìn thấy được. */


volatile bool g_no_overtaking_active = false;

bool no_overtaking_led_should_be_on(void) {
  return g_no_overtaking_active;
}

static void on_wyn_sign(int32_t sign_type, bool has_speed, uint32_t speed,
                        bool has_dist, uint32_t distance_m) {
  LOGRAW(" SIGN  %s(%ld)", sign_name(sign_type), (long)sign_type);
  if (has_speed) printf("  speed=%lu km/h", (unsigned long)speed);
  if (has_dist)  printf("  in %lu m", (unsigned long)distance_m);
  printf("\n");
  switch(sign_type) {
    case 2: if (has_speed) {
       g_speed_limit_kmh = speed;
        g_speed_limit_valid = true;
      
    }
    break;
     case 10: case 11 : case 12 : case 13: {
   
      if ((HUD_GetDisplayMask() & HUD_REGION_CAMERA) != 0) {
  bool cam_on = (distance_m > 0);
  if (cam_on) {
    if (!g_camera_active) {          // chỉ kêu còi khi chuyển tắt→bật
      Buzzer_On(); vTaskDelay(pdMS_TO_TICKS(100)); Buzzer_Off();
    }
    g_camera_active = true;
  } 
  else 
  {
    g_camera_active = false;         // distance_m == 0 → tắt ngay
  }
} 
  else 
{
  SM16206_Camera(0);
  g_camera_active = false;           // user tắt vùng camera trong config
}
      break;}
   
    case 30:  // NO_OVERTAKING_START (không có tên trong bảng sign_name() — vẫn là số hợp lệ)
      if ((HUD_GetDisplayMask() & HUD_REGION_NO_OVERTAKING) != 0) {
        if(distance_m > 0) {
          if(!g_no_overtaking_active) {
            Buzzer_On();
            vTaskDelay(pdMS_TO_TICKS(100));
            Buzzer_Off();
          g_no_overtaking_active = true;
          }
        }
        else if(distance_m == 0){
          g_no_overtaking_active = false;
        }
      }
      else{
        g_no_overtaking_active = false;
        SM16206_NoPassing(0);
      }
      break;
    case 31:  // NO_OVERTAKING_END
      g_no_overtaking_active = false;
      SM16206_NoPassing(0);   // tắt ngay, không cần đợi tới vòng lặp kế tiếp
      break;

    default:
      break;
    case 32:  // TRAFFIC_LIGHT — đèn cấm vượt cũng phải tắt khi xe đã tới gần
          // đèn giao thông (< 10m), coi như hết hiệu lực đoạn cấm vượt,
          // logic giống hệt case 31 NO_OVERTAKING_END ở trên.
  if (has_dist && distance_m == 0) {
    g_no_overtaking_active = false;
    SM16206_NoPassing(0);   // tắt ngay, không cần đợi tới vòng lặp kế tiếp
  }
  break;
  }
}
static void on_android_auto(uint32_t property_id, uint32_t area_id,
                            bool has_f, float f, bool has_i, int32_t i, bool has_b, bool b) {
  LOGRAW(" CAR   %s(%lu) area=0x%lx", vprop_name(property_id),
         (unsigned long)property_id, (unsigned long)area_id);
  if (has_f) printf("  = %.2f", f);
  if (has_i) printf("  = %ld", (long)i);
  if (has_b) printf("  = %s", b ? "true" : "false");
  printf("\n");
}
static void on_map_step(int32_t maneuver, bool has_exit, uint32_t exit_no,
                        bool has_dist, uint32_t dist_m, const char *road) {
  LOGRAW(" NAV   %s(%ld)", maneuver_name(maneuver), (long)maneuver);
  if (has_dist) printf("  in %lu m", (unsigned long)dist_m);
  if (has_exit) printf("  exit #%lu", (unsigned long)exit_no);
  if (road && *road) printf("  on \"%s\"", road);
  printf("\n");
}

/* ── UpdateInfoPayload (§11.4): 1=group, oneof 2=wyn 3=android_auto 4=map ── */
static bool UpdateInfo_apply(const uint8_t *b, size_t n) {
  PbIn in(b, n);
  int32_t group = 0;
  const uint8_t *sub = nullptr; size_t sub_len = 0; uint32_t sub_field = 0;

  while (in.next()) {
    if (in.field == 1 && in.wt == 0) { in.once(); group = (int32_t)in.vint; continue; }
    if ((in.field >= 2 && in.field <= 4) && in.wt == 2) {
      if (sub) { in.bad = true; break; }   /* oneof: at most one branch */
      in.once(); sub = in.data; sub_len = in.dlen; sub_field = in.field; continue;
    }
    in.mark_unknown();
  }
  if (in.bad || in.unknown || !sub) return false;
  /* The oneof branch must match the declared group — a mismatch is a
   * malformed message, not something to guess about. */
  if ((uint32_t)group + 1 != sub_field) return false;

  PbIn s(sub, sub_len);
  if (group == INFO_GROUP_WYN_SIGN) {
    int32_t sign = 0; uint32_t speed = 0, dist = 0; bool hs = false, hd = false;
    while (s.next()) {
      switch (s.field) {
        case 1: sign = (int32_t)s.vint; break;
        case 2: speed = (uint32_t)s.vint; hs = true; break;
        case 3: dist = (uint32_t)s.vint; hd = true; break;
        default: s.mark_unknown(); break;
      }
    }
    if (s.bad || s.unknown) return false;
    on_wyn_sign(sign, hs, speed, hd, dist);
    return true;
  }
  if (group == INFO_GROUP_ANDROID_AUTO) {
    uint32_t prop = 0, area = 0; float f = 0; int32_t i = 0; bool bv = false;
    bool hf = false, hi = false, hb = false;
    while (s.next()) {
      switch (s.field) {
        case 1: prop = (uint32_t)s.vint; break;
        case 2: area = (uint32_t)s.vint; break;
        case 3: break;                       /* timestamp_ms — unused here  */
        case 4: if (s.dlen == 4) { memcpy(&f, s.data, 4); hf = true; } break;  /* fixed32 LE */
        case 5: i = (int32_t)(uint32_t)s.vint; hi = true; break;
        case 6: bv = s.vint != 0; hb = true; break;
        case 7: break;                       /* Int32List — extend if used  */
        default: s.mark_unknown(); break;
      }
    }
    if (s.bad || s.unknown) return false;
    on_android_auto(prop, area, hf, f, hi, i, hb, bv);
    return true;
  }
  if (group == INFO_GROUP_MAP_NAVIGATION) {
    int32_t man = 0; uint32_t exit_no = 0, dist = 0; bool he = false, hd = false;
    char road[64] = {0};
    while (s.next()) {
      switch (s.field) {
        case 1: man = (int32_t)s.vint; break;
        case 2: exit_no = (uint32_t)s.vint; he = true; break;
        case 3: dist = (uint32_t)s.vint; hd = true; break;
        case 4: {
          size_t k = s.dlen < sizeof(road) - 1 ? s.dlen : sizeof(road) - 1;
          memcpy(road, s.data, k); road[k] = 0; break;
        }
        default: s.mark_unknown(); break;
      }
    }
    if (s.bad || s.unknown) return false;
    on_map_step(man, he, exit_no, hd, dist, road);
    return true;
  }
  return false;
}

/* ============================================================================
 *  CRYPTO PRIMITIVES
 * ==========================================================================*/

static bool sha256_of(const uint8_t *in, size_t len, uint8_t out[32]) {
  return mbedtls_sha256(in, len, out, 0) == 0;
}

/* HMAC-SHA256 built directly on mbedtls_sha256 — the mbedtls_md HMAC path is
 * routed through PSA on newer cores and rejects raw-key use in some configs. */
static bool hmac_sha256(const uint8_t *key, size_t keylen,
                        const uint8_t *msg, size_t msglen, uint8_t out[32]) {
  uint8_t k[64] = {0}, ipad[64], opad[64], inner[32];
  bool ok = false;
  if (keylen > 64) { if (!sha256_of(key, keylen, k)) return false; }
  else memcpy(k, key, keylen);
  for (int i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }

  mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx);
  if (mbedtls_sha256_starts(&ctx, 0) || mbedtls_sha256_update(&ctx, ipad, 64) ||
      mbedtls_sha256_update(&ctx, msg, msglen) || mbedtls_sha256_finish(&ctx, inner)) goto done;
  if (mbedtls_sha256_starts(&ctx, 0) || mbedtls_sha256_update(&ctx, opad, 64) ||
      mbedtls_sha256_update(&ctx, inner, 32) || mbedtls_sha256_finish(&ctx, out)) goto done;
  ok = true;
done:
  mbedtls_sha256_free(&ctx);
  mbedtls_platform_zeroize(k, sizeof k); mbedtls_platform_zeroize(ipad, sizeof ipad);
  mbedtls_platform_zeroize(opad, sizeof opad); mbedtls_platform_zeroize(inner, sizeof inner);
  return ok;
}

/* §4 — HMAC under device_claim_key. On production silicon this is the ONLY
 * way to use the key: replace the body with esp_hmac_calculate() over the
 * eFuse block so the key itself is never in RAM. */
static bool factory_hmac(const uint8_t *msg, size_t len, uint8_t out[32]) {
  return hmac_sha256(DEVICE_CLAIM_KEY, sizeof DEVICE_CLAIM_KEY, msg, len, out);
}

/* Constant-time compare — these run over attacker-supplied bytes. */
static bool ct_equal(const uint8_t *a, const uint8_t *b, size_t n) {
  uint8_t d = 0; for (size_t i = 0; i < n; i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}

/* §10.1 — RNG health. esp_fill_random() draws from the hardware TRNG and has
 * no failure return, so the only thing left to catch is a stuck or dead source.
 * Two independent draws that come back identical, or a draw that is all-zero,
 * means the entropy path is not delivering; §10.1 says fail closed and refuse
 * the HELLO rather than derive a session from predictable bytes.
 * ponytail: stuck/zero check only, not a full NIST SP 800-90B suite — that
 * belongs in the vendor CSPRNG, not in a peripheral module. */
static bool g_rng_healthy = true;

static bool rng_health_ok(void) {
  uint8_t a[32], b[32];
  esp_fill_random(a, sizeof a);
  esp_fill_random(b, sizeof b);
  uint8_t acc = 0;
  for (size_t i = 0; i < sizeof a; i++) acc |= a[i];
  bool ok = acc != 0 && memcmp(a, b, sizeof a) != 0;
  mbedtls_platform_zeroize(a, sizeof a);
  mbedtls_platform_zeroize(b, sizeof b);
  if (!ok) g_rng_healthy = false;
  return ok;
}

static int rng_cb(void *, unsigned char *out, size_t len) {
  if (!g_rng_healthy) return -1;
  esp_fill_random(out, len); return 0;
}
static void rand_fill(uint8_t *out, size_t len) { esp_fill_random(out, len); }

/* §20 — bounded exponential backoff over consecutive unauthenticated HELLOs
 * (empty challenge → PRE_OWNER_NOAUTH) and failed ownership attempts. A local
 * delay only: §20 forbids the rate limiter from touching ownership state. */
#define RATE_LIMIT_BASE_MS   250
#define RATE_LIMIT_MAX_MS    8000
static uint8_t g_unauth_streak = 0;

static void rate_limit_penalty(void) {
  if (!g_unauth_streak) return;
  uint32_t d = RATE_LIMIT_BASE_MS << (g_unauth_streak - 1);
  if (d > RATE_LIMIT_MAX_MS) d = RATE_LIMIT_MAX_MS;
  LOGD("§20 rate limit: streak %u → delaying %lu ms", g_unauth_streak, (unsigned long)d);
  delay(d);
}
static void rate_limit_note_unauth(void) { if (g_unauth_streak < 16) g_unauth_streak++; }
static void rate_limit_reset(void)       { g_unauth_streak = 0; }

/* ============================================================================
 *  §3/§14  OWNERSHIP RECORD  (owner_key + binding_version, persistent)
 * ==========================================================================*/

/* §6.2 — the ownership record is ONE blob, never a set of independent keys.
 * The old two-key layout (owner_key here, binding_version there) could not
 * satisfy §6.3 rule 3: power loss between the two writes left a new key with
 * an old version. One blob + CRC + A/B slots makes that state unrepresentable
 * — a torn write fails its CRC and the previous slot is still intact. */
#define OWN_FORMAT_V1       0x00000001u
#define OWN_GENERATION_MAX  0xFFFFFFFFu

struct __attribute__((packed)) own_rec_t {
  uint32_t format_version;
  uint32_t generation;
  uint32_t binding_version;
  uint8_t  owner_key[OWNER_KEY_LEN];
  uint8_t  owner_key_present;      /* canonical 0x00 / 0x01 */
  uint8_t  reserved[3];            /* must be zero          */
  uint32_t integrity;              /* CRC32 over everything above */
};
static_assert(sizeof(own_rec_t) == 52, "ownership record layout drifted");

/* CRC32 (IEEE, reflected). Record integrity only — not a security primitive,
 * the record's confidentiality is storage's job (§14). Inline rather than
 * esp_rom_crc32_le() so this stays independent of exactly which ROM/IDF
 * header that lives in across IDF releases. */
static uint32_t crc32_of(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(c & 1)));
  }
  return ~c;
}

static Preferences g_nvs;
static bool     g_bound = false;
static uint32_t g_binding_version = 0;
static uint32_t g_generation = 0;
static int      g_slot = 0;              /* active A/B slot                 */
static bool     g_storage_fault = false; /* §7.2 — ownership state UNKNOWN  */

static const char *slot_key(int i) { return i ? "rec1" : "rec0"; }

/* §6.2 invariants. A record that fails any of these is not a record. */
static bool own_rec_valid(const own_rec_t &r) {
  if (r.format_version != OWN_FORMAT_V1) return false;
  if (r.integrity != crc32_of((const uint8_t *)&r, offsetof(own_rec_t, integrity))) return false;
  if (r.reserved[0] | r.reserved[1] | r.reserved[2]) return false;
  if (r.owner_key_present > 1) return false;
  if (!r.owner_key_present) {                 /* unbound record carries zero key */
    uint8_t acc = 0;
    for (int i = 0; i < OWNER_KEY_LEN; i++) acc |= r.owner_key[i];
    if (acc) return false;
  }
  return true;
}

static bool own_slot_read(int i, own_rec_t &r) {
  return g_nvs.getBytes(slot_key(i), &r, sizeof r) == sizeof r;
}
static bool own_slot_write(int i, const own_rec_t &r) {
  if (g_nvs.putBytes(slot_key(i), &r, sizeof r) != sizeof r) return false;
  own_rec_t back;                              /* §6.4 step 5/7 — read back  */
  if (!own_slot_read(i, back)) return false;
  return memcmp(&back, &r, sizeof r) == 0;
}

/* §6.6 — publish the METADATA only, never keep the owner key resident. The
 * record lives in storage; the key is read back for the few microseconds a
 * key derivation needs it and wiped again. */
static void own_publish(int slot, const own_rec_t &r) {
  g_slot = slot;
  g_bound = r.owner_key_present == 0x01;
  g_binding_version = r.binding_version;
  g_generation = r.generation;
}

/* §7.1 steps 4-7 + §6.4 slot election. Never guesses: an unreadable record is
 * a fault, not "unbound at version 0" (§7.1). */
static bool identity_load() {
  Preferences p;
  if (!p.begin("wyn", true)) {              // đọc-only, dùng chung namespace "wyn"
    printf("identity_load: NVS 'wyn' namespace not found\n");
    return false;
  }
  size_t n1 = p.getBytes("uid", DEVICE_UID, sizeof DEVICE_UID);
  size_t n2 = p.getBytes("claim_key", DEVICE_CLAIM_KEY, sizeof DEVICE_CLAIM_KEY);
  if (n1 != sizeof DEVICE_UID || n2 != sizeof DEVICE_CLAIM_KEY) {
    printf("identity_load: missing/invalid uid or claim_key — device not provisioned\n");
    return false;
  }
  return true;
}

static void ownership_load() {
  g_nvs.begin("wyn", false);

  own_rec_t r[2];
  bool present[2], valid[2];
  for (int i = 0; i < 2; i++) {
    present[i] = g_nvs.isKey(slot_key(i));
    valid[i]   = present[i] && own_slot_read(i, r[i]) && own_rec_valid(r[i]);
  }

  if (valid[0] && valid[1]) {
    if (r[0].generation != r[1].generation) {
      /* §6.4 — plain integer compare; generation never wraps, so serial-number
       * arithmetic would be wrong here. */
      int win = r[0].generation > r[1].generation ? 0 : 1;
      own_publish(win, r[win]);
    } else if (memcmp(&r[0], &r[1], sizeof(own_rec_t)) == 0) {
      own_publish(0, r[0]);
    } else {
      g_storage_fault = true;
      LOGE("ownership: two valid slots, same generation, different content");
    }
  } else if (valid[0] || valid[1]) {
    int win = valid[0] ? 0 : 1;
    own_publish(win, r[win]);
  } else if (!present[0] && !present[1]) {
    /* Factory-fresh silicon. On a production line the canonical unbound record
     * is written by provisioning alongside the claim key; this module has no
     * provisioning step, so it writes one once. A slot that EXISTS but does
     * not validate is corruption and must NOT land here — that is the fault
     * path below, per §7.1 step 6. */
    own_rec_t init; memset(&init, 0, sizeof init);
    init.format_version = OWN_FORMAT_V1;
    init.integrity = crc32_of((const uint8_t *)&init, offsetof(own_rec_t, integrity));
    if (own_slot_write(0, init)) {
      own_publish(0, init);
      LOGI("ownership: blank device — wrote canonical unbound record");
    } else {
      g_storage_fault = true;
      LOGE("ownership: could not write the initial record");
    }
  } else {
    g_storage_fault = true;
    LOGE("ownership: a slot exists but no valid record — storage corrupted");
  }

  if (g_storage_fault) {
    /* §7.2 — UNKNOWN, not unbound. No HELLO_ACK may claim an ownership state. */
    LOGE("OWNERSHIP_STORAGE_FAULT — refusing every HELLO until serviced");
    return;
  }
  LOGI("ownership: bound=%d version=%lu generation=%lu slot=%c",
       (int)g_bound, (unsigned long)g_binding_version,
       (unsigned long)g_generation, g_slot ? 'B' : 'A');
}

static bool ownership_get_owner_key(uint8_t out[OWNER_KEY_LEN]) {
  if (!g_bound || g_storage_fault) return false;
  own_rec_t r;
  bool ok = own_slot_read(g_slot, r) && own_rec_valid(r) && r.owner_key_present == 0x01;
  if (ok) memcpy(out, r.owner_key, OWNER_KEY_LEN);
  mbedtls_platform_zeroize(&r, sizeof r);
  return ok;
}

/* §6.4 — one transaction for BIND and UNBIND. key == nullptr means unbind.
 * Writes the INACTIVE slot, verifies it, and only then publishes: a power cut
 * anywhere before the final write leaves the active slot untouched. */
static bool ownership_commit(const uint8_t *key, uint32_t new_version) {
  if (g_storage_fault) return false;
  if (new_version <= g_binding_version) return false;      /* §15.3 monotonic */
  if (g_generation >= OWN_GENERATION_MAX) {                /* §6.4 / §18.5    */
    LOGE("ownership: generation exhausted — refusing to mutate (service required)");
    return false;
  }

  own_rec_t r; memset(&r, 0, sizeof r);
  r.format_version    = OWN_FORMAT_V1;
  r.generation        = g_generation + 1;
  r.binding_version   = new_version;
  r.owner_key_present = key ? 0x01 : 0x00;
  if (key) memcpy(r.owner_key, key, OWNER_KEY_LEN);
  r.integrity = crc32_of((const uint8_t *)&r, offsetof(own_rec_t, integrity));

  int target = g_slot ^ 1;
  if (!own_slot_write(target, r)) {
    LOGE("ownership: commit to slot %c failed read-back", target ? 'B' : 'A');
    mbedtls_platform_zeroize(&r, sizeof r);
    return false;
  }
  own_publish(target, r);

  /* §6.5 — after an UNBIND the other slot may still hold a usable owner_key.
   * Overwrite it with this same canonical unbound record: the key is gone and
   * the A/B redundancy survives. */
  if (!key && !own_slot_write(g_slot ^ 1, r))
    LOGE("ownership: stale slot scrub failed — old owner key may persist");

  mbedtls_platform_zeroize(&r, sizeof r);
  return true;
}

/* ============================================================================
 *  §4  PHYSICAL CLAIM WINDOW
 *
 *  An unbound device only answers a HELLO while the user is physically
 *  present. The authorization is latched to ONE transcript, so only the
 *  connection that opened it can spend it, and it is consumed on BIND.
 *
 * ==========================================================================*/

static uint32_t g_claim_deadline_ms = 0;
static bool     g_claim_armed = false;   /* §8.2 — one press grants one HELLO */
static bool     g_claim_latched = false, g_claim_consumed = false;
static uint8_t  g_claim_transcript[TRANSCRIPT_LEN];

static bool claim_window_open() {
  return g_claim_deadline_ms && (int32_t)(millis() - g_claim_deadline_ms) < 0;
}
static bool claim_armed_now() { return g_claim_armed && claim_window_open(); }
/* §8.1 — on a BOUND device the button opens a maintenance advertising window
 * but MUST NOT arm a physical claim. This module advertises continuously, so
 * the whole difference is the arm; saying so in the log keeps the two cases
 * from looking identical. */
static void claim_open_window() {
  g_claim_deadline_ms = millis() + CLAIM_WINDOW_MS;
  g_claim_latched = g_claim_consumed = false;
  g_claim_armed = !g_bound;              /* §8.1 — bound: maintenance, no arm */
  LOGI("%s window open (%d s)", g_bound ? "maintenance (bound — claim NOT armed)"
                                        : "claim", CLAIM_WINDOW_MS / 1000);
}
static void claim_latch(const uint8_t th[TRANSCRIPT_LEN]) {
  memcpy(g_claim_transcript, th, TRANSCRIPT_LEN);
  g_claim_latched = true; g_claim_consumed = false;
  g_claim_armed = false;                 /* §8.4 — first eligible HELLO takes it */
}
static bool claim_matches(const uint8_t th[TRANSCRIPT_LEN]) {
  return g_claim_latched && !g_claim_consumed && claim_window_open() &&
         ct_equal(g_claim_transcript, th, TRANSCRIPT_LEN);
}
static void claim_clear() {
  g_claim_latched = g_claim_consumed = false;
  mbedtls_platform_zeroize(g_claim_transcript, sizeof g_claim_transcript);
}

/* PORT ME — any "user is physically here" gesture works. */
static void claim_button_poll() {
  static uint32_t down_at = 0;
  bool down = digitalRead(CLAIM_BUTTON_PIN) == LOW;
  if (down && !down_at) { down_at = millis(); LOGI("boot key pressed — hold for %d s to open claim window", CLAIM_HOLD_MS / 1000); }
  if (!down) down_at = 0;
  else if (millis() - down_at >= CLAIM_HOLD_MS) { down_at = 0; claim_open_window(); }
}

/* ============================================================================
 *  §5 / §8  SESSION CRYPTO
 * ==========================================================================*/

enum Epoch : uint8_t { EPOCH_NONE = 0, EPOCH_PRE_OWN, EPOCH_OWNER };

static struct {
  Epoch   epoch = EPOCH_NONE;
  uint8_t transcript[TRANSCRIPT_LEN];
  bool    has_transcript = false;
  uint8_t pre_own_key[32];
  uint8_t hs_session_id[SESSION_ID_LEN];  /* from the handshake            */
  uint8_t session_id[SESSION_ID_LEN];     /* the ACTIVE epoch's nonce prefix */
  uint64_t tx = 0, rx = 0;
  mbedtls_gcm_context gcm;
  bool gcm_ready = false;
} S;

/* §14 — this IS "invalidate the connection". Wipes everything derived. */
static void session_reset() {
  if (S.gcm_ready) { mbedtls_gcm_free(&S.gcm); S.gcm_ready = false; }
  S.epoch = EPOCH_NONE; S.tx = S.rx = 0; S.has_transcript = false;
  mbedtls_platform_zeroize(S.transcript, sizeof S.transcript);
  mbedtls_platform_zeroize(S.pre_own_key, sizeof S.pre_own_key);
  mbedtls_platform_zeroize(S.session_id, sizeof S.session_id);
  mbedtls_platform_zeroize(S.hs_session_id, sizeof S.hs_session_id);
}
static bool session_is_secure() { return S.epoch != EPOCH_NONE && S.gcm_ready; }

static bool activate_epoch(Epoch e, const uint8_t key[32], const uint8_t sid[4]) {
  if (S.gcm_ready) { mbedtls_gcm_free(&S.gcm); S.gcm_ready = false; }
  mbedtls_gcm_init(&S.gcm);
  if (mbedtls_gcm_setkey(&S.gcm, MBEDTLS_CIPHER_ID_AES, key, 256) != 0) {
    mbedtls_gcm_free(&S.gcm); return false;
  }
  S.gcm_ready = true; S.epoch = e;
  memcpy(S.session_id, sid, SESSION_ID_LEN);
  S.tx = S.rx = 0;                       /* §8 — counters restart per epoch */
  LOGI("epoch → %s  sid=%02x%02x%02x%02x  counters reset",
       e == EPOCH_PRE_OWN ? "PRE_OWN" : "OWNER", sid[0], sid[1], sid[2], sid[3]);
  return true;
}

/* §5.3 steps 1-11 plus the §5.4 transcript. On return the epoch is still
 * NONE: the caller sends HELLO_ACK first, then picks the epoch. */
struct HelloOut {
  uint8_t peripheral_pub[X25519_KEY_LEN];
  uint8_t session_id[SESSION_ID_LEN];
  uint8_t peripheral_challenge[CHALLENGE_LEN];
  uint8_t device_proof[DEVICE_PROOF_LEN];
  bool    has_device_proof = false;
  uint8_t transcript[TRANSCRIPT_LEN];
};

static bool session_accept_hello(const uint8_t central_pub[32],
                                 const uint8_t *server_challenge /* 16 or null */,
                                 bool bound, uint32_t binding_version,
                                 bool physical_claim_authorized, HelloOut *out) {
  bool ok = false;
  uint8_t our_priv[32], raw_secret[32], hs[HS_CONTEXT_LEN];
  mbedtls_ecp_group grp; mbedtls_mpi d; mbedtls_ecp_point Q, P, R;
  mbedtls_ecp_group_init(&grp); mbedtls_mpi_init(&d);
  mbedtls_ecp_point_init(&Q); mbedtls_ecp_point_init(&P); mbedtls_ecp_point_init(&R);

  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) != 0) goto done;

  /* RFC 7748 §5 clamping — mbedTLS validates the scalar before the ladder. */
  rand_fill(our_priv, sizeof our_priv);
  our_priv[0] &= 0xF8; our_priv[31] &= 0x7F; our_priv[31] |= 0x40;
  if (mbedtls_mpi_read_binary_le(&d, our_priv, sizeof our_priv) != 0) goto done;
  if (mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, rng_cb, nullptr) != 0) goto done;
  if (mbedtls_mpi_write_binary_le(&Q.MBEDTLS_PRIVATE(X), out->peripheral_pub, 32) != 0) goto done;

  rand_fill(out->session_id, SESSION_ID_LEN);
  rand_fill(out->peripheral_challenge, CHALLENGE_LEN);

  /* §5.4 — build the canonical 127-byte context byte for byte. Never hash
   * the protobuf: field order and default-field omission are not stable. */
  memset(hs, 0, sizeof hs);
  memcpy(hs + HS_OFF_MAGIC, HS_MAGIC, 10);
  be32(hs + HS_OFF_VERSION, PROTOCOL_VERSION);
  memcpy(hs + HS_OFF_DEVICE_UID,     DEVICE_UID, 6);
  memcpy(hs + HS_OFF_CENTRAL_PUB,    central_pub, 32);
  memcpy(hs + HS_OFF_PERIPHERAL_PUB, out->peripheral_pub, 32);
  memcpy(hs + HS_OFF_SESSION_ID,     out->session_id, 4);
  hs[HS_OFF_BOUND]     = bound ? 1 : 0;
  hs[HS_OFF_PHYS_AUTH] = physical_claim_authorized ? 1 : 0;
  be32(hs + HS_OFF_BINDING_VER, binding_version);
  memcpy(hs + HS_OFF_PERIPH_CHAL, out->peripheral_challenge, 16);
  hs[HS_OFF_CHAL_PRESENT] = server_challenge ? 1 : 0;
  if (server_challenge) memcpy(hs + HS_OFF_SERVER_CHAL, server_challenge, 16);

  if (!sha256_of(hs, sizeof hs, out->transcript)) goto done;
  LOGD("handshake ctx: bound=%d phys=%d bver=%lu chal=%s",
       (int)bound, (int)physical_claim_authorized,
       (unsigned long)binding_version, server_challenge ? "yes" : "no");
  DUMP("central_pub", central_pub, 32);
  DUMP("periph_pub ", out->peripheral_pub, 32);
  DUMP("session_id ", out->session_id, 4);
  DUMP("periph_chal", out->peripheral_challenge, 16);
  DUMP("hs_context ", hs, HS_CONTEXT_LEN);
  DUMP("transcript ", out->transcript, TRANSCRIPT_LEN);

  /* §5.4 — device_proof = Trunc16(HMAC(claim_key, 0x02 || transcript)) */
  /* §9.7 — only a challenged HELLO gets a proof. Emitting it unconditionally
   * was tried on hardware: the app disconnected 57 ms after the ACK, every
   * time, instead of holding the link. */
  if (server_challenge) {
    uint8_t msg[1 + TRANSCRIPT_LEN], mac[32];
    msg[0] = HMAC_PREFIX_DEVICE_PROOF;
    memcpy(msg + 1, out->transcript, TRANSCRIPT_LEN);
    if (!factory_hmac(msg, sizeof msg, mac)) goto done;
    memcpy(out->device_proof, mac, DEVICE_PROOF_LEN);
    out->has_device_proof = true;
    mbedtls_platform_zeroize(mac, sizeof mac);
    LOGD("device_proof computed (challenge %s)", server_challenge ? "present" : "ABSENT — compat");
    DUMP("device_proof", out->device_proof, DEVICE_PROOF_LEN);
  }

  /* §5.3 step 9 — X25519, rejecting the all-zero (low-order point) result. */
  if (mbedtls_mpi_read_binary_le(&P.MBEDTLS_PRIVATE(X), central_pub, 32) != 0) goto done;
  if (mbedtls_mpi_lset(&P.MBEDTLS_PRIVATE(Z), 1) != 0) goto done;
  if (mbedtls_ecp_mul(&grp, &R, &d, &P, rng_cb, nullptr) != 0) goto done;
  if (mbedtls_mpi_write_binary_le(&R.MBEDTLS_PRIVATE(X), raw_secret, 32) != 0) goto done;
  { uint8_t acc = 0; for (int i = 0; i < 32; i++) acc |= raw_secret[i]; if (!acc) goto done; }

  /* §5.5 — pre_own_key = HMAC(key = transcript_hash, data = raw_secret).
   * NOTE: this is NOT an application key. It only authorizes ownership
   * operations; application traffic needs owner_key from flash as well. */
  if (!hmac_sha256(out->transcript, TRANSCRIPT_LEN, raw_secret, 32, S.pre_own_key)) goto done;
  memcpy(S.transcript, out->transcript, TRANSCRIPT_LEN);
  memcpy(S.hs_session_id, out->session_id, SESSION_ID_LEN);
  S.has_transcript = true;
  ok = true;

done:
  /* §5.3 step 11 — private key and raw secret die here, always. */
  mbedtls_platform_zeroize(our_priv, sizeof our_priv);
  mbedtls_platform_zeroize(raw_secret, sizeof raw_secret);
  mbedtls_platform_zeroize(hs, sizeof hs);
  mbedtls_ecp_point_free(&R); mbedtls_ecp_point_free(&P); mbedtls_ecp_point_free(&Q);
  mbedtls_mpi_free(&d); mbedtls_ecp_group_free(&grp);
  return ok;
}

static bool session_activate_pre_own() {
  return S.has_transcript && activate_epoch(EPOCH_PRE_OWN, S.pre_own_key, S.hs_session_id);
}
/* §6.1 — owner_session_key = HMAC(owner_key, pre_own_key), reusing the
 * handshake session_id.
 * §6.4 — after a BIND, next_sid appends to the HMAC input AND becomes the
 * nonce prefix, so the new epoch cannot overlap the old one inside the same
 * connection. Same derivation, one extra input: one function. */
static bool session_activate_owner(const uint8_t owner_key[OWNER_KEY_LEN],
                                   const uint8_t *next_sid /* 4 bytes, or nullptr */) {
  uint8_t msg[32 + SESSION_ID_LEN], osk[32];
  size_t mlen = 32;
  memcpy(msg, S.pre_own_key, 32);
  if (next_sid) { memcpy(msg + 32, next_sid, SESSION_ID_LEN); mlen += SESSION_ID_LEN; }
  bool ok = hmac_sha256(owner_key, OWNER_KEY_LEN, msg, mlen, osk) &&
            activate_epoch(EPOCH_OWNER, osk, next_sid ? next_sid : S.hs_session_id);
  mbedtls_platform_zeroize(msg, sizeof msg);
  mbedtls_platform_zeroize(osk, sizeof osk);
  return ok;
}

static void build_nonce(uint8_t n[NONCE_LEN], uint8_t dir, uint64_t counter) {
  memcpy(n, S.session_id, SESSION_ID_LEN);
  n[4] = dir;
  for (int i = 0; i < 7; i++) n[5 + i] = (uint8_t)(counter >> (8 * (6 - i)));  /* uint56 BE */
}

static bool session_encrypt(const uint8_t *plain, size_t plen,
                            uint8_t *out, size_t cap, size_t *out_len) {
  if (!session_is_secure() || cap < plen + GCM_TAG_LEN) return false;
  uint8_t nonce[NONCE_LEN]; build_nonce(nonce, DIR_P2C, S.tx);
  int rc = mbedtls_gcm_crypt_and_tag(&S.gcm, MBEDTLS_GCM_ENCRYPT, plen,
                                     nonce, NONCE_LEN, nullptr, 0,
                                     plain, out, GCM_TAG_LEN, out + plen);
  mbedtls_platform_zeroize(nonce, sizeof nonce);
  if (rc != 0) { LOGE("GCM encrypt failed rc=-0x%04x", -rc); return false; }
  *out_len = plen + GCM_TAG_LEN;         /* counter advances on tx_commit */
  LOGV("gcm encrypt %u→%u B  tx_ctr=%llu", (unsigned)plen, (unsigned)*out_len,
       (unsigned long long)S.tx);
  return true;
}
static bool session_decrypt(const uint8_t *in, size_t ilen,
                            uint8_t *out, size_t cap, size_t *out_len) {
  if (!session_is_secure() || ilen < GCM_TAG_LEN) return false;
  size_t clen = ilen - GCM_TAG_LEN;
  if (cap < clen) return false;
  uint8_t nonce[NONCE_LEN]; build_nonce(nonce, DIR_C2P, S.rx);
  int rc = mbedtls_gcm_auth_decrypt(&S.gcm, clen, nonce, NONCE_LEN, nullptr, 0,
                                    in + clen, GCM_TAG_LEN, in, out);
  mbedtls_platform_zeroize(nonce, sizeof nonce);
  /* §8 — a failure is final. Never retry a neighbouring counter and never
   * try the other epoch's key: the counter is not on the wire, so there is
   * no resynchronisation and any probing risks a (key, nonce) reuse. */
  if (rc != 0) {
    LOGE("GCM auth failed (epoch=%d rx_ctr=%llu len=%u) — tampered, wrong key, "
         "or counter desync", (int)S.epoch, (unsigned long long)S.rx, (unsigned)ilen);
    return false;
  }
  *out_len = clen;
  LOGV("gcm decrypt %u→%u B  rx_ctr=%llu", (unsigned)ilen, (unsigned)clen,
       (unsigned long long)S.rx);
  return true;
}
/* §8 — commit ONLY after the frame's last transport op (tx) / after GCM auth
 * AND structural validation (rx). Committing early desyncs permanently. */
static void tx_commit() { if (S.epoch == EPOCH_NONE) return;
                          if (S.tx >= RECORD_COUNTER_MAX) { session_reset(); return; } S.tx++; }
static void rx_commit() { if (S.epoch == EPOCH_NONE) return;
                          if (S.rx >= RECORD_COUNTER_MAX) { session_reset(); return; } S.rx++; }

/* ============================================================================
 *  §9  OWNERSHIP TOKEN VERIFICATION  (ECDSA-P256-SHA256, manufacturer-signed)
 *
 *  Order matters: cheap structural checks first, the ~10 ms scalar multiply
 *  last, and the §13 budget of 3 signature attempts is charged per TOKEN.
 *  Nothing here touches persistent state — the caller commits.
 * ==========================================================================*/

static uint8_t g_ecdsa_attempts = 0;

static bool ecdsa_verify_one(const uint8_t pub[65], const uint8_t *body, size_t blen,
                             const uint8_t sig[64]) {
  mbedtls_ecp_group grp; mbedtls_ecp_point Q; mbedtls_mpi r, s;
  uint8_t digest[32];
  bool ok = false;
  mbedtls_ecp_group_init(&grp); mbedtls_ecp_point_init(&Q);
  mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);

  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) goto done;
  if (mbedtls_ecp_point_read_binary(&grp, &Q, pub, 65) != 0) goto done;
  if (mbedtls_ecp_check_pubkey(&grp, &Q) != 0) goto done;
  if (mbedtls_mpi_read_binary(&r, sig, 32) != 0) goto done;
  if (mbedtls_mpi_read_binary(&s, sig + 32, 32) != 0) goto done;
  if (!sha256_of(body, blen, digest)) goto done;
  ok = mbedtls_ecdsa_verify(&grp, digest, sizeof digest, &Q, &r, &s) == 0;
done:
  mbedtls_platform_zeroize(digest, sizeof digest);
  mbedtls_mpi_free(&s); mbedtls_mpi_free(&r);
  mbedtls_ecp_point_free(&Q); mbedtls_ecp_group_free(&grp);
  return ok;
}

static bool token_signature_ok(const uint8_t *body, size_t blen, const uint8_t sig[64]) {
  if (g_ecdsa_attempts >= MAX_ECDSA_FAILURES) {
    LOGE("token: §13 signature budget exhausted (%d/%d)",
         (int)g_ecdsa_attempts, MAX_ECDSA_FAILURES);
    return false;
  }
  g_ecdsa_attempts++;
  int slots = 0;
  uint32_t t0 = millis();
  for (int i = 0; i < MAX_TRUST_KEYS; i++) {
    uint8_t acc = 0;
    for (int j = 0; j < 65; j++) acc |= MANUFACTURER_PUBKEYS[i][j];
    if (!acc) continue;                                        /* empty slot */
    slots++;
    if (ecdsa_verify_one(MANUFACTURER_PUBKEYS[i], body, blen, sig)) {
      LOGD("token: signature OK (trust key #%d, %lu ms, attempt %d/%d)",
           i, (unsigned long)(millis() - t0), (int)g_ecdsa_attempts, MAX_ECDSA_FAILURES);
      return true;
    }
  }
  if (!slots) LOGE("token: NO manufacturer public key provisioned — "
                   "fill MANUFACTURER_PUBKEYS before shipping");
  else        LOGE("token: signature rejected by all %d trust key(s) (attempt %d/%d)",
                   slots, (int)g_ecdsa_attempts, MAX_ECDSA_FAILURES);
  return false;
}

/* Shared prefix checks for both token kinds (§9.1 steps 3-6 / §9.2). */
static bool token_prefix_ok(const uint8_t *t, uint8_t action, uint32_t *out_version) {
  if (t[TOK_OFF_FORMAT] != TOKEN_FORMAT || t[TOK_OFF_ACTION] != action) {
    LOGE("token: format/action mismatch (got %02x/%02x, want 01/%02x)",
         t[TOK_OFF_FORMAT], t[TOK_OFF_ACTION], action);
    return false;
  }
  if (!ct_equal(t + TOK_OFF_DEVICE_UID, DEVICE_UID, 6)) {
    LOGE("token: issued for a different device UID"); return false;
  }
  uint32_t v = ((uint32_t)t[TOK_OFF_BINDING_VER] << 24) | ((uint32_t)t[TOK_OFF_BINDING_VER+1] << 16) |
               ((uint32_t)t[TOK_OFF_BINDING_VER+2] << 8) | t[TOK_OFF_BINDING_VER+3];
  if (v <= g_binding_version) {      /* strictly monotonic — replay defence */
    LOGE("token: version %lu not greater than stored %lu (replay?)",
         (unsigned long)v, (unsigned long)g_binding_version);
    return false;
  }
  *out_version = v;
  LOGD("token: prefix OK, new binding version %lu", (unsigned long)v);
  return true;
}

static bool token_verify_bind(const uint8_t *t, size_t tlen,
                              const uint8_t *owner_key, size_t oklen,
                              const uint8_t transcript[TRANSCRIPT_LEN],
                              uint32_t *out_version) {
  if (tlen != BIND_TOKEN_LEN || oklen != OWNER_KEY_LEN) {
    LOGE("token: bad lengths (token %u want %d, owner_key %u want %d)",
         (unsigned)tlen, BIND_TOKEN_LEN, (unsigned)oklen, OWNER_KEY_LEN);
    return false;
  }
  if (!token_prefix_ok(t, OWNERSHIP_ACTION_BIND, out_version)) return false;

  /* The key hash is what stops a valid token being paired with an
   * attacker-chosen owner_key. */
  uint8_t kh[32];
  if (!sha256_of(owner_key, oklen, kh)) return false;
  bool key_ok = ct_equal(t + BIND_TOK_OFF_KEY_HASH, kh, 32);
  mbedtls_platform_zeroize(kh, sizeof kh);
  if (!key_ok) { LOGE("token: SHA256(owner_key) does not match the token"); return false; }

  /* Binds the token to THIS connection — a token captured elsewhere is dead. */
  if (!ct_equal(t + BIND_TOK_OFF_TRANSCRIPT, transcript, TRANSCRIPT_LEN)) {
    LOGE("token: transcript mismatch — not issued for this connection");
    return false;
  }

  return token_signature_ok(t, BIND_TOKEN_BODY_LEN, t + BIND_TOKEN_BODY_LEN);
}

static bool token_verify_unbind(const uint8_t *t, size_t tlen,
                                const uint8_t transcript[TRANSCRIPT_LEN],
                                uint32_t *out_version) {
  if (tlen != UNBIND_TOKEN_LEN) {
    LOGE("token: bad unbind length %u want %d", (unsigned)tlen, UNBIND_TOKEN_LEN); return false;
  }
  if (!token_prefix_ok(t, OWNERSHIP_ACTION_UNBIND, out_version)) return false;
  if (!ct_equal(t + UNBIND_TOK_OFF_TRANSCRIPT, transcript, TRANSCRIPT_LEN)) {
    LOGE("token: transcript mismatch — not issued for this connection"); return false;
  }
  return token_signature_ok(t, UNBIND_TOKEN_BODY_LEN, t + UNBIND_TOKEN_BODY_LEN);
}

/* §18 — post_apply_proof, built AFTER the persistent commit: it states what
 * WAS applied, so producing one for an uncommitted change would be a lie. */
static bool build_apply_proof(uint8_t out[16], OwnershipAction action, uint32_t version,
                              bool resulting_bound, const uint8_t th[TRANSCRIPT_LEN]) {
  uint8_t in[APPLY_PROOF_INPUT_LEN], mac[32];
  memset(in, 0, sizeof in);
  in[AP_OFF_DOMAIN] = HMAC_PREFIX_APPLY_PROOF;
  in[AP_OFF_ACTION] = (uint8_t)action;
  memcpy(in + AP_OFF_DEVICE_UID, DEVICE_UID, 6);
  be32(in + AP_OFF_VERSION, version);
  in[AP_OFF_BOUND] = resulting_bound ? 1 : 0;
  memcpy(in + AP_OFF_TRANSCRIPT, th, TRANSCRIPT_LEN);
  bool ok = factory_hmac(in, sizeof in, mac);
  if (ok) memcpy(out, mac, 16);
  mbedtls_platform_zeroize(mac, sizeof mac);
  return ok;
}

/* ============================================================================
 *  §12  FRAGMENTATION  — "reassemble before decrypting"
 *
 *      byte 0    marker (FIRST / CONT / LAST / SINGLE)
 *      byte 1..  chunk
 *
 *  One frame in flight per direction, which is why there is no message id,
 *  fragment index or count. Anything the state machine does not allow
 *  discards the buffer and invalidates the connection.
 * ==========================================================================*/

static uint8_t  g_rx_frame[FRAG_MAX_FRAME];
static size_t   g_rx_len = 0;
static bool     g_rx_active = false;
static uint32_t g_rx_last_ms = 0;

static void on_frame(const uint8_t *frame, size_t len);      /* fwd */
static void invalidate_connection(const char *why);          /* fwd */

static void frag_reset() { g_rx_len = 0; g_rx_active = false; }

static void frag_feed(const uint8_t *d, size_t n) {
  if (n < 1) { invalidate_connection("empty fragment"); return; }
  uint8_t marker = d[0];
  const uint8_t *chunk = d + 1; size_t clen = n - 1;
  g_rx_last_ms = millis();
  LOGV("← frag %s %u B%s",
       marker == FRAG_SINGLE ? "SINGLE" : marker == FRAG_FIRST ? "FIRST" :
       marker == FRAG_CONT   ? "CONT"   : marker == FRAG_LAST  ? "LAST" : "BAD",
       (unsigned)clen, g_rx_active ? " (reassembling)" : "");

  switch (marker) {
    case FRAG_SINGLE:
      if (g_rx_active) { frag_reset(); invalidate_connection("SINGLE during reassembly"); return; }
      on_frame(chunk, clen);
      return;
    case FRAG_FIRST:
      if (g_rx_active) { frag_reset(); invalidate_connection("second simultaneous reassembly"); return; }
      if (clen > sizeof g_rx_frame) { invalidate_connection("frame overflow"); return; }
      memcpy(g_rx_frame, chunk, clen); g_rx_len = clen; g_rx_active = true;
      return;
    case FRAG_CONT:
    case FRAG_LAST:
      if (!g_rx_active) { invalidate_connection("continuation without FIRST"); return; }
      if (g_rx_len + clen > sizeof g_rx_frame) { frag_reset(); invalidate_connection("frame overflow"); return; }
      memcpy(g_rx_frame + g_rx_len, chunk, clen); g_rx_len += clen;
      if (marker == FRAG_LAST) { g_rx_active = false; on_frame(g_rx_frame, g_rx_len); }
      return;
    default:
      frag_reset(); invalidate_connection("unknown fragment header");
      return;
  }
}
static void frag_tick() {
  if (g_rx_active && millis() - g_rx_last_ms > FRAG_TIMEOUT_MS) {
    frag_reset(); invalidate_connection("reassembly timeout");
  }
}

/* §12.1 — payload budget per ATT operation = MTU − 3 − 1, ATT-capped. */
static size_t frag_max_payload(uint16_t mtu) {
  if (mtu < MTU_MIN) mtu = MTU_MIN;
  size_t v = (size_t)mtu - 3;
  if (v > ATT_MAX_VALUE_LEN) v = ATT_MAX_VALUE_LEN;
  return v - 1;
}

/* ============================================================================
 *  GATT SERVER  (NimBLE — h2zero esp-nimble-cpp)
 *
 *  ── CRITICAL GOTCHA: never run the crypto in a BLE callback ───────────────
 *  NimBLE's host dispatches GATT callbacks from the NimBLE host task, whose
 *  stack is sized for protocol housekeeping, not for mbedTLS. The handshake
 *  needs mbedTLS X25519 + HMAC + AES-GCM (and BIND additionally does an
 *  ECDSA verify), plus it calls indicate() and blocks on its confirmation.
 *  Doing that inline in onWrite()/onSubscribe() overflows the host task's
 *  stack and/or stalls the NimBLE host loop, which starves every other GATT
 *  operation on the link.
 *
 *  So: onWrite() copies the bytes into a queue and returns. Every byte of
 *  protocol state below is touched by ONE task — wyn_worker — which also
 *  runs the §12.5 reassembly timeout and the §13 deadlines on its queue
 *  receive timeout. That single-threading is not a style choice: the §8
 *  counters cannot tolerate two writers.
 * ==========================================================================*/

static NimBLEServer         *g_server    = nullptr;
static NimBLECharacteristic *g_chr_event = nullptr;
static uint16_t               g_conn_id  = 0;

/* §2.2 / §9.4 step 1 — the central MUST enable EVENT indications before it
 * sends HELLO. Tracked from NimBLECharacteristicCallbacks::onSubscribe()
 * below (NimBLE creates and owns the CCCD itself for a NOTIFY/INDICATE
 * characteristic, so there is no separate BLE2902 descriptor to read here
 * the way the Bluedroid port had). Reading the CCCD turns "the HELLO_ACK
 * vanished" into a named error instead of a silent stall. */
static volatile bool      g_event_subscribed = false;
static bool event_subscribed() { return g_event_subscribed; }
static bool               g_connected = false;
static uint16_t           g_mtu = MTU_MIN;

/* One inbound GATT write, copied out of the BLE stack (MTU 517 + headroom). */
typedef struct { uint16_t len; uint8_t data[600]; } in_msg_t;
static QueueHandle_t     g_in_queue  = nullptr;
/* Given by onStatus() when the central confirms an indication. */
static SemaphoreHandle_t g_ind_done  = nullptr;
static volatile bool     g_ind_ok    = false;

/* ===================== CUSTOMER DEBUG PATCH START =====================
 * Some Android BLE stacks need a short settling time after enabling CCCD
 * indications before they can confirm the first indication. This delay is
 * intentionally small and affects only EVT_HELLO_ACK, so it helps distinguish
 * a firmware/app timing issue from a protocol issue.
 *
 * Please keep these logs in the customer test build and send back the
 * console output around:
 *   EVENT CCCD enabled -> CMD_HELLO -> EVT_HELLO_ACK -> indication status.
 * ====================== CUSTOMER DEBUG PATCH END ====================== */
static const uint32_t CUSTOMER_DEBUG_HELLO_ACK_DELAY_MS = 250;

/* One transport operation = one CONFIRMED indication. §8 may only advance the
 * TX counter once the central has acknowledged the LAST fragment; NimBLE
 * reports that asynchronously through NimBLECharacteristicCallbacks::onStatus,
 * so we block here until it lands. An unconfirmed indication means the
 * frame's completion is uncertain and the session must die. */
static bool send_fragment(const uint8_t *frag, size_t len) {
  if (!g_chr_event || !g_connected) return false;
  xSemaphoreTake(g_ind_done, 0);                 /* drain a stale give      */
  g_ind_ok = false;
  /* ===================== CUSTOMER DEBUG PATCH START ===================== */
  LOGI("CUSTOMER DEBUG: indicate start len=%u subscribed=%d connected=%d mtu=%u",
       (unsigned)len, (int)event_subscribed(), (int)g_connected, (unsigned)g_mtu);
  /* ====================== CUSTOMER DEBUG PATCH END ====================== */
  g_chr_event->setValue((uint8_t *)frag, len);
  g_chr_event->indicate();
  if (xSemaphoreTake(g_ind_done, pdMS_TO_TICKS(3000)) != pdTRUE) {
    LOGE("indication timed out — no confirmation from the central");
    return false;
  }
  /* ===================== CUSTOMER DEBUG PATCH START ===================== */
  LOGI("CUSTOMER DEBUG: indicate done ok=%d", (int)g_ind_ok);
  /* ====================== CUSTOMER DEBUG PATCH END ====================== */
  return g_ind_ok;
}

static bool frag_send_split(const uint8_t *data, size_t len) {
  size_t budget = frag_max_payload(g_mtu);
  uint8_t out[1 + ATT_MAX_VALUE_LEN];
  LOGV("→ frame %u B, mtu=%u, budget=%u/frag", (unsigned)len, (unsigned)g_mtu, (unsigned)budget);
  if (len <= budget) {
    out[0] = FRAG_SINGLE; memcpy(out + 1, data, len);
    return send_fragment(out, len + 1);
  }
  size_t off = 0;
  while (off < len) {
    size_t take = len - off < budget ? len - off : budget;
    bool last = (off + take == len);
    out[0] = off == 0 ? FRAG_FIRST : (last ? FRAG_LAST : FRAG_CONT);
    memcpy(out + 1, data + off, take);
    if (!send_fragment(out, take + 1)) {
      LOGE("indication not confirmed at offset %u/%u", (unsigned)off, (unsigned)len);
      return false;
    }
    off += take;
  }
  return true;
}

/* ============================================================================
 *  §6/§7  DISPATCH STATE MACHINE
 * ==========================================================================*/

enum Auth : uint8_t {
  AUTH_DISCONNECTED = 0,
  AUTH_HELLO_PENDING,      /* plaintext CMD_HELLO is the only legal frame  */
  AUTH_PRE_OWN_NOAUTH,     /* §12.3 probe-only: HELLO_ACK given, all DENY  */
  AUTH_PRE_OWN,            /* pre_own_key — ownership commands only        */
  AUTH_BIND_ACK_PENDING,   /* no inbound frame is legal here               */
  AUTH_OWNER_CONFIRM,      /* owner key active, awaiting GET_DEVICE_CAP    */
  AUTH_OWNER_ACTIVE,       /* application traffic allowed                  */
};

static Auth     g_auth = AUTH_DISCONNECTED;
static bool     g_pre_own_bound = false;     /* bound state at HELLO time  */
static bool     g_hello_had_challenge = false;
static bool     g_bulk_sync_done = false;
static uint8_t  g_next_session_id[SESSION_ID_LEN];
static bool     g_want_disconnect = false;

/* §13 — the deadlines are mutually exclusive by construction (HELLO is cleared
 * before PRE_OWN is armed; BIND clears PRE_OWN before arming the confirmation),
 * so one timer carrying its own expiry message covers all of them. */
static uint32_t    g_deadline = 0;
static const char *g_deadline_why = "";

static void arm_deadline(uint32_t ms, const char *why) {
  g_deadline = millis() + ms;
  if (!g_deadline) g_deadline = 1;   /* 0 means disarmed — never let the wrap land there */
  g_deadline_why = why;
}

static const char *auth_name(Auth a) {
  switch (a) {
    case AUTH_HELLO_PENDING:    return "HelloPending";
    case AUTH_PRE_OWN_NOAUTH:   return "PreOwnerNoAuth";
    case AUTH_PRE_OWN:          return "PreOwn";
    case AUTH_BIND_ACK_PENDING: return "BindAckPending";
    case AUTH_OWNER_CONFIRM:    return "OwnerConfirmPending";
    case AUTH_OWNER_ACTIVE:     return "OwnerActive";
    default:                    return "Disconnected";
  }
}

/* Everything derived from the link dies here — used for the disconnect edge,
 * and by invalidate_connection() for every protocol violation. */
static void connection_closed() {
  session_reset();
  frag_reset();
  claim_clear();
  g_auth = AUTH_DISCONNECTED;
  g_deadline = 0;
  g_speed_limit_valid = false;
  g_no_overtaking_active = false;
  g_camera_active = false;

}

static void invalidate_connection(const char *why) {
  LOGE("INVALIDATED in %s — %s  (keys wiped, link dropping)", auth_name(g_auth), why);
  connection_closed();
  g_want_disconnect = true;               /* actually dropped in loop()    */
}

static bool send_event(int32_t type, int32_t status,
                       const uint8_t *payload, size_t plen, bool encrypt) {
  uint8_t evt[256], cipher[256 + GCM_TAG_LEN];
  size_t elen = EventPacket_encode(evt, sizeof evt, type, status, payload, plen);
  if (!elen) { invalidate_connection("event encode overflow"); return false; }
  LOGI("→ EVT_%s %s  payload=%u B  %s", evt_name(type), status_name(status),
       (unsigned)plen, encrypt ? "encrypted" : "PLAINTEXT");

  if (!encrypt) return frag_send_split(evt, elen);   /* EVT_HELLO_ACK only */

  size_t clen = 0;
  if (!session_encrypt(evt, elen, cipher, sizeof cipher, &clen)) {
    invalidate_connection("event encryption failed"); return false;
  }
  if (!frag_send_split(cipher, clen)) {
    invalidate_connection("event write did not complete"); return false;
  }
  tx_commit();                            /* §8 — only after the last op   */
  return true;
}

/* §19.2 — a PROTOCOL-VALID request that is simply not allowed (wrong state,
 * bad token semantics, stale version) owes the peer an empty EVT_ACK with
 * STATUS_DENIED before the link drops. Only STRUCTURAL failures (malformed,
 * unknown/duplicate fields, GCM failure, fragment errors) go silent — there
 * the session is no longer trustworthy enough to encrypt a reply.
 * §9.3: the reason is never told to the peer, only to the console log. */
static void deny_and_close(const char *why) {
  send_event(EVT_ACK, STATUS_DENIED, nullptr, 0, /*encrypt=*/true);
  rate_limit_note_unauth();   /* §20 — a failed ownership attempt counts too */
  invalidate_connection(why);
}

/* ── §5.3  plaintext CMD_HELLO ───────────────────────────────────────────── */
static void handle_hello_frame(const uint8_t *frame, size_t len) {
  LOGI("← plaintext frame %u B (expecting CMD_HELLO)", (unsigned)len);
  /* §7.2 — ownership state is UNKNOWN here, and a HELLO_ACK cannot be emitted
   * without asserting a bound flag we have no honest value for. */
  if (g_storage_fault) { invalidate_connection("OWNERSHIP_STORAGE_FAULT"); return; }
  if (!event_subscribed()) {
    invalidate_connection("central sent HELLO without subscribing to EVENT indications");
    return;
  }
  CommandPacket cmd = CommandPacket_decode(frame, len);
  if (!cmd.ok)          { invalidate_connection("plaintext frame is not a CommandPacket"); return; }
  /* A v2.5.1 central's envelope carries the now-reserved sequence/timestamp/
   * schema_version fields and trips exactly here. */
  if (cmd.has_unknown)  {
    LOGE("HELLO envelope carries field %u%s", (unsigned)cmd.unknown_field,
         cmd.unknown_field == 1 ? " (v2.5.1 sequence)" :
         cmd.unknown_field == 4 ? " (v2.5.1 timestamp)" :
         cmd.unknown_field == 5 ? " (v2.5.1 schema_version)" : "");
    invalidate_connection("HELLO envelope has unknown/reserved fields"); return;
  }
  if (cmd.type != CMD_HELLO) { invalidate_connection("first frame is not CMD_HELLO"); return; }
  if (!cmd.has_payload) { invalidate_connection("CMD_HELLO has no payload"); return; }

  HelloPayload hp = HelloPayload_decode(cmd.payload, cmd.payload_len);
  if (!hp.ok || hp.has_unknown)  { invalidate_connection("HelloPayload malformed"); return; }
  if (hp.pub_len != X25519_KEY_LEN) { invalidate_connection("HELLO public_key length"); return; }
  if (hp.protocol_version != PROTOCOL_VERSION) {
    LOGE("central speaks protocol %u.%02u, we speak %u.%02u",
         (unsigned)(hp.protocol_version >> 16), (unsigned)(hp.protocol_version & 0xffff),
         (unsigned)(PROTOCOL_VERSION >> 16), (unsigned)(PROTOCOL_VERSION & 0xffff));
    invalidate_connection("unsupported protocol version"); return;
  }
  if (hp.challenge_len != 0 && hp.challenge_len != CHALLENGE_LEN) {
    invalidate_connection("server_claim_challenge length"); return;
  }

  /* §10.1 — RNG health failure fails closed and does not accept the HELLO.
   * Checked here, before step 6 of the §8.3 accept list, so a dead entropy
   * source can never reach X25519 key generation. */
  if (!rng_health_ok()) {
    invalidate_connection("RNG health check failed — refusing HELLO (§10.1)"); return;
  }

  /* §20 — an unauthenticated HELLO pays the accumulated backoff before it gets
   * an answer. Applied here so the delay is charged to the caller that earned
   * it, and never to a challenged claim or a bound owner reconnecting. */
  if (!g_bound && hp.challenge_len == 0) rate_limit_penalty();

  g_deadline = 0;

  bool bound = g_bound;
  /* §8.3/§8.4 (v2.7.13.1): only a bound==false HELLO carrying the full
   * 16-byte server_claim_challenge may consume the physical-claim arm.
   */
  bool eligible = !bound && hp.challenge_len == CHALLENGE_LEN;
  bool phys     = eligible && claim_armed_now();
  LOGI("CMD_HELLO ok: proto 1.01, challenge=%s, local state bound=%d v%lu, physical=%d",
       hp.challenge_len ? "yes" : "no", (int)bound,
       (unsigned long)g_binding_version, (int)phys);

  /* §8.4 single-HELLO rule. Loud, because the alternative reads exactly like
   * "the button was not held long enough" and sends you debugging the wrong
   * thing (see also §8.4.1: a nearby Central can spend the arm first). */
  if (eligible && !phys && claim_window_open())
    LOGE("claim arm already spent by an earlier HELLO in this window (§8.4) — "
         "press the claim button again to get another attempt");

  /* Name the precondition that actually failed. "Hold the button" is wrong
   * advice when the button is already held and the challenge is what is
   * missing, and it costs a bench session to find that out. */
  if (!bound && !phys) {
    bool win = claim_window_open();
    if (!win && hp.challenge_len != CHALLENGE_LEN)
      LOGI("no physical claim: claim window CLOSED and HELLO carries no 16-byte "
           "challenge — both are required (§8.3). Probe only, BIND refused");
    else if (!win)
      LOGI("no physical claim: challenge is fine, but the claim window is "
           "CLOSED — hold the claim button %d s (§8.3). Probe only, BIND refused",
           CLAIM_HOLD_MS / 1000);
    else
      LOGI("no physical claim: the claim window IS OPEN, but this HELLO carries "
           "no 16-byte server_claim_challenge, so it is not claim-eligible "
           "(§8.3). The button is not the problem — the central must send a "
           "challenge. Probe only, BIND refused");
  }

  HelloOut out;
  if (!session_accept_hello(hp.pub, hp.challenge_len ? hp.challenge : nullptr,
                            bound, g_binding_version, phys, &out)) {
    invalidate_connection("handshake computation failed"); return;
  }
  /* §5.3 step 7 — only THIS connection's BIND can spend the claim window. */
  if (phys) claim_latch(out.transcript);

  uint8_t ack[160];
  size_t alen = HelloAckPayload_encode(ack, sizeof ack, out.peripheral_pub, out.session_id,
                                       bound, g_binding_version, out.peripheral_challenge,
                                       out.has_device_proof ? out.device_proof : nullptr, phys);
  if (!alen) { invalidate_connection("HELLO_ACK encode overflow"); return; }
  /* ===================== CUSTOMER DEBUG PATCH START =====================
   * Android-side timing check: delay a little before the first plaintext
   * indication. If the customer device starts receiving HELLO_ACK after this,
   * their app/phone stack likely sent CMD_HELLO before CCCD indication setup
   * had fully settled.
   * ====================== CUSTOMER DEBUG PATCH END ====================== */
  LOGI("CUSTOMER DEBUG: waiting %lu ms before EVT_HELLO_ACK indication",
       (unsigned long)CUSTOMER_DEBUG_HELLO_ACK_DELAY_MS);
  vTaskDelay(pdMS_TO_TICKS(CUSTOMER_DEBUG_HELLO_ACK_DELAY_MS));
  if (!send_event(EVT_HELLO_ACK, STATUS_OK, ack, alen, /*encrypt=*/false)) {
    invalidate_connection("HELLO_ACK write did not complete"); return;
  }

  g_bulk_sync_done      = false;
  g_pre_own_bound       = bound;
  g_hello_had_challenge = hp.challenge_len != 0;

  /* §9.2 challenge matrix — the epoch and state are chosen by LOCAL state and
   * the challenge, never by anything the central asserts:
   *      unbound + challenge  → PRE_OWN,        claim/rebind control
   *      unbound + no chal.   → PRE_OWN_NOAUTH, probe only (§12.3)
   *      bound   + challenge  → PRE_OWN,        ownership maintenance
   *      bound   + no chal.   → OWNER,          confirmation pending
   * All four run on pre_own_key with counters at 0 (§11.5); only the last
   * one goes straight to the direct owner key. */
  /* The only row that skips pre_own entirely. */
  if (bound && !g_hello_had_challenge) {
    uint8_t ok_key[OWNER_KEY_LEN];
    bool ok = ownership_get_owner_key(ok_key) && session_activate_owner(ok_key, nullptr);
    mbedtls_platform_zeroize(ok_key, sizeof ok_key);
    if (!ok) { invalidate_connection("owner_session_key derivation failed"); return; }
    g_auth = AUTH_OWNER_CONFIRM;
    arm_deadline(OWNER_CONFIRM_MS, "owner confirmation deadline expired");
    LOGI("state → OwnerConfirmPending — awaiting CMD_GET_DEVICE_CAP within %d s",
         OWNER_CONFIRM_MS / 1000);
    return;
  }

  if (!session_activate_pre_own()) { invalidate_connection("pre_own activation failed"); return; }
  if (!bound && !g_hello_had_challenge) {
    g_auth = AUTH_PRE_OWN_NOAUTH;
    rate_limit_note_unauth();          /* §20 — this one was unauthenticated */
    arm_deadline(PROBE_IDLE_MS, "probe deadline expired (§12.3)");
    LOGI("state → PreOwnerNoAuth — probe only, every command is DENIED; "
         "closing in %d s if the central does not disconnect", PROBE_IDLE_MS / 1000);
  } else {
    g_auth = AUTH_PRE_OWN;
    arm_deadline(PRE_OWN_IDLE_MS, "PRE_OWN idle deadline expired");
    LOGI("state → PreOwn (%s) — application commands are refused here; "
         "idle deadline %d s", bound ? "ownership maintenance" : "first claim",
         PRE_OWN_IDLE_MS / 1000);
  }
}

/* ── §9.1  CMD_BIND ──────────────────────────────────────────────────────── */
static void handle_bind(const CommandPacket &cmd) {
  /* §11.8 — only on an unbound device, in PRE_OWN, on a challenged HELLO. */
  if (g_auth != AUTH_PRE_OWN || g_pre_own_bound) { deny_and_close("CMD_BIND outside PRE_OWN-unbound"); return; }
  /* §15.3 requires a challenged HELLO. Compat drops that one precondition —
   * every other BIND check still applies unchanged: the token must be 140 B,
   * carry this UID, hash to this owner_key, quote THIS transcript, be signed
   * by a trust anchor, and beat the stored binding_version, and the physical
   * claim latch must match. A forged BIND is no easier than before. */
  if (!g_hello_had_challenge) { deny_and_close("CMD_BIND on an unchallenged HELLO"); return; }
  if (cmd.has_unknown)        { invalidate_connection("BIND envelope has unknown fields"); return; }

  BindPayload bp = BindPayload_decode(cmd.payload, cmd.payload_len);
  if (!cmd.has_payload || !bp.ok || bp.has_unknown || !bp.owner_key || !bp.token) {
    invalidate_connection("BindPayload malformed"); return;
  }
  if (!S.has_transcript) { invalidate_connection("no transcript for BIND"); return; }
  LOGI("CMD_BIND: owner_key %u B, token %u B — verifying",
       (unsigned)bp.owner_key_len, (unsigned)bp.token_len);

  uint32_t new_version = 0;
  if (!token_verify_bind(bp.token, bp.token_len, bp.owner_key, bp.owner_key_len,
                         S.transcript, &new_version)) {
    /* §9.3 — never tell the peer which check failed; §19.2 still owes it a
     * DENIED before the drop. */
    deny_and_close("BIND token rejected"); return;
  }
  if (!claim_matches(S.transcript)) {
    LOGE("claim latch: latched=%d consumed=%d window_open=%d — the BIND must "
         "come from the same connection that opened the window",
         (int)g_claim_latched, (int)g_claim_consumed, (int)claim_window_open());
    deny_and_close("BIND without a matching claim latch"); return;
  }
  LOGD("all §9.1 checks passed — committing to flash");

  /* §15.4 — persist BEFORE acknowledging. Nothing above touched flash. */
  if (!ownership_commit(bp.owner_key, new_version)) {
    send_event(EVT_ACK, STATUS_ERROR, nullptr, 0, true);
    invalidate_connection("ownership commit failed"); return;
  }
  rate_limit_reset();   /* §20 — an authenticated claim clears the backoff */
  /* §8.4 — "clear latch khi BIND commit thành công". Closing the WINDOW too,
   * not just marking the latch consumed: otherwise a BIND followed by an
   * UNBIND inside the same 60 s leaves the device unbound with the window
   * still open, and the next challenged HELLO would be authorized without
   * anyone touching the button again. */
  g_claim_consumed = true;
  g_claim_deadline_ms = 0;

  uint8_t proof[16];
  if (!build_apply_proof(proof, OWNERSHIP_ACTION_BIND, new_version, true, S.transcript)) {
    invalidate_connection("BIND apply proof failed"); return;
  }
  /* §10.1 — next_session_id must differ from session_id, or the new epoch
   * would reuse the old nonce prefix. */
  do { rand_fill(g_next_session_id, SESSION_ID_LEN); }
  while (ct_equal(g_next_session_id, S.hs_session_id, SESSION_ID_LEN));

  uint8_t ack[64];
  size_t alen = OwnershipApplyAck_encode(ack, sizeof ack, OWNERSHIP_ACTION_BIND,
                                         new_version, true, proof, g_next_session_id);
  if (!alen) { invalidate_connection("OwnershipApplyAck encode overflow"); return; }

  g_auth = AUTH_BIND_ACK_PENDING;
  /* Still under pre_own_key — the central cannot derive the new key until it
   * has this ACK, and the confirmed indication IS the §6.4 switch point. */
  if (!send_event(EVT_ACK, STATUS_OK, ack, alen, true)) {
    /* §6.4 — if the ACK is lost the central reconnects. NEVER roll back the
     * persistent bind; it learns the truth from the next HELLO_ACK. */
    invalidate_connection("BIND ACK write did not complete"); return;
  }

  uint8_t okey[OWNER_KEY_LEN];
  bool ok = ownership_get_owner_key(okey) &&
            session_activate_owner(okey, g_next_session_id);
  mbedtls_platform_zeroize(okey, sizeof okey);
  mbedtls_platform_zeroize(g_next_session_id, sizeof g_next_session_id);
  if (!ok) { invalidate_connection("post-BIND key switch failed"); return; }

  g_pre_own_bound = true;
  g_auth          = AUTH_OWNER_CONFIRM;
  arm_deadline(OWNER_CONFIRM_MS, "owner confirmation deadline expired");
  LOGI("BIND complete: bound at v%lu, key upgraded, state → OwnerConfirmPending",
       (unsigned long)new_version);
}

/* ── §9.2  CMD_APPLY_OWNERSHIP_TOKEN (unbind) ────────────────────────────── */
static void handle_unbind(const CommandPacket &cmd) {
  /* §11.8 — PRE_OWN on a BOUND device (maintenance) or OwnerActive. Never on
   * an unbound device: there is nothing to release. */
  bool allowed = (g_auth == AUTH_PRE_OWN && g_pre_own_bound) || g_auth == AUTH_OWNER_ACTIVE;
  if (!allowed)        { deny_and_close("UNBIND not permitted here"); return; }
  if (cmd.has_unknown) { invalidate_connection("UNBIND envelope has unknown fields"); return; }

  UnbindPayload up = UnbindPayload_decode(cmd.payload, cmd.payload_len);
  if (!cmd.has_payload || !up.ok || up.has_unknown || !up.token) {
    invalidate_connection("ApplyOwnershipTokenPayload malformed"); return;
  }
  if (!g_bound)          { deny_and_close("UNBIND on an unbound device"); return; }
  if (!S.has_transcript) { invalidate_connection("no transcript for UNBIND"); return; }
  LOGI("CMD_APPLY_OWNERSHIP_TOKEN: unbind token %u B — verifying",
       (unsigned)up.token_len);

  uint32_t new_version = 0;
  if (!token_verify_unbind(up.token, up.token_len, S.transcript, &new_version)) {
    deny_and_close("UNBIND token rejected"); return;
  }
  if (!ownership_commit(nullptr, new_version)) {
    send_event(EVT_ACK, STATUS_ERROR, nullptr, 0, true);
    invalidate_connection("unbind commit failed"); return;
  }

  uint8_t proof[16], ack[64];
  size_t alen = 0;
  if (build_apply_proof(proof, OWNERSHIP_ACTION_UNBIND, new_version, false, S.transcript))
    alen = OwnershipApplyAck_encode(ack, sizeof ack, OWNERSHIP_ACTION_UNBIND,
                                    new_version, false, proof, nullptr);
  if (!alen) { invalidate_connection("UNBIND ACK could not be built"); return; }

  LOGI("UNBIND committed v%lu — disconnecting", (unsigned long)new_version);
  send_event(EVT_ACK, STATUS_OK, ack, alen, true);
  invalidate_connection("unbind complete");
}

/* ── §11.3  CMD_GET_DEVICE_CAP — doubles as the §6.1 owner confirmation ──── */
static void handle_get_device_cap() {
  if (g_auth != AUTH_OWNER_CONFIRM && g_auth != AUTH_OWNER_ACTIVE) {
    deny_and_close("GET_DEVICE_CAP before owner confirmation"); return;
  }
  uint8_t cap[96];
  size_t clen = DeviceCapPayload_encode(cap, sizeof cap);
  if (!clen) { invalidate_connection("DeviceCap encode overflow"); return; }
  if (!send_event(EVT_DEVICE_CAP, STATUS_OK, cap, clen, true)) return;

  /* A central that could not derive owner_session_key could not have produced
   * a decryptable request — answering one at all is the proof. */
  if (g_auth == AUTH_OWNER_CONFIRM) {
    g_auth = AUTH_OWNER_ACTIVE;
    g_deadline = 0;
    LOGI("owner confirmed — state → OwnerActive, ready for traffic");
  }
}

/* ── §11.4  CMD_UPDATE_INFO / CMD_BULK ───────────────────────────────────── */
static void handle_update_info(const CommandPacket &cmd) {
  if (g_auth != AUTH_OWNER_ACTIVE) { deny_and_close("UPDATE_INFO before OwnerActive"); return; }
  if (cmd.has_unknown || !cmd.has_payload) { invalidate_connection("UPDATE_INFO malformed"); return; }
  if (!UpdateInfo_apply(cmd.payload, cmd.payload_len)) {
    invalidate_connection("UpdateInfoPayload malformed"); return;
  }
  /* §11.4 — streaming updates are unacknowledged; only the first BULK
   * (the initial full sync) gets an EVT_ACK. */
}

static void handle_bulk(const CommandPacket &cmd) {
  if (g_auth != AUTH_OWNER_ACTIVE) { deny_and_close("BULK before OwnerActive"); return; }
  if (cmd.has_unknown || !cmd.has_payload) { invalidate_connection("BULK malformed"); return; }

  PbIn in(cmd.payload, cmd.payload_len);
  int items = 0;
  while (in.next()) {
    if (in.field != 1 || in.wt != 2) { invalidate_connection("BulkPayload malformed"); return; }
    if (++items > BULK_MAX_ITEMS)    { invalidate_connection("BULK item count"); return; }
    if (!UpdateInfo_apply(in.data, in.dlen)) { invalidate_connection("BULK item malformed"); return; }
  }
  if (in.bad || items == 0) { invalidate_connection("BulkPayload malformed"); return; }
  LOGD("BULK applied: %d item(s)%s", items, g_bulk_sync_done ? "" : " (initial sync)");

  if (!g_bulk_sync_done) { g_bulk_sync_done = true; send_event(EVT_ACK, STATUS_OK, nullptr, 0, true); }
}

/* ── §12.6  secure frame → decrypt → validate → commit → dispatch ────────── */
static void handle_secure_frame(const uint8_t *frame, size_t len) {
  static uint8_t plain[FRAG_MAX_FRAME];
  size_t plen = 0;
  if (!session_decrypt(frame, len, plain, sizeof plain, &plen)) {
    invalidate_connection("AES-GCM authentication failed"); return;
  }
  CommandPacket cmd = CommandPacket_decode(plain, plen);
  if (!cmd.ok)      { invalidate_connection("decrypted CommandPacket malformed"); return; }
  if (!cmd.type)    { invalidate_connection("envelope carries no executable command"); return; }

  /* §8 — the frame authenticated and parsed, so the counter advances now,
   * before dispatch. Even a DENIED command keeps both sides in step. */
  rx_commit();
  LOGI("← CMD_%s (%u B) in %s", cmd_name(cmd.type), (unsigned)plen, auth_name(g_auth));

  /* §13 — every column of the authorization matrix is DENY in probe-only
   * no-auth. Deny once, here, rather than in each handler. */
  if (g_auth == AUTH_PRE_OWN_NOAUTH) {
    deny_and_close("command in PreOwnerNoAuth (probe-only state)"); return;
  }

  switch (cmd.type) {
    case CMD_BIND:                  handle_bind(cmd);        break;
    case CMD_APPLY_OWNERSHIP_TOKEN: handle_unbind(cmd);      break;
    case CMD_GET_DEVICE_CAP:        handle_get_device_cap(); break;
    case CMD_UPDATE_INFO:           handle_update_info(cmd); break;
    case CMD_BULK:                  handle_bulk(cmd);        break;
    case CMD_HELLO:                 /* §5.3 — exactly one HELLO, plaintext */
      invalidate_connection("encrypted CMD_HELLO"); break;
    default:
      deny_and_close("unknown command type"); break;
  }
}

static void on_frame(const uint8_t *frame, size_t len) {
  switch (g_auth) {
    case AUTH_HELLO_PENDING:
      handle_hello_frame(frame, len);
      break;
    case AUTH_PRE_OWN:
      arm_deadline(PRE_OWN_IDLE_MS, "PRE_OWN idle deadline expired");  /* §20 — per frame */
      handle_secure_frame(frame, len);
      break;
    case AUTH_PRE_OWN_NOAUTH:
      handle_secure_frame(frame, len);   /* denies and closes — §12.3 */
      break;
    case AUTH_OWNER_CONFIRM:
    case AUTH_OWNER_ACTIVE:
      handle_secure_frame(frame, len);
      break;
    case AUTH_BIND_ACK_PENDING:
      invalidate_connection("frame during BIND ACK");
      break;
    default:
      invalidate_connection("frame in a state that accepts none");
      break;
  }
}

/* ============================================================================
 *  BLE CALLBACKS  (NimBLE host task — minimum work only)
 * ==========================================================================*/

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *s, NimBLEConnInfo &connInfo) override {
    g_conn_id   = connInfo.getConnHandle();
    g_connected = true;
    g_mtu       = MTU_MIN;                 /* until the central negotiates  */
    /* Flags only — the worker resets protocol state on its next tick, so
     * nothing here races with a frame already in the queue. */
    printf("\n");
    LOGI("=== CONNECTED (conn %u) ===", (unsigned)g_conn_id);

    /* Match the connection parameters the .ino reference got "for free" from
     * arduino-esp32's Bluedroid BLEDevice/BLEServer, which requests these
     * right after connect: min interval 20 ms, max interval 40 ms, latency
     * 0, supervision timeout 4000 ms. The .ino never set these explicitly —
     * they are that library's own defaults — but NimBLE-Arduino's defaults
     * are looser, and some Android BLE stacks disconnect early against
     * looser parameters. Requesting the same values here reproduces the
     * .ino's behavior instead of NimBLE-Arduino's.
     *
     * DIAGNOSTIC NOTE: if the very first EVT_HELLO_ACK indication now times
     * out with no confirmation at all (instead of the earlier "phone
     * disconnects immediately" symptom), this update request landing right
     * at connect — before the central has finished service discovery — is
     * the likely reason: it can make some Android GATT stacks stall
     * in-flight ATT operations while it renegotiates. If that keeps
     * happening, try removing this call entirely and see whether the
     * original early-disconnect symptom returns unchanged; that isolates
     * whether this update helps, hurts, or is unrelated on the phone being
     * tested. */
    /* Match the connection parameters the .ino reference got "for free" from
     * arduino-esp32's Bluedroid BLEDevice/BLEServer, which requests these
     * right after connect: min interval 20 ms, max interval 40 ms, latency
     * 0, supervision timeout 4000 ms. NimBLE-Arduino's own defaults are
     * looser than that; requesting the same values here reproduces the
     * .ino's behavior instead. */
    s->updateConnParams(g_conn_id, 0x10, 0x20, 0, 400);
  }
  void onDisconnect(NimBLEServer *s, NimBLEConnInfo &connInfo, int reason) override {
    g_connected = false;
    g_event_subscribed = false;
    // g_no_overtaking_active = false;
    LOGI("=== DISCONNECTED (reason %d) after %s — advertising again ===",
         reason, auth_name(g_auth));
    NimBLEDevice::startAdvertising();
  }
  void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override {
    g_mtu = MTU;
    LOGD("MTU negotiated → %u (%u payload bytes per fragment)",
         (unsigned)g_mtu, (unsigned)frag_max_payload(g_mtu));
  }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
  /* NimBLE host task — copy the bytes to the queue and return. Nothing else. */
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &connInfo) override {
    in_msg_t msg;
    const NimBLEAttValue &val = c->getValue();
    size_t len = val.length();
    if (len == 0 || len > sizeof msg.data) return;
    msg.len = (uint16_t)len;
    memcpy(msg.data, val.data(), len);
    if (xQueueSend(g_in_queue, &msg, 0) != pdTRUE) LOGE("inbound queue full — frame dropped");
  }
};

class EventCallbacks : public NimBLECharacteristicCallbacks {
  /* §2.2/§9.4 — the central must subscribe to indications before HELLO.
   * subVal: 0 = none, 1 = notifications, 2 = indications (bitmask). */
  void onSubscribe(NimBLECharacteristic *c, NimBLEConnInfo &connInfo, uint16_t subValue) override {
    g_event_subscribed = (subValue & 0x02) != 0;
    LOGD("EVENT CCCD → subValue=0x%02x  indications_enabled=%d",
         (unsigned)subValue, (int)g_event_subscribed);
  }
  /* The indication confirmation §8 waits on.
   *
   * !! CUSTOMER DEBUG PATCH REVERTED — bug found 2026-08 !!
   * The previous patch assumed BLE_HS_EDONE (14) was an early/throwaway
   * marker and that a later onStatus(code=0) would follow with the real
   * confirmation, so it `return`ed WITHOUT giving g_ind_done for code==14.
   * Field logs from the customer's ESP-IDF/esp-nimble-cpp build show that
   * assumption was WRONG: code=14 arrives EXACTLY ONCE per indicate and is
   * the ONLY onStatus() callback ever delivered for a confirmed indication
   * on this stack/version — no second, "real" callback ever comes. Ignoring
   * it meant every single indicate — success or not — hit the 3 s semaphore
   * timeout in send_fragment() and invalidated the connection right after
   * EVT_HELLO_ACK, every time. (The .ino reference never hit this because
   * it uses the Arduino/Bluedroid BLE stack, which reports indicate
   * confirmation through a completely different code path — this quirk is
   * specific to esp-nimble-cpp's onStatus() semantics.)
   *
   * Treat BLE_HS_EDONE the same as code==0: it is this build's success
   * signal for "the central confirmed the indication". Any OTHER non-zero
   * code is still a genuine lost/failed indication. */
  void onStatus(NimBLECharacteristic *c, int code) override {
    LOGI("indication status code=%d", code);
#define WYN_BLE_HS_EDONE 14   /* host/ble_hs.h BLE_HS_EDONE, mirrored so this
                                * builds even if the macro isn't exposed by
                                * the NimBLE-Arduino wrapper headers.        */
    bool ok = (code == 0 || code == WYN_BLE_HS_EDONE);
    if (!ok) LOGE("indication failed (code %d)", code);
    g_ind_ok = ok;
    xSemaphoreGive(g_ind_done);
  }
};

/* ============================================================================
 *  WORKER TASK — owns every byte of protocol state
 *
 *  Inbound fragments arrive on the queue; the §12.5 reassembly timeout, the
 *  §13 deadlines, the claim button and the heartbeat all run on the receive
 *  timeout. One task, one writer, no locks.
 * ==========================================================================*/

static void connection_opened() {
  session_reset(); frag_reset();
  g_ecdsa_attempts  = 0;                         /* §13 — per connection    */
  g_auth            = AUTH_HELLO_PENDING;
  g_want_disconnect = false;
  arm_deadline(HELLO_DEADLINE_MS, "HELLO deadline expired");
  LOGI("state → HelloPending (deadline %d s)", HELLO_DEADLINE_MS / 1000);
}

static void worker_tick() {
  /* Pick up the connect/disconnect edges the BLE callbacks only flagged. */
  static bool was_connected = false;
  if (g_connected != was_connected) {
    was_connected = g_connected;
    if (g_connected) connection_opened(); else connection_closed();
  }

  claim_button_poll();
  frag_tick();

  /* §13 — the armed deadline bounds a state in which the peer is holding our
   * resources without having proved anything. */
  uint32_t now = millis();
  if (g_deadline && (int32_t)(now - g_deadline) >= 0) {
    const char *why = g_deadline_why;
    g_deadline = 0;
    invalidate_connection(why);
  }

  if (g_want_disconnect && g_connected) {
    g_want_disconnect = false;
    g_server->disconnect(g_conn_id);
  }

  /* No display, so the log has to say the device is alive and what it thinks
   * its situation is — every 5 s while idle, every 15 s once connected. */
  static uint32_t last_beat = 0;
  if (now - last_beat >= (g_connected ? 15000u : 5000u)) {
    last_beat = now;
    if (g_connected)
      LOGD("alive: %s  epoch=%d  tx=%llu rx=%llu  mtu=%u", auth_name(g_auth),
           (int)S.epoch, (unsigned long long)S.tx, (unsigned long long)S.rx, (unsigned)g_mtu);
    else if (claim_window_open())
      LOGD("alive: advertising, CLAIM WINDOW OPEN %lu s left, arm=%s",
           (unsigned long)((g_claim_deadline_ms - now) / 1000),
           g_claim_armed ? "ready" : "SPENT (press again)");
    else 
      LOGD("alive: advertising, %s, heap=%lu",
           g_bound ? "bound" : "unbound (claim window closed)",
           (unsigned long)esp_get_free_heap_size());
  }
}

static void worker_task(void *) {
  in_msg_t msg;
  for (;;) {
    if (xQueueReceive(g_in_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (g_connected && g_auth == AUTH_DISCONNECTED) worker_tick();  /* catch the edge first */
      frag_feed(msg.data, msg.len);
    }
    worker_tick();
  }
}

/* ============================================================================
 *  INIT  (replaces the .ino's setup(); there is no loop() here — the caller's
 *  own app_main() owns the rest of the system, and all protocol timing runs
 *  on wyn_worker, not on a superloop.)
 * ==========================================================================*/

/* §2.1 — DIS Serial Number is 12 UPPERCASE hex chars, no separators. The
 * central reads device_uid from here and may compare it as a string against
 * its provisioning registry, so the case is part of the contract, not
 * cosmetic. */
static void uid_hex(char out[13]) {
  static const char *h = "0123456789ABCDEF";
  for (int i = 0; i < 6; i++) { out[i*2] = h[DEVICE_UID[i] >> 4]; out[i*2+1] = h[DEVICE_UID[i] & 15]; }
  out[12] = 0;
}

void wyn_ble_peripheral_init(void) {
  printf("\n");
  printf("======================================================\n");
  printf(" WYN BLE peripheral — protocol 1.01 (spec v2.7.13.1)\n");
  printf(" trace level %d   build %s %s\n", WYN_TRACE, __DATE__, __TIME__);
  printf("======================================================\n");

  /* NVS must be up before ownership_load() opens the "wyn" namespace. Safe
   * to call even if the caller's own app_main() already initialized NVS for
   * something else (Wi-Fi, other storage) — nvs_flash_init() is idempotent
   * once a namespace-less init has already succeeded. */
  esp_err_t nvs_err = nvs_flash_init();
  if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    nvs_err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(nvs_err);

  pinMode(CLAIM_BUTTON_PIN, INPUT_PULLUP);
  if (!identity_load()) {
  printf("FATAL: device identity not provisioned — halting\n");
  while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }   // hoặc xử lý lỗi theo ý bạn
  }
  /* §3 — ownership must be readable BEFORE any HELLO can be answered. */
  ownership_load();

  /* Queue + worker task: the ONLY place protocol state is touched. 16 KB of
   * stack because BIND runs an ECDSA-P256 verify on top of the handshake. */
  g_in_queue = xQueueCreate(4, sizeof(in_msg_t));
  g_ind_done = xSemaphoreCreateBinary();
  xTaskCreate(worker_task, "wyn_worker", 16384, nullptr, 5, nullptr);

  NimBLEDevice::init(ADV_NAME);
  NimBLEDevice::setMTU(517);                        /* §12 — ask for the max */
  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(new ServerCallbacks());

  /* Service 1 — Device Information (§2). Plaintext, read during connection
   * setup so device_uid reaches the backend in parallel with key exchange. */
  NimBLEService *dis = g_server->createService(UUID_SERVICE_DIS);
  char uid[13]; uid_hex(uid);
  dis->createCharacteristic("2A25", NIMBLE_PROPERTY::READ)->setValue(uid);
  dis->createCharacteristic("2A24", NIMBLE_PROPERTY::READ)->setValue(DIS_MODEL);
  dis->createCharacteristic("2A26", NIMBLE_PROPERTY::READ)->setValue(DIS_FW_REV);
  dis->createCharacteristic("2A29", NIMBLE_PROPERTY::READ)->setValue(DIS_MANUFACTURER);
  dis->start();

  /* Service 2 — Command / Event (§2). CHAR_COMMAND is Write With Response
   * ONLY — do not offer NIMBLE_PROPERTY::WRITE_NR: the central needs the ATT
   * Write Response to know a fragment landed before sending the next, and an
   * uncertain write invalidates the session. */
  NimBLEService *cmd = g_server->createService(UUID_SERVICE_CMD);
  NimBLECharacteristic *chr_cmd = cmd->createCharacteristic(
      UUID_CHAR_COMMAND, NIMBLE_PROPERTY::WRITE);
  chr_cmd->setCallbacks(new CommandCallbacks());

  /* CHAR_EVENT: indications only. NimBLE creates and owns the CCCD for a
   * NOTIFY/INDICATE characteristic automatically — the central subscribing
   * to it is what onSubscribe() above tracks. It MUST subscribe before
   * sending CMD_HELLO, or the EVT_HELLO_ACK has nowhere to go. */
  g_chr_event = cmd->createCharacteristic(
      UUID_CHAR_EVENT, NIMBLE_PROPERTY::INDICATE);
  g_chr_event->setCallbacks(new EventCallbacks());
  cmd->start();

  /* Advertise the 128-bit command service + local name. Do NOT advertise
   * 0x180A — the app does not filter on it and it wastes payload space. */
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(UUID_SERVICE_CMD);
  adv->enableScanResponse(true);
  NimBLEDevice::startAdvertising();

  /* One boot-time check that would otherwise only surface as a mysterious
   * BIND rejection much later. */
  { uint8_t acc = 0;
    for (int i = 0; i < MAX_TRUST_KEYS; i++) for (int j = 0; j < 65; j++) acc |= MANUFACTURER_PUBKEYS[i][j];
    if (!acc) LOGE("MANUFACTURER_PUBKEYS is empty — BIND/UNBIND will always fail"); }

  LOGI("advertising as \"%s\"  uid=%s  service=%s", ADV_NAME, uid, UUID_SERVICE_CMD);
  if (g_storage_fault)
    LOGE("state: OWNERSHIP_STORAGE_FAULT — every HELLO is refused (§7.2); "
         "recovery needs the product service mechanism, not the BLE wire");
  else
    LOGI("state: %s, binding version %lu, generation %lu",
         g_bound ? "BOUND" : "UNBOUND", (unsigned long)g_binding_version,
         (unsigned long)g_generation);
  if (!g_bound && !g_storage_fault)
    LOGI("to pair: hold the claim button on GPIO%d for %d s, then run the app",
         CLAIM_BUTTON_PIN, CLAIM_HOLD_MS / 1000);
}