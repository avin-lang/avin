#include <limits.h>
#include <stdlib.h>

#include "shim.h"

z_stream *avin_zcodec_open(int compress, int level, int window_bits, int *status) {
    z_stream *s = calloc(1, sizeof(z_stream));
    if (s == NULL) {
        *status = Z_MEM_ERROR;
        return NULL;
    }
    int r = compress
        ? deflateInit2(s, level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY)
        : inflateInit2(s, window_bits);
    *status = r;
    if (r != Z_OK) {
        free(s);
        return NULL;
    }
    return s;
}

int avin_zcodec_step(z_stream *s, int compress, const unsigned char *in, size_t in_len,
                     unsigned char *out, size_t out_len, int flush, size_t *consumed,
                     size_t *produced) {
    /* zlib counts in `uInt`; a longer buffer is offered in part, and the caller offers the rest. */
    uInt in_avail = in_len > UINT_MAX ? UINT_MAX : (uInt)in_len;
    uInt out_avail = out_len > UINT_MAX ? UINT_MAX : (uInt)out_len;
    s->next_in = (Bytef *)in;
    s->avail_in = in_avail;
    s->next_out = out;
    s->avail_out = out_avail;
    int r = compress ? deflate(s, flush) : inflate(s, flush);
    *consumed = in_avail - s->avail_in;
    *produced = out_avail - s->avail_out;
    s->next_in = Z_NULL;
    s->avail_in = 0;
    s->next_out = Z_NULL;
    s->avail_out = 0;
    return r;
}

int avin_zcodec_reset(z_stream *s, int compress) {
    return compress ? deflateReset(s) : inflateReset(s);
}

const char *avin_zcodec_message(z_stream *s) {
    return s->msg;
}

void avin_zcodec_close(z_stream *s, int compress) {
    if (compress) {
        deflateEnd(s);
    } else {
        inflateEnd(s);
    }
    free(s);
}
