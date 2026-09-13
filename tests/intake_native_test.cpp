// Native intake: identification, unpacking, and the resolution precedence.
//
// This runs against the user's real JARs, because the question it answers is
// whether a player's own file produces a tree the game can actually boot from.
// `intake_test` covers the Python unpacker's rules on synthesised archives;
// this covers the C++ one on the genuine article, and the two implementations
// must agree because either one's tree is read by the same runtime.
//
// The case worth naming: after unpacking, this asserts that `imgfiles.lmp` is
// still present. Expanding it is not the same as replacing it --
// `Game::createImageFromFile` reads that archive directly to build the sprite
// table, so a tree that kept only the expanded members segfaults during
// startup. Finding that out by crashing is how it was found the first time.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "src/common/platform/intake.hpp"

namespace fs = std::filesystem;
namespace intake = platform::intake;

namespace {

int failures = 0;

void check(bool ok, const char *what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool exists(const std::string &path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <dawnstar.jar> <stormhold.jar> [scratch-dir]\n",
                     argv[0]);
        return 2;
    }
    std::string dsJar = argv[1];
    std::string shJar = argv[2];
    const char *testTmp = std::getenv("TEST_TMPDIR");
    std::string scratch = testTmp != nullptr
                              ? (fs::path(testTmp) / "intake_native_scratch").string()
                              : (argc >= 4 ? argv[3] : "intake_native_scratch");

    std::error_code ec;
    fs::remove_all(scratch, ec);
    ec.clear();
    fs::create_directories(scratch, ec);
    if (ec) {
        std::fprintf(stderr, "cannot create scratch directory %s: %s\n",
                     scratch.c_str(), ec.message().c_str());
        return 2;
    }

    // Every unpack below must land in the scratch directory. Without this the
    // resolve() cases would write into the real user's %LOCALAPPDATA%, which a
    // test has no business touching.
    intake::setUserDataDirForTesting(scratch + "/userdata");
    check(intake::treeDir(intake::Variant::Dawnstar) ==
              scratch + "/userdata\\dawnstar",
          "paths: Dawnstar resources live under the data root");
    check(intake::saveDir(intake::Variant::Stormhold) ==
              scratch + "/userdata\\saves\\stormhold",
          "paths: saves are separated by game under the data root");
    check(intake::settingsDir() == scratch + "/userdata\\settings",
          "paths: port settings have a shared directory");

    // ------------------------------------------------------- identification

    check(intake::identifyJar(dsJar) == intake::Variant::Dawnstar,
          "identify: the real Dawnstar JAR");
    // The manifest names only the series here, so this is the case a stricter
    // cross-check would wrongly refuse.
    check(intake::identifyJar(shJar) == intake::Variant::Stormhold,
          "identify: the real Stormhold JAR, whose manifest names no game");

    // Content decides, not the name: the same bytes under a misleading name.
    std::string misnamed = scratch + "/stormhold-definitely.jar";
    fs::copy_file(dsJar, misnamed, fs::copy_options::overwrite_existing, ec);
    check(intake::identifyJar(misnamed) == intake::Variant::Dawnstar,
          "identify: content beats a misleading filename");

    check(intake::identifyJar(scratch + "/nonexistent.jar") ==
              intake::Variant::Unknown,
          "identify: a missing file is Unknown, not a crash");

    // --------------------------------------------------------------- unpack

    std::string dsTree = scratch + "/dawnstar";
    std::string error;
    bool unpacked = intake::unpackJar(dsJar, dsTree, intake::Variant::Dawnstar, &error);
    if (!unpacked) std::fprintf(stderr, "Dawnstar unpack failed: %s\n", error.c_str());
    check(unpacked, "unpack: the real Dawnstar JAR");

    // The .lmp members are expanded, so override/ can shadow one by name.
    check(exists(dsTree + "/helptext.dat"), "unpack: datfiles member expanded");
    check(exists(dsTree + "/geomin.dat"), "unpack: another datfiles member expanded");
    check(exists(dsTree + "/panel.png"), "unpack: imgfiles member expanded");

    // ...and the archives are still there, which is the load-bearing half.
    check(exists(dsTree + "/datfiles.lmp"), "unpack: datfiles.lmp kept");
    check(exists(dsTree + "/imgfiles.lmp"),
          "unpack: imgfiles.lmp kept, because the game reads it directly");

    check(exists(dsTree + "/stamp.json"), "unpack: stamp written");

    std::string shTree = scratch + "/stormhold";
    check(intake::unpackJar(shJar, shTree, intake::Variant::Stormhold, &error),
          "unpack: the real Stormhold JAR");
    check(exists(shTree + "/itemsin.dat"), "unpack: stormhold loose data present");
    check(exists(shTree + "/baglarge.cus"), "unpack: stormhold .cus present");

    // A JAR handed to the wrong game is refused, and says which game it is.
    std::string wrongTree = scratch + "/wrong";
    error.clear();
    check(!intake::unpackJar(dsJar, wrongTree, intake::Variant::Stormhold, &error),
          "unpack: a mismatched expectation is refused");
    check(error.find("dawnstar") != std::string::npos,
          "unpack: and the refusal names the real game");

    // -------------------------------------------------------- tree checking

    std::string why;
    check(intake::treeIsUsable(dsTree, intake::Variant::Dawnstar, &why),
          "tree: a freshly unpacked Dawnstar tree is usable");
    check(intake::treeIsUsable(shTree, intake::Variant::Stormhold, &why),
          "tree: a freshly unpacked Stormhold tree is usable");

    // The crash-shaped failure, reported as a sentence instead.
    std::string maimed = scratch + "/maimed";
    fs::copy(dsTree, maimed, fs::copy_options::recursive, ec);
    fs::remove(maimed + "/imgfiles.lmp", ec);
    why.clear();
    check(!intake::treeIsUsable(maimed, intake::Variant::Dawnstar, &why),
          "tree: a tree missing imgfiles.lmp is not usable");
    check(why.find("imgfiles.lmp") != std::string::npos,
          "tree: and the reason names the archive");

    check(!intake::treeIsUsable(scratch + "/nope", intake::Variant::Dawnstar, &why),
          "tree: a missing directory is not usable");

    // ----------------------------------------------------------- precedence

    // --data wins, and is taken as-is.
    intake::Request req;
    req.want = intake::Variant::Dawnstar;
    req.dataDir = dsTree;
    intake::Result res = intake::resolve(req);
    check(res.status == intake::Result::Status::Ready, "resolve: --data is accepted");
    check(res.tree == dsTree, "resolve: --data is used verbatim");

    // --data that is not usable is refused rather than silently falling
    // through to a scan: the user named a directory and deserves to know it is
    // wrong, not to be given a different one.
    req = intake::Request();
    req.want = intake::Variant::Dawnstar;
    req.dataDir = maimed;
    res = intake::resolve(req);
    check(res.status == intake::Result::Status::Refused,
          "resolve: an unusable --data is refused, not worked around");
    check(res.message.find("imgfiles.lmp") != std::string::npos,
          "resolve: and the message explains what is wrong");

    // A bare argument that is a directory behaves like --data. This is the
    // existing command line, which every harness relies on.
    req = intake::Request();
    req.want = intake::Variant::Stormhold;
    req.bareArg = shTree;
    res = intake::resolve(req);
    check(res.status == intake::Result::Status::Ready,
          "resolve: a bare directory still works");
    check(res.tree == shTree, "resolve: and is used as the tree");

    // A bare argument that is a JAR is intake -- this is drag-and-drop onto the
    // .exe, which arrives as argv[1].
    req = intake::Request();
    req.want = intake::Variant::Dawnstar;
    req.jarPath = dsJar;
    res = intake::resolve(req);
    check(res.status == intake::Result::Status::Ready,
          "resolve: a supplied JAR is unpacked and used");
    check(intake::treeIsUsable(res.tree, intake::Variant::Dawnstar, &why),
          "resolve: and the resulting tree is usable");

    // Nothing at all is a first-run state with instructions, not a failure.
    req = intake::Request();
    req.want = intake::Variant::Dawnstar;
    req.exeDir = scratch + "/empty";
    fs::create_directories(req.exeDir, ec);
    res = intake::resolve(req);
    // This may legitimately find a real tree in the user's own data directory,
    // which is a Ready answer and not a bug; only the empty case is asserted.
    if (res.status == intake::Result::Status::NeedsJar) {
        check(res.message.find(".jar") != std::string::npos,
              "resolve: the first-run message says what to supply");
        check(res.message.find("drag") != std::string::npos,
              "resolve: and how to supply it");
    }

    // The scan: a JAR beside the executable, under a name that says nothing.
    std::string scanDir = scratch + "/scan";
    fs::create_directories(scanDir, ec);
    fs::copy_file(shJar, scanDir + "/download(2).jar",
                  fs::copy_options::overwrite_existing, ec);
    std::vector<std::string> found = intake::jarsIn(scanDir);
    check(found.size() == 1, "scan: finds a .jar regardless of its name");

    // Portable mode redirects every writable location as one unit. The marker
    // supports a self-contained ZIP without requiring a command-line flag.
    std::string portableExe = scratch + "/portable";
    fs::create_directories(portableExe, ec);
    std::FILE *marker = std::fopen((portableExe + "/portable.txt").c_str(), "wb");
    if (marker != nullptr) std::fclose(marker);
    intake::configurePortableMode(portableExe, false);
    check(intake::userDataDir() == portableExe + "\\data",
          "paths: portable.txt selects data beside the executable");
    check(intake::saveDir(intake::Variant::Dawnstar) ==
              portableExe + "\\data\\saves\\dawnstar",
          "paths: portable saves move with the extracted data");

    fs::remove_all(scratch, ec);

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("native intake: all checks passed\n");
    return 0;
}
