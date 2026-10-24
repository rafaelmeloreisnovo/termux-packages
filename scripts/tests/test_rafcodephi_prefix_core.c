/* Hosted sanitizer harness; the separately linked sentinel remains freestanding. */
#include <stddef.h>
unsigned int rafcodephi_legacy_prefix_in_span(const unsigned char *, size_t);

int main(void) {
    const unsigned char legacy[] = "/data/data/com.termux/";
    const unsigned char valid[] = "/data/data/com.termux.rafacodephi/files/usr";
    const unsigned char empty[] = "";
    const size_t null_lengths[] = {0u, 1u, sizeof(legacy) - 1u, (size_t)-1};
    for (size_t i = 0; i < sizeof(null_lengths) / sizeof(null_lengths[0]); ++i)
        if (rafcodephi_legacy_prefix_in_span(NULL, null_lengths[i]) != 2u) return 1;
    for (size_t n = 0; n < sizeof(legacy) - 1u; ++n)
        if (rafcodephi_legacy_prefix_in_span(legacy, n) != 0u) return 2;
    if (rafcodephi_legacy_prefix_in_span(legacy, sizeof(legacy) - 1u) != 1u) return 3;
    if (rafcodephi_legacy_prefix_in_span(valid, sizeof(valid) - 1u) != 0u) return 4;
    if (rafcodephi_legacy_prefix_in_span(empty, 0u) != 0u) return 5;
    return 0;
}
