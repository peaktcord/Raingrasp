#ifndef COMMON_PLATFORM_DESKTOP_HPP
#define COMMON_PLATFORM_DESKTOP_HPP

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/platform/platform.hpp"

namespace platform {

class DirectoryLayer : public FileLayer {
public:
    explicit DirectoryLayer(std::string root) : root_(std::move(root)) {}
    bool read(const std::string &name, std::vector<uint8_t> *out) override;
    std::string describe() const override;
    const std::string &root() const { return root_; }

private:
    std::string root_;
};

class LmpLayer : public FileLayer {
public:
    explicit LmpLayer(const std::string &archive);
    bool read(const std::string &name, std::vector<uint8_t> *out) override;
    std::string describe() const override;
    bool ok() const { return ok_; }
    size_t size() const { return members_.size(); }

private:
    std::string archive_;
    bool ok_ = false;
    std::map<std::string, std::vector<uint8_t>> members_;
};

class MemoryLayer : public FileLayer {
public:
    void put(const std::string &name, std::vector<uint8_t> bytes);
    bool read(const std::string &name, std::vector<uint8_t> *out) override;
    std::string describe() const override;

private:
    std::map<std::string, std::vector<uint8_t>> files_;
};

namespace desktop {

class DesktopResourceStack : public FileLayer {
public:
    explicit DesktopResourceStack(const std::string &root);
    void setRoot(const std::string &root);
    bool read(const std::string &name, std::vector<uint8_t> *out) override;
    std::string describe() const override;

private:
    ResourceStack stack_;
    DirectoryLayer override_{""};
    DirectoryLayer tree_{""};
    std::vector<std::unique_ptr<LmpLayer>> archives_;
};

}

class DirectorySaveStore : public SaveStore {
public:
    explicit DirectorySaveStore(std::string root) : root_(std::move(root)) {}
    bool load(const std::string &name,
              std::vector<std::vector<uint8_t>> *out) override;
    bool save(const std::string &name,
              const std::vector<std::vector<uint8_t>> &records) override;
    bool remove(const std::string &name) override;
    std::vector<std::string> list() override;
    int64_t lastModified(const std::string &name) override;
    void setRoot(std::string root) { root_ = std::move(root); }
    const std::string &root() const { return root_; }

private:
    std::string path(const std::string &name) const;
    std::string root_;
};

namespace desktop {

void setResourceRoot(const std::string &dir);

void setSaveRoot(const std::string &dir);

}

bool parseLmp(const std::vector<uint8_t> &bytes,
              std::map<std::string, std::vector<uint8_t>> *out);

bool parseLmpPreservingCase(const std::vector<uint8_t> &bytes,
                            std::vector<std::pair<std::string, std::vector<uint8_t>>> *out);

}

namespace Resources {
void setRoot(const std::string &dir);
}

namespace SaveRecordFiles {
void setRoot(const std::string &dir);
}

#endif
