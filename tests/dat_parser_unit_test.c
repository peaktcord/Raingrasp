#define main dat_parser_cli_main
#include "../tools/dat_parser.c"
#undef main

int main(void) {
    uint8_t bytes[] = {
        0x12, 0x34,
        0x89, 0xab, 0xcd, 0xef,
        0xff,
        0x00,
    };
    Reader reader = {bytes, sizeof(bytes), 0};

    if ((uint16_t)read_short(&reader) != UINT16_C(0x1234)) return 1;
    if ((uint32_t)read_int(&reader) != UINT32_C(0x89abcdef)) return 2;
    if (read_byte(&reader) != -1) return 3;
    if (read_boolean(&reader) != 0) return 4;
    if (reader_remaining(&reader) != 0) return 5;

    return 0;
}
