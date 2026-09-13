/*
 * cus_converter.c — Converts .cus (Nokia 4444 ARGB palettized) images to PNG
 *
 * .cus format (decoded from class 'g' in Stormhold):
 *   Offset  Size   Field
 *   0       4      Width  (big-endian int32)
 *   4       4      Height (big-endian int32)
 *   8       1      Transparency flag (0=opaque, 1=has transparency)
 *   9       2      Transparent color key (big-endian int16, Nokia 4444 ARGB)
 *   11      1      Palette size (number of colors, max 255)
 *   12      2*N    Palette entries (big-endian int16 each, Nokia 4444 ARGB)
 *   12+2*N  W*H    Pixel data (1 byte per pixel, index into palette)
 *
 * Nokia 4444 ARGB format: 0xARGB where each channel is 4 bits.
 * Transparency: if pixel's palette index matches the transparent color key index,
 * alpha bits are cleared; otherwise alpha is set to 0xF.
 *
 * Usage: cus_converter <input.cus> [output.png]
 *        cus_converter --batch <dir>    (converts all .cus files in directory)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "3p/stb_image_write.h"

/* Read big-endian int32 from buffer */
static int32_t read_be32(const uint8_t *p) {
    return (int32_t)((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

/* Read big-endian int16 from buffer */
static int16_t read_be16(const uint8_t *p) {
    return (int16_t)((p[0] << 8) | p[1]);
}

/* Convert Nokia 4444 ARGB to standard 8888 RGBA */
static uint32_t nokia4444_to_rgba(uint16_t c, int transparent) {
    /* Nokia 4444: bits 15-12=A, 11-8=R, 7-4=G, 3-0=B */
    uint8_t a = transparent ? 0x00 : 0xFF;
    uint8_t r = ((c >> 8) & 0xF);
    uint8_t g = ((c >> 4) & 0xF);
    uint8_t b = ((c >> 0) & 0xF);

    /* Expand 4-bit to 8-bit: 0xN -> 0xNN */
    r = (r << 4) | r;
    g = (g << 4) | g;
    b = (b << 4) | b;

    /* RGBA byte order for stb_image_write */
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

static int convert_cus(const char *input_path, const char *output_path) {
    FILE *f = fopen(input_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", input_path);
        return 1;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = (uint8_t *)malloc(file_size);
    if (!data) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(f);
        return 1;
    }
    fread(data, 1, file_size, f);
    fclose(f);

    /* Parse header */
    if (file_size < 12) {
        fprintf(stderr, "Error: file too small (%ld bytes)\n", file_size);
        free(data);
        return 1;
    }

    int32_t width  = read_be32(data + 0);
    int32_t height = read_be32(data + 4);
    uint8_t has_transparency = data[8];
    int16_t transparent_color = read_be16(data + 9);
    uint8_t palette_size = data[11];

    printf("  %s: %dx%d, %d colors, transparency=%s",
           input_path, width, height, palette_size,
           has_transparency ? "yes" : "no");
    if (has_transparency) {
        printf(" (key=0x%04X)", (uint16_t)transparent_color);
    }
    printf("\n");

    /* Validate */
    long expected_size = 12 + (2 * palette_size) + (width * height);
    if (file_size < expected_size) {
        fprintf(stderr, "Error: file too small (expected %ld bytes, got %ld)\n",
                expected_size, file_size);
        free(data);
        return 1;
    }

    /* Read palette */
    uint16_t palette[256];
    int transparent_index = -1;
    const uint8_t *pal_ptr = data + 12;

    for (int i = 0; i < palette_size; i++) {
        palette[i] = (uint16_t)read_be16(pal_ptr + i * 2);
        if (has_transparency && transparent_index < 0 &&
            palette[i] == (uint16_t)transparent_color) {
            transparent_index = i;
        }
    }

    /* Decode pixels */
    const uint8_t *pixel_data = pal_ptr + (2 * palette_size);
    uint32_t *rgba = (uint32_t *)malloc(width * height * 4);
    if (!rgba) {
        fprintf(stderr, "Error: out of memory for pixel buffer\n");
        free(data);
        return 1;
    }

    for (int i = 0; i < width * height; i++) {
        uint8_t idx = pixel_data[i];
        int is_transparent = (has_transparency && idx == transparent_index);
        rgba[i] = nokia4444_to_rgba(palette[idx], is_transparent);
    }

    /* Write PNG */
    int result = stbi_write_png(output_path, width, height, 4, rgba, width * 4);
    if (!result) {
        fprintf(stderr, "Error: failed to write PNG '%s'\n", output_path);
        free(rgba);
        free(data);
        return 1;
    }

    printf("  -> %s\n", output_path);

    free(rgba);
    free(data);
    return 0;
}

/* Generate output path by replacing .cus extension with .png */
static void make_output_path(const char *input, char *output, size_t output_size) {
    strncpy(output, input, output_size - 1);
    output[output_size - 1] = '\0';
    char *dot = strrchr(output, '.');
    if (dot && (strcmp(dot, ".cus") == 0 || strcmp(dot, ".CUS") == 0)) {
        strcpy(dot, ".png");
    } else {
        strncat(output, ".png", output_size - strlen(output) - 1);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: cus_converter <input.cus> [output.png]\n");
        printf("       cus_converter <file1.cus> <file2.cus> ...\n");
        return 1;
    }

    int errors = 0;

    for (int i = 1; i < argc; i++) {
        const char *input = argv[i];
        char output[512];

        /* If next arg doesn't end in .cus, treat it as explicit output path */
        if (i + 1 < argc && strstr(argv[i + 1], ".cus") == NULL &&
            strstr(argv[i + 1], ".CUS") == NULL) {
            strncpy(output, argv[i + 1], sizeof(output) - 1);
            output[sizeof(output) - 1] = '\0';
            i++; /* consume the output arg */
        } else {
            make_output_path(input, output, sizeof(output));
        }

        errors += convert_cus(input, output);
    }

    return errors > 0 ? 1 : 0;
}
