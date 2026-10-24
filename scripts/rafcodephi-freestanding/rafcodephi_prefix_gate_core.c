/*
 * RAFCODEPHI authorial freestanding prefix sentinel, v1.
 * Pure borrowed-buffer operation: no libc, allocation, I/O or syscalls.
 * A bounded traversal is necessary because the caller supplies arbitrary length.
 * Return 0 = clean readable span, 1 = forbidden prefix, 2 = invalid NULL.
 * Every nonzero result must block a caller. NULL is never clean, even at n=0.
 * A non-NULL empty span is valid; caller owns readability of all n bytes.
 * A PASS here is not
 * evidence of package-manager runtime or signature authenticity.
 */
typedef __SIZE_TYPE__ raf_size_t;

unsigned int rafcodephi_legacy_prefix_in_span(
    const unsigned char *data, raf_size_t n
) {
    static const unsigned char legacy[] = "/data/data/com.termux/";
    const raf_size_t width = sizeof(legacy) - 1u;
    raf_size_t offset = 0u;
    unsigned int result = 0u;
    if (!data) return 2u;
    if (n < width) return 0u;
    while (offset <= n - width) {
        raf_size_t column = 0u;
        unsigned int delta = 0u;
        while (column < width) {
            delta |= ((unsigned int)data[offset + column]) ^
                     ((unsigned int)legacy[column]);
            column += 1u;
        }
        result |= (unsigned int)(delta == 0u);
        offset += 1u;
    }
    return result;
}
