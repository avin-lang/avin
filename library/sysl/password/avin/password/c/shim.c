/*
 * The two calls `avin.crypto.password` makes into the vendored C. See shim.h.
 */

#include <string.h>

#include "argon2.h"
#include "crypt_blowfish.h"
#include "shim.h"

/* The most threads one hash computes its lanes on; more lanes than this share them. */
#define AVIN_PASSWORD_MAX_THREADS 16u

int avin_password_argon2(uint32_t variant, uint32_t version, uint32_t memory_kib, uint32_t passes,
                         uint32_t lanes, const uint8_t *password, size_t password_len,
                         const uint8_t *salt, size_t salt_len, const uint8_t *secret,
                         size_t secret_len, const uint8_t *ad, size_t ad_len, uint8_t *out,
                         size_t out_len) {
    argon2_context ctx;
    if (password_len > ARGON2_MAX_PWD_LENGTH) return ARGON2_PWD_TOO_LONG;
    if (salt_len > ARGON2_MAX_SALT_LENGTH) return ARGON2_SALT_TOO_LONG;
    if (secret_len > ARGON2_MAX_SECRET) return ARGON2_SECRET_TOO_LONG;
    if (ad_len > ARGON2_MAX_AD_LENGTH) return ARGON2_AD_TOO_LONG;
    if (out_len > ARGON2_MAX_OUTLEN) return ARGON2_OUTPUT_TOO_LONG;
    if (variant > (uint32_t)Argon2_id) return ARGON2_INCORRECT_TYPE;
    memset(&ctx, 0, sizeof ctx);
    ctx.out = out;
    ctx.outlen = (uint32_t)out_len;
    /* The reference implementation writes through `pwd` only under ARGON2_FLAG_CLEAR_PASSWORD,
       which is not set, and never through `salt`, `secret` or `ad`. */
    ctx.pwd = password_len == 0 ? NULL : (uint8_t *)password;
    ctx.pwdlen = (uint32_t)password_len;
    ctx.salt = salt_len == 0 ? NULL : (uint8_t *)salt;
    ctx.saltlen = (uint32_t)salt_len;
    ctx.secret = secret_len == 0 ? NULL : (uint8_t *)secret;
    ctx.secretlen = (uint32_t)secret_len;
    ctx.ad = ad_len == 0 ? NULL : (uint8_t *)ad;
    ctx.adlen = (uint32_t)ad_len;
    ctx.t_cost = passes;
    ctx.m_cost = memory_kib;
    ctx.lanes = lanes;
    ctx.threads = lanes < AVIN_PASSWORD_MAX_THREADS ? lanes : AVIN_PASSWORD_MAX_THREADS;
    ctx.version = version;
    ctx.allocate_cbk = NULL;
    ctx.free_cbk = NULL;
    ctx.flags = ARGON2_DEFAULT_FLAGS;
    return argon2_ctx(&ctx, (argon2_type)variant);
}

const char *avin_password_argon2_error(int code) { return argon2_error_message(code); }

int avin_password_bcrypt(const uint8_t *key, size_t key_len, const uint8_t *setting,
                         size_t setting_len, uint8_t *out) {
    /* bcrypt reads its key as a C string and at most 72 bytes of it, so a copy of the first 72
       bytes, ended by a NUL, is the same key; the setting it reads is 29 characters. */
    char key_text[73];
    char setting_text[64];
    size_t k = key_len < 72 ? key_len : 72;
    size_t s = setting_len < 63 ? setting_len : 63;
    char *made;
    if (k > 0) memcpy(key_text, key, k);
    key_text[k] = '\0';
    if (s > 0) memcpy(setting_text, setting, s);
    setting_text[s] = '\0';
    made = _crypt_blowfish_rn(key_text, setting_text, (char *)out, 61);
    /* The copy of the key is wiped through a volatile pointer, which the compiler keeps. */
    {
        volatile char *p = key_text;
        size_t i;
        for (i = 0; i < sizeof key_text; i++) p[i] = 0;
    }
    return made == NULL ? -1 : 0;
}
