/*
 * lmp_extractor.c — Extracts files from Dawnstar .lmp archives
 *
 * .lmp format (confirmed from hex analysis):
 *   Archive = Entry*
 *   Entry   = '-' filename '-' offset_be32 size_be16
 *
 *   - filename: ASCII string delimited by '-' characters
 *   - offset:   4-byte big-endian absolute offset into the .lmp file
 *   - size:     2-byte big-endian file size in bytes
 *
 * The header/directory entries are packed sequentially at the start of the file,
 * followed by the raw file data. Each entry's offset points into the data region.
 *
 * Usage: lmp_extractor <archive.lmp> [output_dir]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <direct.h>  /* _mkdir on Windows */

typedef struct {
    char name[256];
    uint32_t offset;
    uint16_t size;
} LmpEntry;

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: lmp_extractor <archive.lmp> [output_dir]\n");
        return 1;
    }

    const char *archive_path = argv[1];
    const char *output_dir = argc >= 3 ? argv[2] : ".";

    /* Read entire archive */
    FILE *f = fopen(archive_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", archive_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = (uint8_t *)malloc(file_size);
    fread(data, 1, file_size, f);
    fclose(f);

    /* Parse directory entries */
    LmpEntry entries[256];
    int num_entries = 0;
    size_t pos = 0;

    while (pos < (size_t)file_size && num_entries < 256) {
        /* Entries start with '-' */
        if (data[pos] != '-') break;
        pos++; /* skip leading '-' */

        /* Read filename until next '-' */
        size_t name_start = pos;
        while (pos < (size_t)file_size && data[pos] != '-') pos++;
        if (pos >= (size_t)file_size) break;

        size_t name_len = pos - name_start;
        if (name_len >= sizeof(entries[0].name)) name_len = sizeof(entries[0].name) - 1;
        memcpy(entries[num_entries].name, data + name_start, name_len);
        entries[num_entries].name[name_len] = '\0';

        pos++; /* skip trailing '-' */

        /* Read 4-byte offset + 2-byte size */
        if (pos + 6 > (size_t)file_size) break;
        entries[num_entries].offset = read_be32(data + pos);
        entries[num_entries].size   = read_be16(data + pos + 4);
        pos += 6;

        num_entries++;
    }

    printf("Archive: %s (%ld bytes)\n", archive_path, file_size);
    printf("Found %d entries:\n\n", num_entries);

    /* Create output directory */
    _mkdir(output_dir);

    /* Extract each entry */
    int errors = 0;
    for (int i = 0; i < num_entries; i++) {
        LmpEntry *e = &entries[i];
        printf("  %-30s offset=%-6u size=%-6u", e->name, e->offset, e->size);

        /* Validate */
        if (e->offset + e->size > (uint32_t)file_size) {
            printf(" [ERROR: extends beyond file]\n");
            errors++;
            continue;
        }

        /* Write file */
        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, e->name);

        FILE *out = fopen(out_path, "wb");
        if (!out) {
            printf(" [ERROR: cannot create output file]\n");
            errors++;
            continue;
        }
        fwrite(data + e->offset, 1, e->size, out);
        fclose(out);
        printf(" -> %s\n", out_path);
    }

    printf("\nExtracted %d/%d files (%d errors)\n",
           num_entries - errors, num_entries, errors);

    free(data);
    return errors > 0 ? 1 : 0;
}
