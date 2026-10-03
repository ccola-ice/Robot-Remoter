/* 穷尽 16 位字符域，保留独立旧实现对照入口及稳定的小端结果文件。 */
#include "ff.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef CC936_COMPARE_REFERENCE
WCHAR old_ff_convert(WCHAR chr, UINT dir);
WCHAR old_ff_wtoupper(WCHAR chr);

static volatile uint32_t benchmark_result;

static double benchmark(WCHAR (*convert)(WCHAR, UINT), WCHAR code, UINT dir)
{
    const unsigned iterations = 2000000U;
    unsigned i;
    uint32_t sum = 0U;
    clock_t start = clock();
    for(i = 0U; i < iterations; ++i) sum += convert(code, dir);
    benchmark_result = sum;
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

static void benchmark_reference(void)
{
    static const struct {const char *name; WCHAR code; UINT dir;} cases[] = {
        {"OEM missing (old search reaches 16 iterations)", 0xFEFE, 1U},
        {"OEM first valid pair", 0x8140, 1U},
        {"Unicode missing, populated page", 0xFFFF, 0U},
        {"Unicode missing, shared empty page", 0xD800, 0U},
        {"Unicode Chinese", 0x4E2D, 0U},
        {"Unicode Euro single-byte mapping", 0x20AC, 0U},
        {"ASCII fast path", 'A', 0U},
    };
    unsigned i;
    for(i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        double original = benchmark(old_ff_convert, cases[i].code, cases[i].dir);
        double current = benchmark(ff_convert, cases[i].code, cases[i].dir);
        printf("Host benchmark, 2000000 calls, %s: old %.3f s / current %.3f s\n",
               cases[i].name, original, current);
    }
}
#endif

int main(int argc, char **argv)
{
    uint32_t code;
    FILE *output;
    if(argc != 2 || sizeof(WCHAR) != 2U || sizeof(WORD) != 2U) return 1;
    output = fopen(argv[1], "wb");
    if(output == NULL) return 2;
    for(code = 0U; code <= 0xffffU; ++code) {
        WCHAR values[3];
        unsigned operation;
        values[0] = ff_convert((WCHAR)code, 0U);
        values[1] = ff_convert((WCHAR)code, 1U);
        values[2] = ff_wtoupper((WCHAR)code);
        /* 原接口所有非零 dir 均按 OEM -> Unicode 处理。 */
        if(ff_convert((WCHAR)code, 2U) != values[1] ||
           ff_convert((WCHAR)code, (UINT)-1) != values[1]) return 3;
#ifdef CC936_COMPARE_REFERENCE
        if(values[0] != old_ff_convert((WCHAR)code, 0U) ||
           values[1] != old_ff_convert((WCHAR)code, 1U) ||
           values[2] != old_ff_wtoupper((WCHAR)code)) {
            fprintf(stderr, "CP936 reference mismatch: 0x%04lX\n", (unsigned long)code);
            fclose(output);
            return 4;
        }
#endif
        for(operation = 0U; operation < 3U; ++operation) {
            if(fputc(values[operation] & 0xffU, output) == EOF ||
               fputc(values[operation] >> 8, output) == EOF) {
                fclose(output);
                return 5;
            }
        }
    }
    if(fclose(output) != 0) return 6;
    puts("CP936: 131072 conversions, 65536 case mappings, 131072 nonzero-direction aliases passed.");
#ifdef CC936_COMPARE_REFERENCE
    benchmark_reference();
#endif
    return 0;
}
