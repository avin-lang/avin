/*
 * The two calls `avin.crypto.password` makes into the vendored C, each with scalar and pointer
 * arguments only, so the Sysl declarations over them carry no struct layout.
 */

#ifndef AVIN_PASSWORD_SHIM_H
#define AVIN_PASSWORD_SHIM_H

#include <stddef.h>
#include <stdint.h>

/*
 * Argon2 of the given variant (0 Argon2d, 1 Argon2i, 2 Argon2id) and version (0x10 or 0x13) into
 * `out[0 .. out_len)`. Answers 0 or one of `argon2.h`'s negative error codes. Every input is read
 * and none is changed; the reference implementation's working memory is wiped before it is freed.
 */
int avin_password_argon2(uint32_t variant, uint32_t version, uint32_t memory_kib, uint32_t passes,
                         uint32_t lanes, const uint8_t *password, size_t password_len,
                         const uint8_t *salt, size_t salt_len, const uint8_t *secret,
                         size_t secret_len, const uint8_t *ad, size_t ad_len, uint8_t *out,
                         size_t out_len);

/* The reference implementation's text for one of its error codes. */
const char *avin_password_argon2_error(int code);

/*
 * The bcrypt string `setting` (`$2a$`, `$2b$` or `$2y$`, a two-digit cost and 22 characters of
 * salt; anything after them is ignored) computes for the key `key[0 .. key_len)`, written into
 * `out`, which holds at least 61 bytes, as 60 characters and a NUL. bcrypt reads a key up to its
 * first NUL byte and at most 72 bytes of it. Answers 0, or -1 where `setting` is not one it reads.
 */
int avin_password_bcrypt(const uint8_t *key, size_t key_len, const uint8_t *setting,
                         size_t setting_len, uint8_t *out);

#endif
