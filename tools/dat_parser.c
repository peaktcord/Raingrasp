/*
 * dat_parser.c — Dumps .dat data files from TES Travels: Stormhold & Dawnstar
 *
 * All .dat files use Java DataInputStream format (big-endian):
 *   readShort()   = 2 bytes big-endian signed int16
 *   readInt()     = 4 bytes big-endian signed int32
 *   readByte()    = 1 byte signed int8
 *   readBoolean() = 1 byte (0=false, nonzero=true)
 *   readLong()    = 8 bytes big-endian signed int64
 *   readUTF()     = 2-byte length prefix (big-endian uint16) + UTF-8 string bytes
 *
 * Supported files (from decompiled Stormhold and Dawnstar source):
 *   itemsin.dat            — item definitions (class a / ItemDatabase)
 *   spellsin.dat           — spell definitions (class b / SpellDatabase)
 *   monstersin.dat         — monster type stats (class d / Monster)
 *   droppeditemsin.dat     — loot drop tables (class a / ItemDatabase)
 *   npcstrings.dat         — NPC dialogue strings (class k / NPCSystem)
 *   monsterfilenamesin.dat — monster sprite filenames (Game)
 *   charin.dat             — character templates / class stats (class j / Player)
 *   dungnamesin.dat        — dungeon / floor names (class i / Dungeon)
 *   geomin.dat             — dungeon geometry / entrance-exit data (Game / class c)
 *   helptext.dat           — help topics and dialogue (Dawnstar Game)
 *
 * Usage: dat_parser <file1.dat> [file2.dat ...]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   pos;
} Reader;

static int reader_open(Reader *r, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    r->size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    r->data = (uint8_t *)malloc(r->size);
    if (!r->data) { fclose(f); return 0; }
    fread(r->data, 1, r->size, f);
    fclose(f);
    r->pos = 0;
    return 1;
}

static void reader_close(Reader *r) { free(r->data); r->data = NULL; }

static int reader_remaining(Reader *r) { return (int)(r->size - r->pos); }

static int8_t read_byte(Reader *r) {
    if (r->pos >= r->size) { fprintf(stderr, "read_byte: EOF\n"); return 0; }
    return (int8_t)r->data[r->pos++];
}

static int16_t read_short(Reader *r) {
    if (r->pos + 2 > r->size) { fprintf(stderr, "read_short: EOF\n"); return 0; }
    int16_t v = (int16_t)((r->data[r->pos] << 8) | r->data[r->pos + 1]);
    r->pos += 2;
    return v;
}

static int32_t read_int(Reader *r) {
    if (r->pos + 4 > r->size) { fprintf(stderr, "read_int: EOF\n"); return 0; }
    int32_t v = (int32_t)((r->data[r->pos] << 24) | (r->data[r->pos+1] << 16) |
                           (r->data[r->pos+2] << 8) | r->data[r->pos+3]);
    r->pos += 4;
    return v;
}

static int64_t read_long(Reader *r) {
    int64_t hi = (int64_t)(uint32_t)read_int(r);
    int64_t lo = (int64_t)(uint32_t)read_int(r);
    return (hi << 32) | lo;
}

static int read_boolean(Reader *r) {
    return read_byte(r) != 0;
}

/* Java modified UTF-8: 2-byte length prefix + string bytes */
static char *read_utf(Reader *r) {
    if (r->pos + 2 > r->size) { fprintf(stderr, "read_utf: EOF\n"); return _strdup(""); }
    uint16_t len = (uint16_t)((r->data[r->pos] << 8) | r->data[r->pos + 1]);
    r->pos += 2;
    if (r->pos + len > r->size) { fprintf(stderr, "read_utf: truncated string\n"); len = (uint16_t)(r->size - r->pos); }
    char *s = (char *)malloc(len + 1);
    memcpy(s, r->data + r->pos, len);
    s[len] = '\0';
    r->pos += len;
    return s;
}

static char **read_utf_array(Reader *r, int16_t *out_count) {
    int16_t count = read_short(r);
    *out_count = count;
    if (count <= 0) return NULL;
    char **arr = (char **)malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) {
        arr[i] = read_utf(r);
    }
    return arr;
}

static void free_utf_array(char **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) free(arr[i]);
    free(arr);
}

/* ========== File-specific parsers ========== */

static void parse_itemsin(Reader *r) {
    int16_t numCategories = read_short(r);
    printf("=== itemsin.dat ===\n");
    printf("Item categories: %d\n", numCategories);
    char **categories = (char **)malloc(numCategories * sizeof(char *));
    for (int i = 0; i < numCategories; i++) {
        categories[i] = read_utf(r);
        printf("  Category[%d]: %s\n", i, categories[i]);
    }

    int16_t numItems = read_short(r);
    printf("\nItems: %d\n", numItems);

    char **names = (char **)malloc(numItems * sizeof(char *));
    for (int i = 0; i < numItems; i++) names[i] = read_utf(r);

    int8_t *types    = (int8_t *)malloc(numItems);
    int8_t *subtypes = (int8_t *)malloc(numItems);
    int8_t *weights  = (int8_t *)malloc(numItems);
    int16_t *values  = (int16_t *)malloc(numItems * 2);
    int16_t *powers  = (int16_t *)malloc(numItems * 2);
    int8_t *slots    = (int8_t *)malloc(numItems);

    for (int i = 0; i < numItems; i++) types[i]    = read_byte(r);
    for (int i = 0; i < numItems; i++) subtypes[i] = read_byte(r);
    for (int i = 0; i < numItems; i++) weights[i]  = read_byte(r);
    for (int i = 0; i < numItems; i++) values[i]   = read_short(r);
    for (int i = 0; i < numItems; i++) powers[i]   = read_short(r);
    for (int i = 0; i < numItems; i++) slots[i]    = read_byte(r);

    printf("\n  %-4s %-30s %-6s %-6s %-6s %-6s %-6s %-6s\n",
           "ID", "Name", "Type", "Sub", "Wt", "Value", "Power", "Slot");
    printf("  %-4s %-30s %-6s %-6s %-6s %-6s %-6s %-6s\n",
           "----", "------------------------------", "------", "------", "------", "------", "------", "------");
    for (int i = 0; i < numItems; i++) {
        printf("  %-4d %-30s %-6d %-6d %-6d %-6d %-6d %-6d\n",
               i + 1, names[i], types[i], subtypes[i], weights[i],
               values[i], powers[i], slots[i]);
    }

    for (int i = 0; i < numItems; i++) free(names[i]);
    for (int i = 0; i < numCategories; i++) free(categories[i]);
    free(names); free(categories);
    free(types); free(subtypes); free(weights); free(values); free(powers); free(slots);
}

static void parse_spellsin(Reader *r) {
    int16_t numSpells = read_short(r);
    printf("=== spellsin.dat ===\n");
    printf("Spells: %d\n\n", numSpells);

    char **names = (char **)malloc(numSpells * sizeof(char *));
    for (int i = 0; i < numSpells; i++) names[i] = read_utf(r);

    int8_t *manaCost  = (int8_t *)malloc(numSpells);
    int8_t *minDmg    = (int8_t *)malloc(numSpells);
    int8_t *maxDmg    = (int8_t *)malloc(numSpells);
    int8_t *spellType = (int8_t *)malloc(numSpells);
    int8_t *level     = (int8_t *)malloc(numSpells);
    int8_t *element   = (int8_t *)malloc(numSpells);

    for (int i = 0; i < numSpells; i++) manaCost[i]  = read_byte(r);
    for (int i = 0; i < numSpells; i++) minDmg[i]    = read_byte(r);
    for (int i = 0; i < numSpells; i++) maxDmg[i]    = read_byte(r);
    for (int i = 0; i < numSpells; i++) spellType[i] = read_byte(r);
    for (int i = 0; i < numSpells; i++) level[i]     = read_byte(r);
    for (int i = 0; i < numSpells; i++) element[i]   = read_byte(r);

    char **descs = (char **)malloc(numSpells * sizeof(char *));
    for (int i = 0; i < numSpells; i++) descs[i] = read_utf(r);

    printf("  %-4s %-20s %-6s %-6s %-6s %-6s %-6s %-6s %s\n",
           "ID", "Name", "Mana", "MinDmg", "MaxDmg", "Type", "Lvl", "Elem", "Description");
    printf("  %-4s %-20s %-6s %-6s %-6s %-6s %-6s %-6s %s\n",
           "----", "--------------------", "------", "------", "------", "------", "------", "------", "-----------");
    for (int i = 0; i < numSpells; i++) {
        printf("  %-4d %-20s %-6d %-6d %-6d %-6d %-6d %-6d %s\n",
               i + 1, names[i], manaCost[i], minDmg[i], maxDmg[i],
               spellType[i], level[i], element[i], descs[i]);
    }

    for (int i = 0; i < numSpells; i++) { free(names[i]); free(descs[i]); }
    free(names); free(descs);
    free(manaCost); free(minDmg); free(maxDmg); free(spellType); free(level); free(element);
}

static void parse_monstersin(Reader *r) {
    int32_t numTypes = read_int(r);
    printf("=== monstersin.dat ===\n");
    printf("Monster types: %d\n\n", numTypes);

    char **names = (char **)malloc(numTypes * sizeof(char *));
    for (int i = 0; i < numTypes; i++) names[i] = read_utf(r);

    /* 17 stat bytes per monster type */
    printf("  %-4s %-20s", "ID", "Name");
    const char *stat_names[] = {
        "S0", "S1", "Hit", "Evade", "AC", "Dmg", "S6", "S7",
        "S8", "S9", "S10", "Status", "S12", "S13", "MaxHP", "LootPct", "LootRoll"
    };
    for (int s = 0; s < 17; s++) printf(" %-7s", stat_names[s]);
    printf("\n");

    for (int i = 0; i < numTypes; i++) {
        printf("  %-4d %-20s", i + 1, names[i]);
        for (int s = 0; s < 17; s++) {
            printf(" %-7d", read_byte(r));
        }
        printf("\n");
    }

    for (int i = 0; i < numTypes; i++) free(names[i]);
    free(names);
}

static void parse_droppeditemsin(Reader *r) {
    int16_t numRows = read_short(r);
    int16_t numCols = read_short(r);
    printf("=== droppeditemsin.dat ===\n");
    printf("Loot table: %d rows x %d cols\n\n", numRows, numCols);

    printf("  %-6s", "Level");
    for (int c = 0; c < numCols; c++) printf(" Col%-3d", c);
    printf("\n");

    for (int row = 0; row < numRows; row++) {
        printf("  %-6d", row);
        for (int col = 0; col < numCols; col++) {
            printf(" %-6d", read_byte(r));
        }
        printf("\n");
    }
}

static void parse_npcstrings(Reader *r) {
    /* NPCSystem loads 8 NPC dialogue blocks with expected counts */
    int expected[] = {20, 20, 20, 20, 5, 22, 5, 41};
    const char *npc_names[] = {
        "Arantamo", "Celegil", "Favela Dralor", "Vander",
        "Beneca", "Helga", "Varus(Warden)", "Generic/System"
    };

    printf("=== npcstrings.dat ===\n\n");

    for (int npc = 0; npc < 8; npc++) {
        if (reader_remaining(r) < 4) break;
        int32_t count = read_int(r);
        printf("--- NPC %d: %s (%d strings, expected %d) ---\n",
               npc, npc_names[npc], count, expected[npc]);
        for (int i = 0; i < count; i++) {
            char *s = read_utf(r);
            printf("  [%2d] %s\n", i, s);
            free(s);
        }
        printf("\n");
    }
}

static void parse_geomin(Reader *r) {
    printf("=== geomin.dat ===\n");
    printf("Dungeon Geometry Table: 37 dungeons x 6 parameters (222 bytes)\n\n");
    printf("  %-8s %-6s %-6s %-6s %-6s %-8s %-8s\n",
           "Dungeon", "P0", "P1", "P2", "P3", "Exit1(P4)", "Exit2(P5)");
    printf("  %-8s %-6s %-6s %-6s %-6s %-8s %-8s\n",
           "-------", "------", "------", "------", "------", "---------", "---------");

    for (int dung = 0; dung < 37 && reader_remaining(r) >= 6; dung++) {
        int8_t p0 = read_byte(r);
        int8_t p1 = read_byte(r);
        int8_t p2 = read_byte(r);
        int8_t p3 = read_byte(r);
        int8_t exit1 = read_byte(r);
        int8_t exit2 = read_byte(r);
        printf("  Dung %-3d %-6d %-6d %-6d %-6d %-8d %-8d\n",
               dung + 1, p0, p1, p2, p3, exit1, exit2);
    }
}

static void parse_monsterfilenamesin(Reader *r) {
    printf("=== monsterfilenamesin.dat ===\n");
    printf("Monster Sprite Filenames: 5 rows x 7 columns (35 filenames)\n\n");

    for (int row = 0; row < 5; row++) {
        printf("--- Row %d ---\n", row);
        for (int col = 0; col < 7; col++) {
            char *name = read_utf(r);
            printf("  [%d][%d]: %s\n", row, col, name);
            free(name);
        }
        printf("\n");
    }
}

static void parse_dungnamesin(Reader *r) {
    printf("=== dungnamesin.dat ===\n");
    printf("Dungeon Names Table: 37 dungeons x 2 names\n\n");
    printf("  %-4s %-30s %-30s\n", "ID", "Dungeon Name", "Area / Level Name");
    printf("  %-4s %-30s %-30s\n", "----", "------------------------------", "------------------------------");

    for (int dung = 0; dung < 37 && reader_remaining(r) >= 4; dung++) {
        char *name1 = read_utf(r);
        char *name2 = read_utf(r);
        printf("  %-4d %-30s %-30s\n", dung + 1, name1, name2);
        free(name1);
        free(name2);
    }
}

static void parse_charin(Reader *r) {
    printf("=== charin.dat ===\n");

    int16_t numH = 0, numY = 0, numK = 0, numR = 0, numE = 0;
    char **attributes   = read_utf_array(r, &numH);
    char **categories   = read_utf_array(r, &numY);
    char **classNames   = read_utf_array(r, &numK);
    char **descriptions = read_utf_array(r, &numR);
    char **skillNames   = read_utf_array(r, &numE);

    printf("Attributes (%d):\n", numH);
    for (int i = 0; i < numH; i++) printf("  [%d] %s\n", i, attributes[i]);

    printf("\nCategories/Archetypes (%d):\n", numY);
    for (int i = 0; i < numY; i++) printf("  [%d] %s\n", i, categories[i]);

    printf("\nClasses (%d):\n", numK);
    for (int i = 0; i < numK; i++) printf("  [%d] %s\n", i, classNames[i]);

    printf("\nClass Descriptions (%d):\n", numR);
    for (int i = 0; i < numR; i++) printf("  [%d] %s\n", i, descriptions[i]);

    printf("\nSkill Types (%d, expected 14):\n", numE);
    for (int i = 0; i < numE; i++) printf("  [%d] %s\n", i, skillNames[i]);

    if (numE != 14) {
        fprintf(stderr, "Warning: skill types count %d != 14\n", numE);
    }

    printf("\nBase Skill Constants (14 shorts):\n  ");
    for (int i = 0; i < numE; i++) {
        int16_t val = read_short(r);
        printf("%d ", val);
    }
    printf("\n");

    int numStats = 13 + 2 * numE; /* 13 + 2 * 14 = 41 */
    printf("\nClass Stat Matrix: %d classes x %d stats:\n", numK, numStats);

    for (int c = 0; c < numK; c++) {
        printf("  [%d] %-15s: ", c, (c < numK && classNames) ? classNames[c] : "Unknown");
        for (int s = 0; s < numStats; s++) {
            int16_t stat = read_short(r);
            printf("%d ", stat);
        }
        printf("\n");
    }

    free_utf_array(attributes, numH);
    free_utf_array(categories, numY);
    free_utf_array(classNames, numK);
    free_utf_array(descriptions, numR);
    free_utf_array(skillNames, numE);
}

static void parse_helptext(Reader *r) {
    printf("=== helptext.dat ===\n");
    int32_t count = read_int(r);
    printf("Help topics count: %d\n\n", count);

    for (int i = 0; i < count && reader_remaining(r) >= 2; i++) {
        char *text = read_utf(r);
        printf("  [%2d] %s\n", i, text);
        free(text);
    }
}

static void parse_generic(Reader *r, const char *filename) {
    printf("=== %s ===\n", filename);
    printf("Size: %zu bytes\n\n", r->size);

    /* Try to auto-detect: if first 2 bytes look like a count, try reading UTFs */
    if (r->size >= 2) {
        int16_t maybe_count = read_short(r);
        printf("First short: %d (possible record count)\n", maybe_count);

        if (maybe_count > 0 && maybe_count < 1000) {
            printf("Attempting to read %d UTF strings:\n", maybe_count);
            for (int i = 0; i < maybe_count && reader_remaining(r) > 2; i++) {
                char *s = read_utf(r);
                printf("  [%2d] %s\n", i, s);
                free(s);
            }
        }
    }

    /* Dump remaining bytes as hex */
    if (reader_remaining(r) > 0) {
        printf("\nRemaining %d bytes (hex dump):\n", reader_remaining(r));
        int count = 0;
        while (r->pos < r->size && count < 256) {
            if (count % 16 == 0) printf("  %04X: ", (unsigned)(r->pos));
            printf("%02X ", r->data[r->pos++]);
            if (++count % 16 == 0) printf("\n");
        }
        if (count % 16 != 0) printf("\n");
        if (reader_remaining(r) > 0) {
            printf("  ... (%d more bytes)\n", reader_remaining(r));
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: dat_parser <file.dat> [file2.dat ...]\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        Reader r;
        if (!reader_open(&r, argv[i])) {
            fprintf(stderr, "Error: cannot open '%s'\n", argv[i]);
            continue;
        }

        /* Extract basename for format detection */
        const char *basename = strrchr(argv[i], '\\');
        if (!basename) basename = strrchr(argv[i], '/');
        basename = basename ? basename + 1 : argv[i];

        if (strcmp(basename, "itemsin.dat") == 0)                 parse_itemsin(&r);
        else if (strcmp(basename, "spellsin.dat") == 0)           parse_spellsin(&r);
        else if (strcmp(basename, "monstersin.dat") == 0)         parse_monstersin(&r);
        else if (strcmp(basename, "droppeditemsin.dat") == 0)     parse_droppeditemsin(&r);
        else if (strcmp(basename, "npcstrings.dat") == 0)         parse_npcstrings(&r);
        else if (strcmp(basename, "geomin.dat") == 0)             parse_geomin(&r);
        else if (strcmp(basename, "monsterfilenamesin.dat") == 0) parse_monsterfilenamesin(&r);
        else if (strcmp(basename, "dungnamesin.dat") == 0)        parse_dungnamesin(&r);
        else if (strcmp(basename, "charin.dat") == 0)             parse_charin(&r);
        else if (strcmp(basename, "helptext.dat") == 0)           parse_helptext(&r);
        else                                                      parse_generic(&r, basename);

        if (reader_remaining(&r) > 0 && strcmp(basename, "npcstrings.dat") != 0) {
            printf("\n[%d bytes remaining after parse]\n", reader_remaining(&r));
        }

        printf("\n");
        reader_close(&r);
    }

    return 0;
}
