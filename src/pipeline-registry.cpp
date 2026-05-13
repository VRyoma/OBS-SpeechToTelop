#include "pipeline-registry.h"

void PipelineRegistry::register_pipeline(const std::string& id, Pipeline* p) {
    std::lock_guard<std::mutex> lock(mutex_);
    map_[id] = p;
}

void PipelineRegistry::unregister_pipeline(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    map_.erase(id);
}

Pipeline* PipelineRegistry::find(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(id);
    return it != map_.end() ? it->second : nullptr;
}

std::vector<std::string> PipelineRegistry::list_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(map_.size());
    for (auto& [k, _] : map_) ids.push_back(k);
    return ids;
}

PipelineRegistry& PipelineRegistry::global() {
    static PipelineRegistry instance;
    return instance;
}
