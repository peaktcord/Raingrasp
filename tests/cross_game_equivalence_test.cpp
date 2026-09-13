// Layer 4: cross-game equivalence.
//
// Layers 0-3 all prove the same kind of thing: *this change did not alter this
// game*. None of them proves the claim the whole unification rests on --
// *these two implementations were the same, so one copy suffices*. That needs
// a test whose subject is the pair.
//
// This one drives both variants through the same script and compares their
// traces, which the replay probes already emit. A difference is either
// declared here, with a justification pointing at the decompiled Java, or it
// is a bug.
//
// It fails in both directions, and the second is the important one:
//
//   - an *undeclared* difference means the games diverge somewhere nobody has
//     accounted for, which during a merge means one of them is about to lose
//     behaviour silently;
//   - a *declared* difference that has stopped happening means the registry is
//     describing a world that no longer exists. Without that check the list
//     rots into a set of excuses that are never revisited, and a merge that
//     accidentally erased a real difference would look like success.
//
// The traces come from the committed replay baselines rather than from running
// the games here: those baselines are already checked against the live games by
// //:dawnstar_replay_test and //:stormhold_replay_test every commit, so reading
// them keeps this test fast and its failures unambiguous -- a mismatch here is
// about the *pair*, never about one game having drifted.
//
// Usage: cross_game_equivalence_test <dawnstar.tsv> <stormhold.tsv>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

// A declared difference between the two games.
//
// `id` matches the row in docs/DIVERGENCES.md; the two documents are meant to
// be read together, and an entry here without one there is half-done.
struct Divergence {
    const char *id;
    const char *site;
    const char *why;
    const char *evidence;
    // Which trace field this shows up in. The trace is a set of `key=value`
    // pairs, so a divergence names the key it is allowed to move.
    const char *field;
    // Ticks over which the difference is expected, inclusive. A divergence
    // that is real but confined -- an extra screen, say -- should not excuse
    // the field moving for the whole run.
    int firstTick;
    int lastTick;
};

// The registry. Every entry must also appear in docs/DIVERGENCES.md.
const Divergence kDivergences[] = {
    {
        "intro-screen-count",
        "Game menu flow",
        "Dawnstar shows two introduction screens after the welcome; Stormhold "
        "shows one. Every later tick of a fixed script is therefore one screen "
        "out of step between the games, which is why this covers the tail.",
        "decompiled/dawnstar/Game.java (screens 101 and 102) vs "
        "decompiled/stormhold/Game.java (101 only)",
        "screen",
        24, 1000,
    },
    {
        // The same root cause as intro-screen-count, but a different field and
        // a bounded window, so it is declared separately rather than widening
        // that entry: Stormhold reaches the dungeon while Dawnstar is still on
        // its second intro screen, and both are running from tick 32 on. A
        // change that made this last longer would be a real regression, and
        // folding it into the entry above would hide that.
        "intro-screen-canvas-start",
        "Game menu flow",
        "Stormhold's canvas starts four ticks earlier than Dawnstar's, because "
        "Dawnstar has one more introduction screen to dismiss first.",
        "Same as intro-screen-count; verified in the replay baselines, where "
        "both games report canvas=run from tick 32.",
        "canvas",
        28, 31,
    },
    {
        "dungeon-seed",
        "Dungeon generation",
        "The generator is seeded dungeonId*8000 in Dawnstar and dungeonId*5000 "
        "in Stormhold, so the starting position in dungeon 1 differs and every "
        "later position with it.",
        "docs/DIVERGENCES.md dungeon-seed; i::b in both ports",
        "pos",
        0, 1000,
    },
    {
        "start-facing",
        "Dungeon generation",
        "A consequence of dungeon-seed: the player is placed facing a "
        "different way, so the compass differs from the first dungeon frame.",
        "docs/DIVERGENCES.md dungeon-seed",
        "dir",
        0, 1000,
    },
    {
        "character-creation-timing",
        "Game menu flow",
        "Stormhold's createNewGame runs before its progress screen is shown "
        "and so has a player by screen 6; Dawnstar's runs after, and its "
        "player fields are still zero until the dungeon is entered. A start-up "
        "ordering difference, not a gameplay one.",
        "decompiled/stormhold/Game.java:665-668 starts the job before "
        "setCurrentDisplay; the Dawnstar equivalent shows the screen first",
        "dung",
        0, 1000,
    },
};

const int kDivergenceCount = (int)(sizeof(kDivergences) / sizeof(kDivergences[0]));

// ------------------------------------------------------------------ trace --

struct Row {
    int tick = 0;
    std::map<std::string, std::string> fields;
};

std::vector<std::string> split(const std::string &text, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

// A baseline row is `script<TAB>tick<TAB>hash<TAB>note`, and the note is a run
// of `key=value` pairs. The hash is deliberately ignored: it is a digest of
// per-game state and could never match across variants.
bool parse(const std::string &path, std::vector<Row> *out) {
    std::FILE *handle = std::fopen(path.c_str(), "rb");
    if (handle == nullptr) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return false;
    }
    std::string text;
    char buffer[4096];
    size_t got;
    while ((got = std::fread(buffer, 1, sizeof(buffer), handle)) > 0) text.append(buffer, got);
    std::fclose(handle);

    for (const std::string &line : split(text, '\n')) {
        if (line.empty()) continue;
        std::vector<std::string> cols = split(line, '\t');
        if (cols.size() < 4) continue;
        Row row;
        row.tick = std::atoi(cols[1].c_str());
        for (const std::string &pair : split(cols[3], ' ')) {
            size_t eq = pair.find('=');
            if (eq == std::string::npos) continue;
            row.fields[pair.substr(0, eq)] = pair.substr(eq + 1);
        }
        out->push_back(row);
    }
    return !out->empty();
}

const Divergence *findDivergence(const std::string &field, int tick) {
    for (int n1 = 0; n1 < kDivergenceCount; ++n1) {
        const Divergence &d = kDivergences[n1];
        if (field == d.field && tick >= d.firstTick && tick <= d.lastTick) return &d;
    }
    return nullptr;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <dawnstar-replay.tsv> <stormhold-replay.tsv>\n",
                     argv[0]);
        return 2;
    }

    std::vector<Row> dawnstar, stormhold;
    if (!parse(argv[1], &dawnstar)) return 1;
    if (!parse(argv[2], &stormhold)) return 1;

    int failures = 0;
    // Which declared divergences actually fired. One that never does is as
    // much of a failure as an undeclared difference -- see the header.
    std::map<std::string, int> observed;

    size_t common = dawnstar.size() < stormhold.size() ? dawnstar.size() : stormhold.size();
    std::printf("comparing %zu ticks (dawnstar %zu, stormhold %zu)\n", common,
                dawnstar.size(), stormhold.size());

    for (size_t n1 = 0; n1 < common; ++n1) {
        const Row &a = dawnstar[n1];
        const Row &b = stormhold[n1];

        // Every field either game reports.
        std::map<std::string, bool> keys;
        for (const auto &kv : a.fields) keys[kv.first] = true;
        for (const auto &kv : b.fields) keys[kv.first] = true;

        for (const auto &kv : keys) {
            const std::string &field = kv.first;
            auto ia = a.fields.find(field);
            auto ib = b.fields.find(field);
            std::string va = ia == a.fields.end() ? std::string("<absent>") : ia->second;
            std::string vb = ib == b.fields.end() ? std::string("<absent>") : ib->second;
            if (va == vb) continue;

            const Divergence *declared = findDivergence(field, a.tick);
            if (declared != nullptr) {
                observed[declared->id]++;
                continue;
            }
            if (failures < 10) {
                std::printf("  UNDECLARED at tick %d: %s is %s (dawnstar) vs %s "
                            "(stormhold)\n",
                            a.tick, field.c_str(), va.c_str(), vb.c_str());
            }
            ++failures;
        }
    }

    // A declared divergence that no longer happens.
    for (int n1 = 0; n1 < kDivergenceCount; ++n1) {
        const Divergence &d = kDivergences[n1];
        if (observed.find(d.id) == observed.end()) {
            std::printf("  STALE: divergence '%s' is declared but never observed.\n", d.id);
            std::printf("         %s\n", d.why);
            std::printf("         If the games really do agree now, delete the entry\n"
                        "         here and in docs/DIVERGENCES.md. If a merge erased a\n"
                        "         real difference, that is the bug.\n");
            ++failures;
        }
    }

    std::printf("\ndeclared divergences observed:\n");
    for (int n1 = 0; n1 < kDivergenceCount; ++n1) {
        const Divergence &d = kDivergences[n1];
        auto it = observed.find(d.id);
        std::printf("  %-28s %s (%d tick%s)\n", d.id,
                    it == observed.end() ? "NEVER" : "yes", it == observed.end() ? 0 : it->second,
                    (it != observed.end() && it->second == 1) ? "" : "s");
    }

    if (failures != 0) {
        std::printf("\nCROSS-GAME FAIL: %d problem(s).\n", failures);
        std::printf("An undeclared difference means the games diverge somewhere nobody\n"
                    "has accounted for. Add it to docs/DIVERGENCES.md and to the\n"
                    "registry in this file, with evidence from the decompiled Java --\n"
                    "or fix it, if it is a bug.\n");
        return 1;
    }
    std::printf("\nCROSS-GAME OK: the two games agree except where declared.\n");
    return 0;
}
