#include "src/common/platform/desktop.hpp"
#include "src/common/save_records.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <filesystem>
namespace fs = std::filesystem;
static int fails = 0;
static void check(bool ok, const char *what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++fails;
}
int main() {
    const std::string root = "atomic-save-check";
    fs::remove_all(root);
    platform::DirectorySaveStore store(root);
    std::vector<std::vector<uint8_t>> good{{1,2,3},{4,5}};
    check(store.save("slot", good), "first save succeeds");
    check(fs::exists(root + "/slot.rs"), "slot.rs written");
    check(!fs::exists(root + "/slot.rs.tmp"), "no temp left behind");

    std::vector<std::vector<uint8_t>> back;
    check(store.load("slot", &back) && back == good, "round-trips");

    // list() must see exactly one store even with a stray temp present.
    { std::FILE *f = std::fopen((root + "/slot.rs.tmp").c_str(), "wb"); std::fputc('x', f); std::fclose(f); }
    std::vector<std::string> names = store.list();
    check(names.size() == 1 && names[0] == "slot", "list() ignores a leftover temp");
    fs::remove(root + "/slot.rs.tmp");

    // Overwriting in place keeps a single file and the new contents.
    std::vector<std::vector<uint8_t>> second{{9}};
    check(store.save("slot", second), "overwrite succeeds");
    back.clear();
    check(store.load("slot", &back) && back == second, "overwrite replaced contents");
    check(!fs::exists(root + "/slot.rs.tmp"), "no temp after overwrite");

    // A replacement SaveRecords transaction must not retain the previous
    // generation. Game load reads record 1 from this single logical slot.
    platform::PlatformContext context;
    context.installSaveStore(&store);
    SaveRecords *records = SaveRecords::replace(&context, "records");
    SharedArray<int8_t> first{1, 2, 3};
    records->add(first, 0, first.length());
    records->close();
    delete records;
    records = SaveRecords::replace(&context, "records");
    SharedArray<int8_t> latest{9};
    records->add(latest, 0, latest.length());
    records->close();
    delete records;
    records = SaveRecords::open(&context, "records", false);
    check(records->recordCount() == 1 && records->get(1)[0] == 9,
          "replacement transaction keeps only the newest generation");
    delete records;

    // The failure path: make the temp name un-creatable by parking a
    // directory there, then confirm the previous save survives untouched.
    fs::create_directory(root + "/slot.rs.tmp");
    check(!store.save("slot", good), "save fails when the temp cannot be written");
    back.clear();
    check(store.load("slot", &back) && back == second, "previous save survived the failure");
    fs::remove_all(root + "/slot.rs.tmp");

    fs::remove_all(root);
    std::printf(fails ? "FAILED\n" : "PASSED\n");
    return fails ? 1 : 0;
}
