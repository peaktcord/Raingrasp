#include "src/common/platform/platform.hpp"

namespace platform {

std::string normalise(const std::string &name) {
    std::string out;
    out.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (c == '\\') c = '/';
        if (c == '/' && (out.empty() || out[out.size() - 1] == '/')) continue;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out += c;
    }
    while (!out.empty() && out[out.size() - 1] == '/') out.resize(out.size() - 1);
    return out;
}

void ResourceStack::push(FileLayer *layer) {
    if (layer != nullptr) layers_.push_back(layer);
}

void ResourceStack::clear() { layers_.clear(); }

bool ResourceStack::read(const std::string &name, std::vector<uint8_t> *out) {
    for (size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i]->read(name, out)) return true;
    }
    return false;
}

std::string ResourceStack::describe() const {
    std::string out = "stack[";
    for (size_t i = 0; i < layers_.size(); ++i) {
        if (i != 0) out += ", ";
        out += layers_[i]->describe();
    }
    return out + "]";
}

namespace {
PlatformContext g_defaultContext;
}

PlatformContext *defaultContext() { return &g_defaultContext; }

}
