#include "stt-cloud.h"
#include <curl/curl.h>
#include <cstring>
#include <vector>
#include <cstdint>
#include <thread>
#include <chrono>
#include <cmath>

static size_t write_cb(char* ptr, size_t size, size_t nmemb, std::string* out) {
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::vector<uint8_t> SttCloud::encode_wav(const float* samples, size_t n) {
    uint32_t data_size = static_cast<uint32_t>(n * sizeof(int16_t));
    uint32_t file_size = 44 + data_size;
    std::vector<uint8_t> wav(file_size);

    auto w32 = [&](size_t off, uint32_t v) { memcpy(&wav[off], &v, 4); };
    auto w16 = [&](size_t off, uint16_t v) { memcpy(&wav[off], &v, 2); };

    memcpy(&wav[0], "RIFF", 4);
    w32(4, file_size - 8);
    memcpy(&wav[8], "WAVE", 4);
    memcpy(&wav[12], "fmt ", 4);
    w32(16, 16);        // chunk size
    w16(20, 1);         // PCM format
    w16(22, 1);         // mono
    w32(24, 16000);     // sample rate
    w32(28, 32000);     // byte rate = sample_rate * block_align
    w16(32, 2);         // block align = channels * bits/8
    w16(34, 16);        // bits per sample
    memcpy(&wav[36], "data", 4);
    w32(40, data_size);

    for (size_t i = 0; i < n; ++i) {
        float f = samples[i];
        if (f > 1.f) f = 1.f;
        if (f < -1.f) f = -1.f;
        int16_t s = static_cast<int16_t>(f * 32767.f);
        memcpy(&wav[44 + i * 2], &s, 2);
    }
    return wav;
}

SttCloud::SttCloud(Config cfg) : cfg_(std::move(cfg)) {}

SttResult SttCloud::transcribe(const float* samples, size_t num_samples) {
    for (int attempt = 0; attempt < cfg_.max_retries; ++attempt) {
        std::string resp = post_to_api(samples, num_samples);
        if (!resp.empty()) {
            return {parse_text_from_json(resp), true};
        }
        // Exponential backoff: 1s, 2s, 4s
        std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
    }
    return {"[...]", false};
}

std::string SttCloud::post_to_api(const float* samples, size_t num_samples) {
    auto wav = encode_wav(samples, num_samples);
    std::string body;

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_data(part, reinterpret_cast<const char*>(wav.data()), wav.size());
    curl_mime_filename(part, "audio.wav");
    curl_mime_type(part, "audio/wav");

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "model");
    curl_mime_data(part, "whisper-1", CURL_ZERO_TERMINATED);

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "language");
    curl_mime_data(part, cfg_.language.c_str(), CURL_ZERO_TERMINATED);

    std::string auth = "Authorization: Bearer " + cfg_.api_key;
    struct curl_slist* headers = curl_slist_append(nullptr, auth.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/audio/transcriptions");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, cfg_.timeout_s);

    CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || http_code != 200) return "";
    return body;
}

std::string SttCloud::parse_text_from_json(const std::string& json) {
    auto pos = json.find("\"text\"");
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 6);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}
