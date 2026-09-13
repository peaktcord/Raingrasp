#include "src/common/replay/visibility_harness.hpp"
#include "src/stormhold/profile.hpp"

int main(int argc, char **argv) {
    return visibility_harness::run(argc, argv, stormhold::profile());
}
