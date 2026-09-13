#include "src/common/replay/monster_harness.hpp"
#include "src/dawnstar/profile.hpp"

int main(int argc, char **argv) {
    return monster_harness::run(argc, argv, dawnstar::profile());
}
