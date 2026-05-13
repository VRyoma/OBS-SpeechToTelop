#include "model-downloader.h"
#include <obs-module.h>
#include <curl/curl.h>
#include <fstream>
#include <cstring>

const ModelInfo kKnownModels[] = {
    {"tiny",     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin",      "ggml-tiny.bin",      75572328u},
    {"base",     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",      "ggml-base.bin",     147951465u},
    {"small",    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin",     "ggml-small.bin",    487601225u},
    {"medium",   "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin",    "ggml-medium.bin",  1533763059u},
    {"large-v3", "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin",  "ggml-large-v3.bin",3094623691u},
};
const size_t kKnownModelsCount = 5;

std::string model_data_dir() {
    char* dir = obs_module_get_config_path(obs_current_module(), "models");
    std::string result = dir ? dir : "";
    bfree(dir);
    return result;
}

std::string model_path_for(const char* model_name) {
    for (size_t i = 0; i < kKnownModelsCount; ++i) {
        if (strcmp(kKnownModels[i].name, model_name) == 0)
            return model_data_dir() + "/" + kKnownModels[i].filename;
    }
    return "";
}

struct DlState {
    std::ofstream file;
    std::function<void(int64_t, int64_t)> cb;
};

static size_t write_data(void* ptr, size_t size, size_t nmemb, DlState* s) {
    size_t bytes = size * nmemb;
    s->file.write(static_cast<const char*>(ptr), bytes);
    return bytes;
}

static int xfer_info(DlState* s, curl_off_t total, curl_off_t done, curl_off_t, curl_off_t) {
    if (s->cb) s->cb(static_cast<int64_t>(done), static_cast<int64_t>(total));
    return 0;
}

bool download_file(const std::string& url, const std::string& dest_path,
                   std::function<void(int64_t, int64_t)> progress_cb) {
    DlState state;
    state.file.open(dest_path, std::ios::binary);
    state.cb = std::move(progress_cb);
    if (!state.file.is_open()) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_info);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    return rc == CURLE_OK && http_code == 200;
}
