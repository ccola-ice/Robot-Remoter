#include <assert.h>
#include <string.h>
#include "tjpgd.c"

int main(void)
{
    LONG value, input[64];
    BYTE output[64];
    unsigned i;
    assert(jd_checked_multiply(2147483647L, 1L, &value) && value == 2147483647L);
    assert(jd_checked_multiply(-2147483647L - 1L, 1L, &value) && value == -2147483647L - 1L);
    assert(!jd_checked_multiply(2147483647L, 2L, &value));
    assert(!jd_checked_multiply(-2147483647L - 1L, -1L, &value));
    assert(!jd_checked_multiply(2047L, 255L * 8192L, &value));
    assert(!jd_checked_multiply(-2047L, 255L * 8192L, &value));
    assert(jd_checked_multiply(0L, 2147483647L, &value) && value == 0L);
    memset(input, 0, sizeof(input));
    assert(block_idct(input, output) == JDR_OK);
    for(i = 0U; i < 64U; i++) assert(output[i] == 128U);
    memset(input, 0, sizeof(input));
    input[0] = 8388608L;
    assert(block_idct(input, output) == JDR_FMT1);
    input[0] = -8388609L;
    assert(block_idct(input, output) == JDR_FMT1);
    input[0] = 8388607L;
    assert(block_idct(input, output) == JDR_OK);
    memset(input, 0, sizeof(input));
    input[1] = 2000000L;
    assert(block_idct(input, output) == JDR_FMT1);
    return 0;
}
