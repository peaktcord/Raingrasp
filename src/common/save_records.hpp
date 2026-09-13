#ifndef COMMON_SAVE_RECORDS_HPP
#define COMMON_SAVE_RECORDS_HPP

#include "src/common/platform/platform.hpp"
#include "src/common/runtime.hpp"

namespace SaveRecordFiles {
void setRoot(const std::string &dir);
}

class SaveRecords {
    platform::SaveStore *store_ = nullptr;
    std::string name_;
    std::vector<std::vector<uint8_t>> records_;
    bool dirty_ = false;
    SaveRecords(platform::SaveStore *store, std::string name,
                std::vector<std::vector<uint8_t>> recs)
        : store_(store), name_(std::move(name)), records_(std::move(recs)) {}
    void persist();
public:
    static SaveRecords *open(platform::PlatformContext *context,
                                        const std::string &name, bool create);
    // Begin a replacement transaction without reading the existing records.
    // The previous store remains intact until close() atomically persists the
    // newly added records.
    static SaveRecords *replace(platform::PlatformContext *context,
                                const std::string &name);
    static void remove(platform::PlatformContext *context,
                                  const std::string &name);
    static SharedArray<std::string> list(platform::PlatformContext *context);

    int32_t recordCount() const { return (int32_t)records_.size(); }
    int32_t add(const SharedArray<int8_t> &data, int32_t off, int32_t len) {
        std::vector<uint8_t> rec((size_t)len);
        for (int32_t i = 0; i < len; ++i) rec[(size_t)i] = (uint8_t)data[off + i];
        records_.push_back(std::move(rec));
        dirty_ = true;
        return (int32_t)records_.size();
    }
    SharedArray<int8_t> get(int32_t id) {
        const std::vector<uint8_t> &rec = records_.at((size_t)(id - 1));
        SharedArray<int8_t> out((int32_t)rec.size());
        for (int32_t i = 0; i < out.length(); ++i) out[i] = (int8_t)rec[(size_t)i];
        return out;
    }
    int32_t get(int32_t id, SharedArray<int8_t> &buf, int32_t off) {
        const std::vector<uint8_t> &rec = records_.at((size_t)(id - 1));
        for (size_t i = 0; i < rec.size(); ++i) buf[off + (int32_t)i] = (int8_t)rec[i];
        return (int32_t)rec.size();
    }
    int64_t lastModified() const;
    void close() {
        if (dirty_) persist();
        dirty_ = false;
    }
};

#endif
