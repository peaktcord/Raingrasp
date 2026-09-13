#include "src/common/platform/intake.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <utility>

#include "src/common/platform/desktop.hpp"
#include "src/common/platform/inflate.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/platform/sha256.hpp"

namespace fs = std::filesystem;

namespace platform {
namespace intake {

namespace {

const int kUnpackerVersion = 1;

#ifdef _WIN32
const char kSep = '\\';
#else
const char kSep = '/';
#endif

bool slurp(const std::string &path, std::vector<uint8_t> *out) {
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out->resize((size_t)(n < 0 ? 0 : n));
    if (!out->empty()) {
        size_t got = std::fread(out->data(), 1, out->size(), f);
        out->resize(got);
    }
    std::fclose(f);
    return true;
}

bool spill(const std::string &path, const std::vector<uint8_t> &bytes) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::FILE *f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    if (!bytes.empty()) std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return true;
}

std::string stampField(const std::string &stamp, const char *key) {
    std::string needle = std::string("\"") + key + "\"";
    size_t at = stamp.find(needle);
    if (at == std::string::npos) return "";
    at = stamp.find(':', at + needle.size());
    if (at == std::string::npos) return "";
    size_t open = stamp.find('"', at);
    if (open == std::string::npos) return "";
    size_t close = stamp.find('"', open + 1);
    if (close == std::string::npos) return "";
    return stamp.substr(open + 1, close - open - 1);
}

std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
    }
    return s;
}

bool endsWith(const std::string &s, const char *suffix) {
    size_t n = std::string(suffix).size();
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

Variant byMembers(const std::vector<std::string> &names) {
    bool hasDatfiles = false;
    bool hasImgfiles = false;
    bool looseDat = false;
    bool hasCus = false;
    for (size_t i = 0; i < names.size(); ++i) {
        std::string n = lower(names[i]);
        if (n == "datfiles.lmp") hasDatfiles = true;
        if (n == "imgfiles.lmp") hasImgfiles = true;
        if (endsWith(n, ".dat")) looseDat = true;
        if (endsWith(n, ".cus")) hasCus = true;
    }
    bool hasLmp = hasDatfiles && hasImgfiles;
    if (hasLmp && !hasCus) return Variant::Dawnstar;
    if (hasCus && looseDat && !hasLmp) return Variant::Stormhold;
    return Variant::Unknown;
}

Variant byManifest(const std::string &text) {
    std::string t = lower(text);
    if (t.find("dawnstar") != std::string::npos) return Variant::Dawnstar;
    if (t.find("stormhold") != std::string::npos) return Variant::Stormhold;
    return Variant::Unknown;
}

std::string envOrEmpty(const char *name) {
#ifdef _WIN32
    char *value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) return std::string();
    std::string out(value);
    std::free(value);
    return out;
#else
    const char *value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
#endif
}

}

const char *variantId(Variant v) {
    switch (v) {
        case Variant::Dawnstar: return "dawnstar";
        case Variant::Stormhold: return "stormhold";
        default: return "unknown";
    }
}

Variant variantFromId(const std::string &id) {
    std::string v = lower(id);
    if (v == "dawnstar") return Variant::Dawnstar;
    if (v == "stormhold") return Variant::Stormhold;
    return Variant::Unknown;
}

namespace {
std::string &overrideDir() {
    static std::string dir;
    return dir;
}
}

void setUserDataDirForTesting(const std::string &dir) { overrideDir() = dir; }

std::string userDataDir() {
    if (!overrideDir().empty()) return overrideDir();
    std::string base = envOrEmpty("LOCALAPPDATA");
    if (base.empty()) base = envOrEmpty("XDG_DATA_HOME");
    if (base.empty()) {
        std::string home = envOrEmpty("HOME");
        if (!home.empty()) base = home + "/.local/share";
    }
    if (base.empty()) base = ".";
    std::string out = base + "/Raingrasp";
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] == '/') out[i] = kSep;
    }
    return out;
}

void configurePortableMode(const std::string &exeDir, bool requested) {
    if (exeDir.empty()) return;
    std::error_code ec;
    const fs::path marker = fs::path(exeDir) / "portable.txt";
    if (requested || fs::is_regular_file(marker, ec)) {
        overrideDir() = (fs::path(exeDir) / "data").string();
    }
}

std::string treeDir(Variant v) {
    return userDataDir() + kSep + variantId(v);
}

std::string settingsDir() {
    return userDataDir() + kSep + "settings";
}

std::string saveDir(Variant v) {
    return userDataDir() + kSep + "saves" + kSep + variantId(v);
}

Variant identifyJar(const std::string &jarPath) {
    std::vector<uint8_t> bytes;
    if (!slurp(jarPath, &bytes)) return Variant::Unknown;

    std::vector<ZipEntry> entries;
    std::string error;
    if (!readZip(bytes, &entries, &error)) return Variant::Unknown;

    std::vector<std::string> names;
    std::string manifest;
    for (size_t i = 0; i < entries.size(); ++i) {
        names.push_back(entries[i].name);
        if (lower(entries[i].name) == "meta-inf/manifest.mf") {
            manifest.assign((const char *)entries[i].data.data(),
                            entries[i].data.size());
        }
    }

    Variant members = byMembers(names);
    if (members == Variant::Unknown) return Variant::Unknown;

    Variant manifestSays = byManifest(manifest);
    if (manifestSays != Variant::Unknown && manifestSays != members) {
        return Variant::Unknown;
    }
    return members;
}

std::vector<std::string> jarsIn(const std::string &dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const fs::directory_entry &e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        if (lower(e.path().extension().string()) == ".jar") {
            out.push_back(e.path().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool treeIsUsable(const std::string &dir, Variant v, std::string *why) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        if (why != nullptr) *why = "no such directory";
        return false;
    }

    std::vector<uint8_t> stampBytes;
    bool haveStamp = slurp(dir + "/stamp.json", &stampBytes);
    std::string stamp((const char *)stampBytes.data(), stampBytes.size());

    if (haveStamp && stamp.find("\"variant\"") != std::string::npos) {
        std::string want = std::string("\"") + variantId(v) + "\"";
        if (stamp.find(want) == std::string::npos) {
            if (why != nullptr) *why = "stamp.json records a different game";
            return false;
        }
    }

    if (v == Variant::Dawnstar) {
        static const char *required[] = {"datfiles.lmp", "imgfiles.lmp"};
        for (size_t i = 0; i < 2; ++i) {
            if (!fs::is_regular_file(dir + "/" + required[i], ec)) {
                if (why != nullptr) {
                    *why = std::string(required[i]) +
                           " is missing; the game reads it directly and cannot "
                           "start without it";
                }
                return false;
            }
        }
    } else if (v == Variant::Stormhold) {
        if (!fs::is_regular_file(dir + "/itemsin.dat", ec)) {
            if (why != nullptr) *why = "itemsin.dat is missing";
            return false;
        }
    }

    return true;
}

std::string treeContentDigest(const std::string &dir) {
    std::vector<uint8_t> bytes;
    if (!slurp(dir + "/stamp.json", &bytes)) return "";
    std::string stamp((const char *)bytes.data(), bytes.size());
    return stampField(stamp, "content_sha256");
}

bool unpackJar(const std::string &jarPath, const std::string &dest, Variant expect,
               std::string *error) {
    std::vector<uint8_t> bytes;
    if (!slurp(jarPath, &bytes)) {
        *error = "cannot read " + jarPath;
        return false;
    }

    std::vector<ZipEntry> entries;
    std::string zipError;
    if (!readZip(bytes, &entries, &zipError)) {
        *error = zipError;
        return false;
    }

    std::vector<std::string> names;
    std::string manifest;
    for (size_t i = 0; i < entries.size(); ++i) {
        names.push_back(entries[i].name);
        if (lower(entries[i].name) == "meta-inf/manifest.mf") {
            manifest.assign((const char *)entries[i].data.data(),
                            entries[i].data.size());
        }
    }

    Variant members = byMembers(names);
    if (members == Variant::Unknown) {
        *error =
            "that file does not look like either game. Dawnstar packs "
            "datfiles.lmp and imgfiles.lmp; Stormhold ships its .dat and .cus "
            "files loose.";
        return false;
    }
    Variant manifestSays = byManifest(manifest);
    if (manifestSays != Variant::Unknown && manifestSays != members) {
        *error = std::string("refusing that JAR: its members look like ") +
                 variantId(members) + " but its manifest says " +
                 variantId(manifestSays) + ".";
        return false;
    }
    if (expect != Variant::Unknown && expect != members) {
        *error = std::string("that is a ") + variantId(members) + " JAR, not " +
                 variantId(expect) + ".";
        return false;
    }

    std::vector<std::pair<std::string, const std::vector<uint8_t> *>> content;
    for (size_t i = 0; i < entries.size(); ++i) {
        content.push_back(std::make_pair(entries[i].name, &entries[i].data));
    }
    std::string digest = contentDigest(content);

    if (treeContentDigest(dest) == digest) {
        std::string why;
        if (treeIsUsable(dest, members, &why)) return true;
    }

    std::error_code ec;
    fs::create_directories(dest, ec);

    fs::path root = fs::weakly_canonical(fs::absolute(dest, ec), ec);
    size_t written = 0;
    std::string expanded;
    for (size_t i = 0; i < entries.size(); ++i) {
        fs::path target = fs::weakly_canonical(root / entries[i].name, ec);
        fs::path relative = target.lexically_relative(root);
        if (relative.empty() || relative.is_absolute() ||
            *relative.begin() == fs::path("..")) {
            *error = "refusing an entry outside the destination: " + entries[i].name;
            return false;
        }
        std::string t = target.string();
        if (!spill(t, entries[i].data)) {
            *error = "cannot write " + t;
            return false;
        }
        ++written;

        if (endsWith(lower(entries[i].name), ".lmp")) {
            std::vector<std::pair<std::string, std::vector<uint8_t>>> members2;
            if (parseLmpPreservingCase(entries[i].data, &members2)) {
                for (size_t m = 0; m < members2.size(); ++m) {
                    if (spill((root / members2[m].first).string(), members2[m].second)) {
                        ++written;
                    }
                }
                expanded += std::string(expanded.empty() ? "" : ", ") + "\"" +
                            entries[i].name + "\"";
            }
        }
    }

    if (written == 0) {
        *error = jarPath + " contained no files";
        return false;
    }

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
                  "{\n  \"unpacker\": \"native\",\n  \"unpacker_version\": %d,\n"
                  "  \"variant\": \"%s\",\n  \"content_sha256\": \"%s\",\n"
                  "  \"files_written\": %u,\n"
                  "  \"archives_expanded\": [%s]\n}\n",
                  kUnpackerVersion, variantId(members), digest.c_str(),
                  (unsigned)written, expanded.c_str());
    std::string stamp(buf);
    spill((root / "stamp.json").string(),
          std::vector<uint8_t>(stamp.begin(), stamp.end()));
    return true;
}

Variant identifyTree(const std::string &dir) {
    std::string why;
    if (treeIsUsable(dir, Variant::Dawnstar, &why)) return Variant::Dawnstar;
    if (treeIsUsable(dir, Variant::Stormhold, &why)) return Variant::Stormhold;
    return Variant::Unknown;
}

Result resolve(const Request &request) {
    Result result;
    result.variant = request.want;

    if (!request.dataDir.empty()) {
        std::string why;
        Variant v = request.want == Variant::Unknown ? identifyTree(request.dataDir)
                                                     : request.want;
        if (v != Variant::Unknown && treeIsUsable(request.dataDir, v, &why)) {
            result.status = Result::Status::Ready;
            result.variant = v;
            result.tree = request.dataDir;
            return result;
        }
        if (v == Variant::Unknown) why = "it carries no intake stamp naming a game";
        result.status = Result::Status::Refused;
        result.message = request.dataDir + " is not a usable game directory: " + why;
        return result;
    }

    std::string jar = request.jarPath;
    if (jar.empty() && !request.bareArg.empty()) {
        std::error_code ec;
        if (fs::is_directory(request.bareArg, ec)) {
            std::string why;
            Variant v = request.want == Variant::Unknown ? identifyTree(request.bareArg)
                                                         : request.want;
            if (v != Variant::Unknown && treeIsUsable(request.bareArg, v, &why)) {
                result.status = Result::Status::Ready;
                result.variant = v;
                result.tree = request.bareArg;
                return result;
            }
            if (v == Variant::Unknown) why = "it carries no intake stamp naming a game";
            result.status = Result::Status::Refused;
            result.message =
                request.bareArg + " is not a usable game directory: " + why;
            return result;
        }
        jar = request.bareArg;
    }

    if (!jar.empty()) {
        Variant expect = request.want;
        if (expect == Variant::Unknown) {
            expect = identifyJar(jar);
            if (expect == Variant::Unknown) {
                result.status = Result::Status::Refused;
                result.message = jar + " is not a Dawnstar or Stormhold JAR";
                return result;
            }
        }
        std::string dest = treeDir(expect);
        std::string error;
        if (unpackJar(jar, dest, expect, &error)) {
            result.status = Result::Status::Ready;
            result.variant = expect;
            result.tree = dest;
            return result;
        }
        Variant actual = identifyJar(jar);
        if (actual != Variant::Unknown && actual != request.want) {
            std::string otherDest = treeDir(actual);
            std::string otherError;
            if (unpackJar(jar, otherDest, actual, &otherError)) {
                result.status = Result::Status::Refused;
                result.message = std::string("that is a ") + variantId(actual) +
                                 " JAR, not " + variantId(request.want) +
                                 ". It has been unpacked into " + otherDest +
                                 " for the " + variantId(actual) + " build.";
                return result;
            }
        }
        result.status = Result::Status::Refused;
        result.message = error;
        return result;
    }

    if (request.want == Variant::Unknown) {
        Result found[2];
        int hits = 0;
        const Variant both[] = {Variant::Dawnstar, Variant::Stormhold};
        for (Variant v : both) {
            Request narrowed = request;
            narrowed.want = v;
            Result r = resolve(narrowed);
            if (r.status == Result::Status::Ready) found[hits++] = r;
        }
        if (hits == 1) return found[0];
        if (hits == 2) {
            result.status = Result::Status::Refused;
            result.message =
                "Both games' data are present. Say which to play: "
                "--game dawnstar or --game stormhold.";
            return result;
        }
        result.status = Result::Status::NeedsJar;
        result.message =
            "No game data found.\n\n"
            "This port ships no game assets. Supply your own copy of the "
            "Dawnstar or Stormhold .jar in any of these ways:\n"
            "  - drag the .jar onto this program\n"
            "  - pass it:  --jar <path-to.jar>\n"
            "  - put it beside this program, or in " + userDataDir() + "\n"
            "  - point at an already-unpacked directory:  --data <dir>\n"
            "  - or unpack it beside this program as ./dawnstar or ./stormhold"
            "\n\n"
            "It is identified by its contents, so the filename does not matter.";
        return result;
    }

    std::string tree = treeDir(request.want);
    std::string why;
    if (treeIsUsable(tree, request.want, &why)) {
        result.status = Result::Status::Ready;
        result.tree = tree;
        return result;
    }

    if (!request.exeDir.empty()) {
        std::string beside = request.exeDir + kSep + variantId(request.want);
        std::string besideWhy;
        if (treeIsUsable(beside, request.want, &besideWhy)) {
            result.status = Result::Status::Ready;
            result.tree = beside;
            return result;
        }
    }

    std::vector<std::string> candidates;
    if (!request.exeDir.empty()) {
        std::vector<std::string> found = jarsIn(request.exeDir);
        candidates.insert(candidates.end(), found.begin(), found.end());
    }
    std::vector<std::string> inData = jarsIn(userDataDir());
    candidates.insert(candidates.end(), inData.begin(), inData.end());

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (identifyJar(candidates[i]) != request.want) continue;
        std::string error;
        if (unpackJar(candidates[i], tree, request.want, &error)) {
            result.status = Result::Status::Ready;
            result.tree = tree;
            return result;
        }
    }

    result.status = Result::Status::NeedsJar;
    result.message =
        std::string("No ") + variantId(request.want) +
        " game data found.\n\n"
        "This port ships no game assets. Supply your own copy of the " +
        variantId(request.want) +
        " .jar in any of these ways:\n"
        "  - drag the .jar onto this program\n"
        "  - pass it:  --jar <path-to.jar>\n"
        "  - put it beside this program, or in " + userDataDir() + "\n"
        "  - point at an already-unpacked directory:  --data <dir>\n"
        "  - or unpack it beside this program as ./" + variantId(request.want) +
        "\n\n"
        "It is identified by its contents, so the filename does not matter.";
    return result;
}

}
}
