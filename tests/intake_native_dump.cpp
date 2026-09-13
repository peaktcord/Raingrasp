// A command-line front door to native intake, for testing and for anyone who
// wants to unpack without launching a game.
//
// Its real job here is comparison. `bazel/intake.py` and
// `platform/intake.cpp` are two implementations of the same unpacking rules,
// and either one's tree is read by the same runtime -- so they have to produce
// the same tree. This makes the C++ side runnable from a script, which is what
// lets `intake_parity_test` diff the two.

#include <cstdio>
#include <cstring>
#include <string>

#include "src/common/platform/intake.hpp"

namespace intake = platform::intake;

int main(int argc, char **argv) {
    if (argc >= 3 && std::strcmp(argv[1], "identify") == 0) {
        intake::Variant v = intake::identifyJar(argv[2]);
        std::printf("%s\n", intake::variantId(v));
        return v == intake::Variant::Unknown ? 1 : 0;
    }

    if (argc >= 4 && std::strcmp(argv[1], "unpack") == 0) {
        intake::Variant expect = intake::Variant::Unknown;
        for (int n = 4; n < argc; ++n) {
            if (std::strcmp(argv[n], "--expect") == 0 && n + 1 < argc) {
                expect = intake::variantFromId(argv[++n]);
            }
        }
        std::string error;
        if (!intake::unpackJar(argv[2], argv[3], expect, &error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::printf("unpacked %s into %s\n",
                    intake::variantId(intake::identifyJar(argv[2])), argv[3]);
        return 0;
    }

    if (argc >= 4 && std::strcmp(argv[1], "check") == 0) {
        std::string why;
        intake::Variant v = intake::variantFromId(argv[3]);
        if (!intake::treeIsUsable(argv[2], v, &why)) {
            std::fprintf(stderr, "%s: %s\n", argv[2], why.c_str());
            return 1;
        }
        std::printf("%s: usable\n", argv[2]);
        return 0;
    }

    std::fprintf(stderr,
                 "usage: %s identify <archive.jar>\n"
                 "       %s unpack <archive.jar> <dir> [--expect <variant>]\n"
                 "       %s check <dir> <variant>\n",
                 argv[0], argv[0], argv[0]);
    return 2;
}
