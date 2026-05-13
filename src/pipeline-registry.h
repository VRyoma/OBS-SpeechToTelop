#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "pipeline.h"

class PipelineRegistry {
public:
    void register_pipeline(const std::string& id, Pipeline* p);
    void unregister_pipeline(const std::string& id);
    Pipeline* find(const std::string& id) const;
    std::vector<std::string> list_ids() const;

    static PipelineRegistry& global();

private:
    mutable std::mutex mutex_;
    std::map<std::string, Pipeline*> map_;
};
