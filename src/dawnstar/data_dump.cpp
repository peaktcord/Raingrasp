#include "src/common/replay/data_harness.hpp"
#include "src/dawnstar/profile.hpp"

int main(int argc, char **argv) {
    return data_harness::run(argc, argv, dawnstar::profile());
}
