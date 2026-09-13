#include "src/common/save_records.hpp"

#include <stdexcept>

#include "src/common/platform/platform.hpp"

namespace {

platform::SaveStore *store(platform::PlatformContext *context) {
    platform::SaveStore *s = context->saveStore();
    if (s == nullptr) throw std::runtime_error("SaveRecordsError: no save store installed");
    return s;
}

}

SaveRecords *SaveRecords::open(platform::PlatformContext *context,
                                          const std::string &name, bool create) {
    platform::SaveStore *saveStore = store(context);
    std::string key = name;
    std::vector<std::vector<uint8_t>> recs;
    if (saveStore->load(key, &recs)) {
        return new SaveRecords(saveStore, key, std::move(recs));
    }
    if (!create) throw std::runtime_error(std::string("SaveRecordsNotFound: ") + name);
    if (!saveStore->save(key, recs)) {
        throw std::runtime_error(std::string("SaveRecordsError: cannot create ") + name);
    }
    return new SaveRecords(saveStore, key, std::move(recs));
}

SaveRecords *SaveRecords::replace(platform::PlatformContext *context,
                                  const std::string &name) {
    return new SaveRecords(store(context), name, {});
}

void SaveRecords::persist() {
    if (!store_->save(name_, records_)) {
        throw std::runtime_error("SaveRecordsError: cannot write store");
    }
}

void SaveRecords::remove(platform::PlatformContext *context,
                                    const std::string &name) {
    if (!store(context)->remove(name)) {
        throw std::runtime_error(std::string("SaveRecordsNotFound: ") + name);
    }
}

SharedArray<std::string> SaveRecords::list(platform::PlatformContext *context) {
    std::vector<std::string> names = store(context)->list();
    if (names.empty()) return SharedArray<std::string>();
    SharedArray<std::string> out((int32_t)names.size());
    for (int32_t i = 0; i < out.length(); ++i) out[i] = std::string(names[(size_t)i]);
    return out;
}

int64_t SaveRecords::lastModified() const {
    return (int64_t)store_->lastModified(name_);
}
