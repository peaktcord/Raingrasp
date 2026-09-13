#ifndef COMMON_PLATFORM_INTAKE_HPP
#define COMMON_PLATFORM_INTAKE_HPP

#include <string>
#include <vector>

namespace platform {
namespace intake {

enum class Variant { Unknown, Dawnstar, Stormhold };

const char *variantId(Variant v);
Variant variantFromId(const std::string &id);

struct Result {
    enum class Status {
        Ready,
        NeedsJar,
        Refused,
    };

    Status status = Status::NeedsJar;
    std::string tree;
    Variant variant = Variant::Unknown;
    std::string message;
};

struct Request {
    Variant want = Variant::Unknown;
    std::string dataDir;
    std::string jarPath;
    std::string bareArg;
    std::string exeDir;
};

Result resolve(const Request &request);

std::string userDataDir();

void configurePortableMode(const std::string &exeDir, bool requested);

void setUserDataDirForTesting(const std::string &dir);

std::string treeDir(Variant v);

std::string settingsDir();
std::string saveDir(Variant v);

Variant identifyJar(const std::string &jarPath);

Variant identifyTree(const std::string &dir);

bool treeIsUsable(const std::string &dir, Variant v, std::string *why);

std::string treeContentDigest(const std::string &dir);

bool unpackJar(const std::string &jarPath, const std::string &dest, Variant expect,
               std::string *error);

std::vector<std::string> jarsIn(const std::string &dir);

}
}

#endif
