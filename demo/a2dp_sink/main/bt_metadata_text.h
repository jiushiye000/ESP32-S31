#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Copy bounded, valid UTF-8 without cutting a codepoint at the byte limit.
 * Bluetooth metadata supplies a length, not necessarily a terminating NUL. */
static inline void bt_metadata_text_copy(char *dst, size_t capacity, const uint8_t *src, size_t length)
{
    if (capacity == 0) return;
    size_t pos = 0;
    while (src != NULL && pos < length && src[pos] != 0) {
        unsigned first = src[pos];
        size_t bytes = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2 :
                       first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (bytes == 0 || bytes > length - pos || bytes >= capacity - pos) break;
        uint32_t codepoint = first & (bytes == 1 ? 0x7f : (1U << (7 - bytes)) - 1);
        bool valid = true;
        for (size_t i = 1; i < bytes; ++i) {
            if ((src[pos + i] & 0xc0) != 0x80) { valid = false; break; }
            codepoint = (codepoint << 6) | (src[pos + i] & 0x3f);
        }
        if (!valid || (bytes == 2 && codepoint < 0x80) || (bytes == 3 && codepoint < 0x800) ||
            (bytes == 4 && codepoint < 0x10000) || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) break;
        pos += bytes;
    }
    if (pos != 0) memcpy(dst, src, pos);
    dst[pos] = '\0';
}
