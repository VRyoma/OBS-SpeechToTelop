#include "audio-filter.h"
#include "audio-resampler.h"
#include "pipeline.h"
#include "pipeline-registry.h"
#include "stt-engine.h"
#include "stt-whisper.h"
#include "stt-cloud.h"
#include "model-downloader.h"
#include <obs-module.h>
#include <atomic>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <util/platform.h>

static std::atomic<int> s_instance_counter{0};

struct AudioFilterData {
    std::string pipeline_id;
    Pipeline pipeline;
    std::unique_ptr<AudioResampler> resampler;
    std::unique_ptr<SttEngine> engine;
    obs_source_t* source = nullptr;
};

static const char* get_name(void*) {
    return obs_module_text("AudioFilter.Name");
}

static void* filter_create(obs_data_t* settings, obs_source_t* source) {
    auto* d = new AudioFilterData();
    d->source = source;
    // Use filter's own name as pipeline ID — stable across restarts.
    const char* filter_name = obs_source_get_name(source);
    d->pipeline_id = (filter_name && *filter_name)
        ? filter_name
        : "SpeechToTelop-" + std::to_string(s_instance_counter++);
    ++s_instance_counter;

    struct obs_audio_info ai;
    obs_get_audio_info(&ai);
    uint32_t sample_rate = ai.samples_per_sec;
    uint32_t channels = (ai.speakers == SPEAKERS_MONO) ? 1u : 2u;
    d->resampler = std::make_unique<AudioResampler>(sample_rate, channels);

    // Capture settings needed by factory (obs_data values are only valid on main thread).
    const char* backend_str  = obs_data_get_string(settings, "stt_backend");
    const char* model_name   = obs_data_get_string(settings, "model");
    const char* custom_path  = obs_data_get_string(settings, "model_path");
    const char* lang_str     = obs_data_get_string(settings, "language");

    std::string backend_s    = backend_str  ? backend_str  : "local";
    std::string model_name_s = model_name  ? model_name   : "base";
    std::string custom_path_s = custom_path ? custom_path : "";
    std::string lang_s       = lang_str    ? lang_str     : "ja";
    std::string api_key_s    = obs_data_get_string(settings, "api_key")
                               ? obs_data_get_string(settings, "api_key") : "";

    // Resolve paths on main thread (obs_current_module() is thread-local).
    std::string resolved_model_path = custom_path_s.empty()
        ? model_path_for(model_name_s.c_str())
        : custom_path_s;
    std::string resolved_data_dir = model_data_dir();

    Pipeline* pipeline_ptr = &d->pipeline;

    blog(LOG_INFO, "[SpeechToTelop] filter_create: backend=%s model_path=%s",
         backend_s.c_str(), resolved_model_path.c_str());

    // Factory runs on engine thread — no blocking work on the UI thread.
    SttEngine::BackendFactory factory;
    if (backend_s == "cloud") {
        factory = [api_key_s]() -> std::unique_ptr<ISttBackend> {
            SttCloud::Config cfg;
            cfg.api_key = api_key_s;
            return std::make_unique<SttCloud>(std::move(cfg));
        };
    } else {
        factory = [resolved_model_path, resolved_data_dir, model_name_s, lang_s, pipeline_ptr]()
                  -> std::unique_ptr<ISttBackend> {
            std::string model_path = resolved_model_path;

            // Ensure directory exists (recursive).
            if (!resolved_data_dir.empty())
                os_mkdirs(resolved_data_dir.c_str());

            if (!std::ifstream(model_path).good()) {
                auto update = [pipeline_ptr](const std::string& msg) {
                    std::lock_guard<std::mutex> lock(pipeline_ptr->text_mutex);
                    pipeline_ptr->latest_text = msg;
                    pipeline_ptr->text_updated = true;
                };
                update("モデルをダウンロード中...");
                bool ok = false;
                for (size_t i = 0; i < kKnownModelsCount; ++i) {
                    if (kKnownModels[i].name == model_name_s) {
                        ok = download_file(kKnownModels[i].url, model_path,
                            [&update](int64_t done, int64_t total) {
                                update("ダウンロード中: " +
                                    std::to_string(done / 1024 / 1024) + "/" +
                                    std::to_string(total / 1024 / 1024) + " MB");
                            });
                        break;
                    }
                }
                if (!ok)
                    throw std::runtime_error("モデルのダウンロードに失敗しました: " + model_path);
            }

            SttWhisper::Config cfg;
            cfg.model_path = model_path;
            cfg.language   = lang_s;
            return std::make_unique<SttWhisper>(std::move(cfg));
        };
    }

    SttEngine::Config eng_cfg;
    long long speed_val = obs_data_get_int(settings, "speed_accuracy");
    eng_cfg.chunk_ms = static_cast<int>(speed_val > 0 ? speed_val : 2) * 1000;
    const char* mode = obs_data_get_string(settings, "display_mode");
    eng_cfg.instant_mode = (mode && strcmp(mode, "instant") == 0);
    long long timeout_s = obs_data_get_int(settings, "buffer_timeout");
    eng_cfg.text_timeout_ms = static_cast<int>(timeout_s > 0 ? timeout_s : 5) * 1000;

    d->engine = std::make_unique<SttEngine>(&d->pipeline, std::move(factory), eng_cfg);
    d->engine->start();

    PipelineRegistry::global().register_pipeline(d->pipeline_id, &d->pipeline);
    return d;
}

static void filter_destroy(void* data) {
    auto* d = static_cast<AudioFilterData*>(data);
    PipelineRegistry::global().unregister_pipeline(d->pipeline_id);
    d->engine->stop();
    delete d;
}

static struct obs_audio_data* filter_audio(void* data, struct obs_audio_data* audio) {
    auto* d = static_cast<AudioFilterData*>(data);
    if (!audio || !audio->frames) return audio;

    uint32_t channels = audio_output_get_channels(obs_get_audio());
    if (channels == 0) channels = 1;

    std::vector<float> interleaved(audio->frames * channels);
    for (uint32_t f = 0; f < audio->frames; ++f) {
        for (uint32_t c = 0; c < channels; ++c) {
            float* ch = reinterpret_cast<float*>(audio->data[c]);
            interleaved[f * channels + c] = (ch && c < MAX_AV_PLANES) ? ch[f] : 0.f;
        }
    }

    auto resampled = d->resampler->process(interleaved.data(), audio->frames);
    for (float s : resampled)
        d->pipeline.pcm_ring->push(s);

    return audio;
}

static void get_defaults(obs_data_t* settings) {
    obs_data_set_default_string(settings, "stt_backend", "local");
    obs_data_set_default_string(settings, "model", "base");
    obs_data_set_default_string(settings, "model_path", "");
    obs_data_set_default_int(settings, "speed_accuracy", 2);
    obs_data_set_default_string(settings, "language", "ja");
    obs_data_set_default_string(settings, "display_mode", "sentence");
    obs_data_set_default_int(settings, "buffer_timeout", 5);
    obs_data_set_default_string(settings, "api_key", "");
}

static obs_properties_t* get_properties(void*) {
    obs_properties_t* props = obs_properties_create();

    obs_property_t* backend = obs_properties_add_list(props, "stt_backend",
        obs_module_text("STT.Backend"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(backend, obs_module_text("STT.Backend.Local"), "local");
    obs_property_list_add_string(backend, obs_module_text("STT.Backend.Cloud"), "cloud");

    obs_property_t* model = obs_properties_add_list(props, "model",
        obs_module_text("STT.Model"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(model, "tiny (~75MB)",      "tiny");
    obs_property_list_add_string(model, "base (~142MB)",     "base");
    obs_property_list_add_string(model, "small (~466MB)",    "small");
    obs_property_list_add_string(model, "medium (~1.5GB)",   "medium");
    obs_property_list_add_string(model, "large-v3 (~2.9GB)", "large-v3");

    obs_properties_add_path(props, "model_path",
        obs_module_text("STT.ModelPath"), OBS_PATH_FILE, "*.bin", nullptr);

    obs_properties_add_int_slider(props, "speed_accuracy",
        obs_module_text("STT.Speed"), 1, 10, 1);

    obs_property_t* lang = obs_properties_add_list(props, "language",
        obs_module_text("STT.Language"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(lang, "日本語", "ja");
    obs_property_list_add_string(lang, "English", "en");

    obs_property_t* mode_prop = obs_properties_add_list(props, "display_mode",
        obs_module_text("STT.DisplayMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(mode_prop, obs_module_text("STT.DisplayMode.Sentence"), "sentence");
    obs_property_list_add_string(mode_prop, obs_module_text("STT.DisplayMode.Instant"),  "instant");

    obs_properties_add_int_slider(props, "buffer_timeout",
        obs_module_text("STT.BufferTimeout"), 1, 30, 1);

    obs_properties_add_text(props, "api_key",
        obs_module_text("STT.APIKey"), OBS_TEXT_PASSWORD);

    return props;
}

static struct obs_source_info s_audio_filter_info = {};

void audio_filter_register() {
    s_audio_filter_info.id             = "speech_to_telop_filter";
    s_audio_filter_info.type           = OBS_SOURCE_TYPE_FILTER;
    s_audio_filter_info.output_flags   = OBS_SOURCE_AUDIO;
    s_audio_filter_info.get_name       = get_name;
    s_audio_filter_info.create         = filter_create;
    s_audio_filter_info.destroy        = filter_destroy;
    s_audio_filter_info.filter_audio   = filter_audio;
    s_audio_filter_info.get_defaults   = get_defaults;
    s_audio_filter_info.get_properties = get_properties;
    obs_register_source(&s_audio_filter_info);
}
