#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../5_ModuleDrivers/debug_text.h"

static unsigned length;
static uint8_t output[256];
static void capture(uint8_t byte)
{
    if(length == sizeof(output)) exit(2);
    output[length++] = byte;
}
static void check(const char *input, const char *expected)
{
    DebugTextState state = {0U, 0U};
    length = 0U;
    while(*input) DebugText_Put(&state, (uint8_t)*input++, capture);
    if(length != strlen(expected) || memcmp(output, expected, length) != 0)
    {
        fputs("FAIL: debug UART encoding/line ending\n", stderr);
        exit(1);
    }
}
int main(void)
{
    check("[GTP] raw=(799,0)\nnext\r\n", "[GTP] raw=(799,0)\r\nnext\r\n");
    check("\xC4\xE3\xBA\xC3\n", "\xE4\xBD\xA0\xE5\xA5\xBD\r\n");
    check("\xD6\xD0\xCE\xC4\r\n", "\xE4\xB8\xAD\xE6\x96\x87\r\n");
    check("\xC4\nOK\n", "\xEF\xBF\xBD\r\nOK\r\n");
    check("\xFF\x80", "\xEF\xBF\xBD\xE2\x82\xAC");
    check("\r\r\n", "\r\r\n");
    puts("PASS: production GBK table to UTF-8, ASCII, CRLF, malformed input");
    return 0;
}
