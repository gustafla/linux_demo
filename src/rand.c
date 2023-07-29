/*  Adapted from the code included on Sebastiano Vigna's website */

#include <stdint.h>

uint64_t rol64(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

struct xoshiro256ss_state {
    uint64_t s[4];
} state = {{123, 450435, 9, ~0}};

uint64_t rand_xoshiro(void) {
    uint64_t *s = state.s;
    uint64_t const result = rol64(s[1] * 5, 7) * 9;
    uint64_t const t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = rol64(s[3], 45);

    return result;
}
