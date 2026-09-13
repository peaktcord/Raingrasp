#include "src/common/replay/menu_harness.hpp"
#include "src/dawnstar/profile.hpp"

int main(int argc, char **argv) {
    return menu_harness::run(argc, argv, dawnstar::profile());
}
