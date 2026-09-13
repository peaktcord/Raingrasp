// The resource VFS: name normalisation, layer precedence, and the .lmp reader.
//
// These are the guarantees the rest of Phase 3 and all of Phase 4 rest on, and
// two of them are the kind that fail silently rather than loudly:
//
//   - Normalisation is what stops a mod working on Windows and not on the web.
//     NTFS is case-insensitive and wasm's MEMFS is not, so `HelpText.dat`
//     resolving on one platform and not the other is a real, shipped bug that
//     no game baseline would catch -- both games only ever ask in one casing.
//
//   - Precedence is what makes `override/` mean anything. If the variant tree
//     were consulted first, an override would be dead weight that appears to
//     work whenever the base file happens to be absent.
//
// The .lmp cases pin the format against the shipped archive's actual layout,
// which is *not* what docs/DATA_FORMATS.md described before this phase.

#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"

namespace {

int failures = 0;

void check(bool ok, const char *what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

std::vector<uint8_t> bytesOf(const std::string &s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::string text(const std::vector<uint8_t> &b) {
    return std::string((const char *)b.data(), b.size());
}

class MarkerSaveStore : public platform::SaveStore {
public:
    bool load(const std::string &,
              std::vector<std::vector<uint8_t>> *) override {
        return false;
    }
    bool save(const std::string &,
              const std::vector<std::vector<uint8_t>> &) override {
        return true;
    }
    bool remove(const std::string &) override { return false; }
    std::vector<std::string> list() override { return {}; }
    int64_t lastModified(const std::string &) override { return 0; }
};

// ---------------------------------------------------------- normalisation

void testNormalise() {
    using platform::normalise;
    // The two spellings the game itself produces. GameUtil::ensureLeadingSlash
    // adds a slash that getResourceAsStream then stripped, so both reach the
    // VFS today and must land on one key.
    check(normalise("/datfiles.lmp") == "datfiles.lmp", "leading slash stripped");
    check(normalise("datfiles.lmp") == "datfiles.lmp", "bare name unchanged");
    check(normalise("/datfiles.lmp") == normalise("datfiles.lmp"),
          "both game spellings agree");

    // Case folding: the platform difference that would otherwise ship.
    check(normalise("HelpText.DAT") == "helptext.dat", "case folded");
    check(normalise("/SPLASHTOP.PNG") == "splashtop.png", "slash and case together");

    // Separators, so a Windows-shaped override path resolves the same.
    check(normalise("sub\\file.dat") == "sub/file.dat", "backslash becomes slash");
    check(normalise("//doubled//sep.dat") == "doubled/sep.dat", "doubled separators");
    check(normalise("trailing/") == "trailing", "trailing slash dropped");
    check(normalise("") == "", "empty stays empty");
}

// ------------------------------------------------------------- precedence

void testPrecedence() {
    platform::MemoryLayer over;
    platform::MemoryLayer base;
    over.put("helptext.dat", bytesOf("OVERRIDE"));
    base.put("helptext.dat", bytesOf("BASE"));
    base.put("itemsin.dat", bytesOf("ONLY-IN-BASE"));

    platform::ResourceStack stack;
    stack.push(&over);
    stack.push(&base);

    std::vector<uint8_t> got;
    check(stack.read("helptext.dat", &got), "shadowed name resolves");
    check(text(got) == "OVERRIDE", "override wins over base");

    got.clear();
    check(stack.read("itemsin.dat", &got), "base-only name resolves");
    check(text(got) == "ONLY-IN-BASE", "falls through to base");

    got.clear();
    check(!stack.read("absent.dat", &got), "missing name is a miss");

    // A miss in a higher layer must not disturb the answer a lower one gives.
    // This is the bug the `read` contract exists to prevent: a layer that
    // cleared `out` before deciding it had nothing would blank the result.
    got = bytesOf("PREEXISTING");
    check(stack.read("itemsin.dat", &got), "read after a higher-layer miss");
    check(text(got) == "ONLY-IN-BASE", "higher-layer miss left no residue");

    // The stack normalises through to its layers, so a caller may ask in any
    // spelling the game produces.
    got.clear();
    check(stack.read(platform::normalise("/HelpText.dat"), &got),
          "odd spelling resolves through the stack");
    check(text(got) == "OVERRIDE", "and still honours precedence");

    // An empty stack answers nothing rather than crashing -- the state a
    // front-end is in before it installs.
    platform::ResourceStack empty;
    got.clear();
    check(empty.empty(), "fresh stack reports empty");
    check(!empty.read("anything", &got), "empty stack is a miss");
}

// -------------------------------------------------------------- the .lmp

// Builds an archive in the shipped layout: a run of `'-' name '-' off32 len16`
// directory entries, then the data region.
std::vector<uint8_t> makeLmp(
    const std::vector<std::pair<std::string, std::string> > &members) {
    size_t dirSize = 0;
    for (size_t i = 0; i < members.size(); ++i) {
        dirSize += 1 + members[i].first.size() + 1 + 6;
    }
    std::vector<uint8_t> out;
    std::vector<uint8_t> data;
    size_t offset = dirSize;
    for (size_t i = 0; i < members.size(); ++i) {
        const std::string &name = members[i].first;
        const std::string &body = members[i].second;
        out.push_back('-');
        out.insert(out.end(), name.begin(), name.end());
        out.push_back('-');
        out.push_back((uint8_t)(offset >> 24));
        out.push_back((uint8_t)(offset >> 16));
        out.push_back((uint8_t)(offset >> 8));
        out.push_back((uint8_t)offset);
        out.push_back((uint8_t)(body.size() >> 8));
        out.push_back((uint8_t)body.size());
        data.insert(data.end(), body.begin(), body.end());
        offset += body.size();
    }
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

std::vector<std::pair<std::string, std::string> > sampleMembers() {
    std::vector<std::pair<std::string, std::string> > members;
    members.push_back(std::make_pair(std::string("charin.dat"), std::string("CHARS")));
    members.push_back(
        std::make_pair(std::string("helptext.dat"), std::string("HELP-TEXT")));
    members.push_back(std::make_pair(std::string("geomin.dat"), std::string()));
    return members;
}

void testLmp() {
    std::vector<uint8_t> archive = makeLmp(sampleMembers());
    std::map<std::string, std::vector<uint8_t> > parsed;
    check(platform::parseLmp(archive, &parsed), "well-formed archive parses");
    check(parsed.size() == 3, "all members found");
    check(text(parsed["charin.dat"]) == "CHARS", "first member's bytes");
    check(text(parsed["helptext.dat"]) == "HELP-TEXT", "second member's bytes");
    check(parsed.count("geomin.dat") == 1, "zero-length member is present");
    check(parsed["geomin.dat"].empty(), "and is empty rather than missing");

    // Members are keyed normalised, so an archive that spells a name in mixed
    // case is still found by the game's lowercase request.
    std::vector<std::pair<std::string, std::string> > odd;
    odd.push_back(std::make_pair(std::string("HelpText.DAT"), std::string("X")));
    std::map<std::string, std::vector<uint8_t> > oddParsed;
    check(platform::parseLmp(makeLmp(odd), &oddParsed), "mixed-case archive parses");
    check(oddParsed.count("helptext.dat") == 1, "member key is normalised");

    // A truncated directory entry is reported rather than silently yielding
    // nothing: a damaged archive baked into a baseline is the failure mode the
    // jar_resources provenance work exists to prevent.
    std::vector<uint8_t> truncated(archive.begin(), archive.begin() + 8);
    std::map<std::string, std::vector<uint8_t> > partial;
    check(!platform::parseLmp(truncated, &partial), "truncated archive reports failure");

    // A member whose extent runs past the end is damage, not a short read.
    std::vector<uint8_t> overrun = makeLmp(sampleMembers());
    overrun.resize(overrun.size() - 4);
    std::map<std::string, std::vector<uint8_t> > over;
    check(!platform::parseLmp(overrun, &over), "overrunning member reports failure");

    // Empty input is an archive with nothing in it, not an error.
    std::map<std::string, std::vector<uint8_t> > none;
    check(platform::parseLmp(std::vector<uint8_t>(), &none), "empty input parses");
    check(none.empty(), "and yields no members");
}

// An override must beat an archive member. Before Phase 3 the archive opener
// bypassed the resource stack, so a loose file could never shadow a packed one.
void testOverrideBeatsArchive() {
    std::vector<std::pair<std::string, std::string> > members;
    members.push_back(std::make_pair(std::string("helptext.dat"), std::string("PACKED")));
    std::map<std::string, std::vector<uint8_t> > parsed;
    check(platform::parseLmp(makeLmp(members), &parsed), "archive parses");

    platform::MemoryLayer archive;
    for (std::map<std::string, std::vector<uint8_t> >::const_iterator it = parsed.begin();
         it != parsed.end(); ++it) {
        archive.put(it->first, it->second);
    }
    platform::MemoryLayer over;
    over.put("helptext.dat", bytesOf("LOOSE"));

    platform::ResourceStack stack;
    stack.push(&over);
    stack.push(&archive);

    std::vector<uint8_t> got;
    check(stack.read("helptext.dat", &got), "name resolves");
    check(text(got) == "LOOSE", "loose override shadows the archive member");
}

// The installed slots start null, so a front-end that forgets to install fails
// at the seam instead of reading the process's working directory.
void testInstallation() {
    platform::PlatformContext *context = platform::defaultContext();
    check(context->fileSystem() == nullptr, "file system starts uninstalled");
    check(context->saveStore() == nullptr, "save store starts uninstalled");

    platform::MemoryLayer layer;
    layer.put("a.dat", bytesOf("A"));
    context->installFileSystem(&layer);
    check(context->fileSystem() == &layer, "install takes effect");

    std::vector<uint8_t> got;
    check(context->fileSystem()->read("a.dat", &got), "installed layer answers");
    check(text(got) == "A", "with the right bytes");

    context->installFileSystem(nullptr);
    check(context->fileSystem() == nullptr, "uninstall takes effect");
}

void testContextIsolation() {
    platform::MemoryLayer firstLayer;
    platform::MemoryLayer secondLayer;
    MarkerSaveStore firstSaves;
    MarkerSaveStore secondSaves;
    platform::PlatformContext first;
    platform::PlatformContext second;
    first.installFileSystem(&firstLayer);
    first.installSaveStore(&firstSaves);
    second.installFileSystem(&secondLayer);
    second.installSaveStore(&secondSaves);

    check(first.fileSystem() == &firstLayer && first.saveStore() == &firstSaves,
          "first context retains its resource and save bindings");
    check(second.fileSystem() == &secondLayer && second.saveStore() == &secondSaves,
          "second context retains independent bindings");
    second.installFileSystem(nullptr);
    check(second.fileSystem() == nullptr && first.fileSystem() == &firstLayer,
          "mutating one explicit context leaves the other untouched");
}

}  // namespace

int main() {
    // Installation first: it asserts the slots are still null, which only holds
    // before anything else installs.
    testInstallation();
    testContextIsolation();
    testNormalise();
    testPrecedence();
    testLmp();
    testOverrideBeatsArchive();

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("vfs: all checks passed\n");
    return 0;
}
