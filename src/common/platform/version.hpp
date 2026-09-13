#pragma once

#include <string>

namespace platform::version {

// The hand-maintained build number.  Bump it when publishing a new build.
// Nothing enforces this, which is the point of the commit id below: a report
// naming a build number that never moved is still traceable to a tree.
inline constexpr int kBuild = 1;

// The commit the binary was built from, or "unstamped" in an ordinary build.
//
// Bazel's --stamp deliberately makes a target non-deterministic, so it is not
// on by default here: `--config=release` turns it on for a build meant to be
// handed out.  Everything else builds unstamped and stays cacheable.  The
// definition lives in a generated source so only this leaf library and the two
// binaries that link it rebuild when HEAD moves.
const char *commit();

// What goes at the top of the log: "build 1 (8adabac)", or "build 1
// (unstamped)" when built without --config=release.
inline std::string describe() {
    return "build " + std::to_string(kBuild) + " (" + commit() + ")";
}

}  // namespace platform::version
