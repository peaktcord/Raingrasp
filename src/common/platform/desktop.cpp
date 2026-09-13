#include "src/common/platform/desktop.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <utility>

#include "src/common/save_records.hpp"

namespace fs = std::filesystem;

namespace platform {

namespace {

bool slurp(const std::string &path, std::vector<uint8_t> *out) {
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data((size_t)(n < 0 ? 0 : n));
    if (!data.empty()) {
        size_t got = std::fread(data.data(), 1, data.size(), f);
        data.resize(got);
    }
    std::fclose(f);
    *out = std::move(data);
    return true;
}

uint32_t readBE32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

uint32_t readBE16(const uint8_t *p) {
    return ((uint32_t)p[0] << 8) | (uint32_t)p[1];
}

}

namespace {

template <typename Emit>
bool parseLmpInto(const std::vector<uint8_t> &bytes, Emit emit) {
    size_t pos = 0;
    while (pos < bytes.size() && bytes[pos] == '-') {
        ++pos;
        size_t nameStart = pos;
        while (pos < bytes.size() && bytes[pos] != '-') ++pos;
        if (pos >= bytes.size()) return false;
        std::string name((const char *)&bytes[nameStart], pos - nameStart);
        ++pos;
        if (pos + 6 > bytes.size()) return false;
        uint32_t offset = readBE32(&bytes[pos]);
        uint32_t size = readBE16(&bytes[pos + 4]);
        pos += 6;
        if ((size_t)offset + (size_t)size > bytes.size()) return false;
        std::vector<uint8_t> member(bytes.begin() + (long)offset,
                                    bytes.begin() + (long)(offset + size));
        emit(name, std::move(member));
    }
    return true;
}

}

bool parseLmp(const std::vector<uint8_t> &bytes,
              std::map<std::string, std::vector<uint8_t>> *out) {
    return parseLmpInto(bytes, [out](const std::string &name,
                                     std::vector<uint8_t> body) {
        (*out)[normalise(name)] = std::move(body);
    });
}

bool parseLmpPreservingCase(
    const std::vector<uint8_t> &bytes,
    std::vector<std::pair<std::string, std::vector<uint8_t>>> *out) {
    return parseLmpInto(bytes, [out](const std::string &name,
                                     std::vector<uint8_t> body) {
        out->push_back(std::make_pair(name, std::move(body)));
    });
}

LmpLayer::LmpLayer(const std::string &archive) : archive_(archive) {
    std::vector<uint8_t> bytes;
    if (!slurp(archive, &bytes)) return;
    ok_ = parseLmp(bytes, &members_);
}

bool LmpLayer::read(const std::string &name, std::vector<uint8_t> *out) {
    std::map<std::string, std::vector<uint8_t>>::const_iterator it =
        members_.find(name);
    if (it == members_.end()) return false;
    *out = it->second;
    return true;
}

std::string LmpLayer::describe() const {
    return "lmp:" + archive_ + (ok_ ? "" : "(unreadable)");
}

bool DirectoryLayer::read(const std::string &name, std::vector<uint8_t> *out) {
    if (name.empty()) return false;
    if (name.find("..") != std::string::npos) return false;
    return slurp(root_ + "/" + name, out);
}

std::string DirectoryLayer::describe() const { return "dir:" + root_; }

void MemoryLayer::put(const std::string &name, std::vector<uint8_t> bytes) {
    files_[normalise(name)] = std::move(bytes);
}

bool MemoryLayer::read(const std::string &name, std::vector<uint8_t> *out) {
    std::map<std::string, std::vector<uint8_t>>::const_iterator it =
        files_.find(name);
    if (it == files_.end()) return false;
    *out = it->second;
    return true;
}

std::string MemoryLayer::describe() const { return "memory"; }

std::string DirectorySaveStore::path(const std::string &name) const {
    return root_ + "/" + name + ".rs";
}

namespace {

uint32_t readU32(std::FILE *f) {
    uint8_t b[4] = {0, 0, 0, 0};
    if (std::fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

void writeU32(std::FILE *f, uint32_t v) {
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16),
                    (uint8_t)(v >> 24)};
    std::fwrite(b, 1, 4, f);
}

}

bool DirectorySaveStore::load(const std::string &name,
                              std::vector<std::vector<uint8_t>> *out) {
    std::FILE *f = std::fopen(path(name).c_str(), "rb");
    if (f == nullptr) return false;
    uint32_t count = readU32(f);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t len = readU32(f);
        std::vector<uint8_t> rec(len);
        if (len != 0 && std::fread(rec.data(), 1, len, f) != len) break;
        out->push_back(std::move(rec));
    }
    std::fclose(f);
    return true;
}

bool DirectorySaveStore::save(const std::string &name,
                              const std::vector<std::vector<uint8_t>> &records) {
    std::error_code ec;
    fs::create_directories(root_, ec);
    const std::string target = path(name);
    const std::string temp = target + ".tmp";
    std::FILE *f = std::fopen(temp.c_str(), "wb");
    if (f == nullptr) return false;
    writeU32(f, (uint32_t)records.size());
    for (size_t i = 0; i < records.size(); ++i) {
        writeU32(f, (uint32_t)records[i].size());
        if (!records[i].empty()) {
            std::fwrite(records[i].data(), 1, records[i].size(), f);
        }
    }
    const bool wroteCleanly = std::ferror(f) == 0;
    const bool closedCleanly = std::fclose(f) == 0;
    if (!wroteCleanly || !closedCleanly) {
        fs::remove(temp, ec);
        return false;
    }
    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        return false;
    }
    return true;
}

bool DirectorySaveStore::remove(const std::string &name) {
    std::error_code ec;
    bool gone = fs::remove(path(name), ec);
    return gone && !ec;
}

std::vector<std::string> DirectorySaveStore::list() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const fs::directory_entry &e : fs::directory_iterator(root_, ec)) {
        if (e.path().extension() == ".rs") names.push_back(e.path().stem().string());
    }
    return names;
}

int64_t DirectorySaveStore::lastModified(const std::string &name) {
    std::error_code ec;
    fs::file_time_type t = fs::last_write_time(path(name), ec);
    if (ec) return 0;
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               t.time_since_epoch())
        .count();
}

namespace desktop {

namespace {

DesktopResourceStack &installedResources() {
    static DesktopResourceStack resources{""};
    return resources;
}

}

DesktopResourceStack::DesktopResourceStack(const std::string &root) {
    setRoot(root);
}

void DesktopResourceStack::setRoot(const std::string &root) {
    stack_.clear();
    archives_.clear();
    override_ = DirectoryLayer(root + "/override");
    tree_ = DirectoryLayer(root);
    stack_.push(&override_);
    stack_.push(&tree_);

    std::error_code ec;
    std::vector<std::string> found;
    for (const fs::directory_entry &e : fs::directory_iterator(root, ec)) {
        std::string ext = e.path().extension().string();
        for (size_t i = 0; i < ext.size(); ++i) {
            if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] - 'A' + 'a');
        }
        if (ext == ".lmp") found.push_back(e.path().string());
    }
    std::sort(found.begin(), found.end());
    for (size_t i = 0; i < found.size(); ++i) {
        archives_.push_back(std::make_unique<LmpLayer>(found[i]));
        stack_.push(archives_.back().get());
    }
}

bool DesktopResourceStack::read(const std::string &name, std::vector<uint8_t> *out) {
    return stack_.read(name, out);
}

std::string DesktopResourceStack::describe() const { return stack_.describe(); }

void setResourceRoot(const std::string &dir) {
    DesktopResourceStack &resources = installedResources();
    resources.setRoot(dir);
    defaultContext()->installFileSystem(&resources);
}

void setSaveRoot(const std::string &dir) {
    static DirectorySaveStore store{"saves/rms"};
    store.setRoot(dir);
    defaultContext()->installSaveStore(&store);
}

}

}

namespace Resources {

void setRoot(const std::string &dir) { platform::desktop::setResourceRoot(dir); }

}

namespace SaveRecordFiles {

void setRoot(const std::string &dir) { platform::desktop::setSaveRoot(dir); }

}
