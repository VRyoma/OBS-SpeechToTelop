#pragma once
#include <functional>
#include <string>
#include <cstdint>
#include <cstddef>

struct ModelInfo {
    const char* name;
    const char* url;
    const char* filename;
    size_t size_bytes;
};

extern const ModelInfo kKnownModels[];
extern const size_t kKnownModelsCount;

// Returns path in OBS module config directory for models.
std::string model_data_dir();
std::string model_path_for(const char* model_name);

// Download url to dest_path. progress_cb(bytes_done, bytes_total) called periodically.
bool download_file(const std::string& url,
                   const std::string& dest_path,
                   std::function<void(int64_t, int64_t)> progress_cb);
