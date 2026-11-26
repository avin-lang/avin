#ifndef AVIN_ZCODEC_SHIM_H
#define AVIN_ZCODEC_SHIM_H

#include <stddef.h>
#include <zlib.h>

/* zlib's stream, driven a step at a time between two buffers the caller hands over.
 *
 * A `z_stream` is a struct whose fields the caller sets before each call and reads after it, and
 * whose initializers, `deflateInit2` and `inflateInit2`, are macros. Both are C-only shapes, so they
 * live here: the stream is allocated and initialized in one call, and each step sets its buffers,
 * calls `deflate` or `inflate`, and reports how far it got through two out parameters. */

/* A stream to compress (`compress` non-zero) at `level` or to decompress, its wrapper chosen by
 * `window_bits` as zlib's initializers read it. NULL where it could not be made, with zlib's code in
 * `*status`. */
z_stream *avin_zcodec_open(int compress, int level, int window_bits, int *status);

/* One call of `deflate` or `inflate` from `in` into `out` with `flush`, answering zlib's code and how
 * many bytes of each it took and wrote. */
int avin_zcodec_step(z_stream *s, int compress, const unsigned char *in, size_t in_len,
                     unsigned char *out, size_t out_len, int flush, size_t *consumed,
                     size_t *produced);

/* The stream made ready for a new one, its allocation kept. */
int avin_zcodec_reset(z_stream *s, int compress);

/* zlib's sentence about the last failure, or NULL. */
const char *avin_zcodec_message(z_stream *s);

/* The stream's memory given back. */
void avin_zcodec_close(z_stream *s, int compress);

#endif
