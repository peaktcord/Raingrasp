#include "src/common/replay/player_harness.hpp"
#include "src/dawnstar/profile.hpp"

int main(int argc, char **argv) {
    return player_harness::run(argc, argv, dawnstar::profile());
}
