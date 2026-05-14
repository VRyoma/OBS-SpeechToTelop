#include "telop-source.h"
#include "pipeline-registry.h"
#include "telop-renderer.h"
#include "telop-animation.h"
#include <obs-module.h>
#include <memory>
#include <string>

struct TelopSourceData {
    obs_source_t* source = nullptr;
    std::string linked_pipeline_id;
    std::string displayed_text;
    std::unique_ptr<TelopRenderer> renderer;
    std::unique_ptr<TelopAnimation> animation;
    uint32_t width  = 1920;
    uint32_t height = 200;
};

// OBS stores colors as R|(G<<8)|(B<<16)|(A<<24) — convert to ARGB used by CoreText/GDI+
static uint32_t obs_to_argb(long long val, bool opaque) {
    uint32_t r = (val >>  0) & 0xFF;
    uint32_t g = (val >>  8) & 0xFF;
    uint32_t b = (val >> 16) & 0xFF;
    uint32_t a = opaque ? 0xFF : ((val >> 24) & 0xFF);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static TelopStyle style_from_settings(obs_data_t* settings) {
    TelopStyle s;
    const char* font = obs_data_get_string(settings, "font");
    if (font && *font) s.font_family = font;
    s.font_size         = static_cast<float>(obs_data_get_int(settings, "font_size"));
    s.text_color        = obs_to_argb(obs_data_get_int(settings, "text_color"),       true);
    s.outline           = obs_data_get_bool(settings, "outline");
    s.outline_thickness = static_cast<float>(obs_data_get_int(settings, "outline_thickness"));
    s.outline_color     = obs_to_argb(obs_data_get_int(settings, "outline_color"),     true);
    s.shadow            = obs_data_get_bool(settings, "shadow");
    s.background        = obs_data_get_bool(settings, "background");
    s.background_color  = obs_to_argb(obs_data_get_int(settings, "background_color"), false);
    return s;
}

static const char* get_name(void*) {
    return obs_module_text("TelopSource.Name");
}

static void* source_create(obs_data_t* settings, obs_source_t* source) {
    auto* d = new TelopSourceData();
    d->source = source;
    d->linked_pipeline_id = obs_data_get_string(settings, "pipeline_id");

    TelopStyle style = style_from_settings(settings);
    d->renderer = std::make_unique<TelopRenderer>(style);

    float enter_s   = static_cast<float>(obs_data_get_double(settings, "anim_duration"));
    float exit_s    = enter_s;
    float display_s = static_cast<float>(obs_data_get_double(settings, "display_duration"));
    d->animation = std::make_unique<TelopAnimation>(enter_s, exit_s, display_s);

    const char* name = obs_source_get_name(source);
    d->displayed_text = (name && *name) ? name : "SpeechToTelop";
    d->animation->trigger(d->displayed_text);

    return d;
}

static void source_destroy(void* data) {
    delete static_cast<TelopSourceData*>(data);
}

static void source_update(void* data, obs_data_t* settings) {
    auto* d = static_cast<TelopSourceData*>(data);
    d->linked_pipeline_id = obs_data_get_string(settings, "pipeline_id");
    // Rebuild renderer with updated style
    TelopStyle style = style_from_settings(settings);
    d->renderer = std::make_unique<TelopRenderer>(style);
    // Force re-render by resetting text on next video_render tick
    d->animation->trigger(d->displayed_text);
}

static void video_tick(void* data, float seconds) {
    auto* d = static_cast<TelopSourceData*>(data);
    d->animation->tick(seconds);

    Pipeline* pipeline = PipelineRegistry::global().find(d->linked_pipeline_id);
    if (!pipeline) {
        // Auto-discover: fall back to the first registered pipeline.
        auto ids = PipelineRegistry::global().list_ids();
        if (!ids.empty()) {
            blog(LOG_INFO, "[SpeechToTelop] auto-discover: linked_id='%s' using '%s'",
                 d->linked_pipeline_id.c_str(), ids[0].c_str());
            d->linked_pipeline_id = ids[0];
            pipeline = PipelineRegistry::global().find(ids[0]);
        } else {
            blog(LOG_INFO, "[SpeechToTelop] video_tick: no pipelines registered yet");
        }
    }
    if (!pipeline) return;

    std::lock_guard<std::mutex> lock(pipeline->text_mutex);
    if (pipeline->text_updated) {
        pipeline->text_updated = false;
        d->displayed_text = pipeline->latest_text;
        blog(LOG_INFO, "[SpeechToTelop] video_tick: new text='%s'", d->displayed_text.c_str());
        d->animation->trigger(d->displayed_text);
    }
}

static void video_render(void* data, gs_effect_t*) {
    auto* d = static_cast<TelopSourceData*>(data);
    if (d->animation->state() == AnimState::Idle) return;

    d->renderer->update_text(d->displayed_text);

    float alpha = d->animation->alpha();
    float x = (d->width  > d->renderer->texture_width())
              ? (d->width  - d->renderer->texture_width())  / 2.f : 0.f;
    float y = (d->height > d->renderer->texture_height())
              ? (d->height - d->renderer->texture_height()) - 10.f : 0.f;
    d->renderer->render(x, y, alpha);
}

static uint32_t get_width(void* data) {
    return static_cast<TelopSourceData*>(data)->width;
}

static uint32_t get_height(void* data) {
    return static_cast<TelopSourceData*>(data)->height;
}

static void get_defaults(obs_data_t* settings) {
    obs_data_set_default_string(settings, "pipeline_id", "");
    obs_data_set_default_string(settings, "font", "Hiragino Sans");
    obs_data_set_default_int(settings, "font_size", 48);
    obs_data_set_default_int(settings, "text_color", 0x00FFFFFF);    // white in OBS ABGR
    obs_data_set_default_string(settings, "position", "bottom_center");
    obs_data_set_default_bool(settings, "outline", true);
    obs_data_set_default_int(settings, "outline_thickness", 3);
    obs_data_set_default_int(settings, "outline_color", 0x00000000);  // black in OBS ABGR
    obs_data_set_default_bool(settings, "shadow", true);
    obs_data_set_default_bool(settings, "background", false);
    obs_data_set_default_int(settings, "background_color", 0x99000000); // semi-transparent black
    obs_data_set_default_string(settings, "enter_exit", "fade");
    obs_data_set_default_double(settings, "anim_duration", 0.5);
    obs_data_set_default_double(settings, "display_duration", 5.0);
}

static bool populate_pipeline_list(void*, obs_properties_t*, obs_property_t* prop, obs_data_t*) {
    obs_property_list_clear(prop);
    obs_property_list_add_string(prop, "(none)", "");
    for (const auto& id : PipelineRegistry::global().list_ids())
        obs_property_list_add_string(prop, id.c_str(), id.c_str());
    return true;
}

static obs_properties_t* get_properties(void* data) {
    obs_properties_t* props = obs_properties_create();

    obs_property_t* pipeline_prop = obs_properties_add_list(props, "pipeline_id",
        obs_module_text("Telop.Pipeline"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_set_modified_callback2(pipeline_prop, populate_pipeline_list, data);
    populate_pipeline_list(data, props, pipeline_prop, nullptr);

    // Font selection
    obs_property_t* font_prop = obs_properties_add_list(props, "font",
        obs_module_text("Telop.Font"), OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
    // macOS fonts
    obs_property_list_add_string(font_prop, "Hiragino Sans",        "Hiragino Sans");
    obs_property_list_add_string(font_prop, "Hiragino Mincho ProN", "Hiragino Mincho ProN");
    obs_property_list_add_string(font_prop, "Hiragino Kaku Gothic Pro", "Hiragino Kaku Gothic Pro");
    // Windows fonts
    obs_property_list_add_string(font_prop, "Yu Gothic",   "Yu Gothic");
    obs_property_list_add_string(font_prop, "Meiryo",      "Meiryo");
    obs_property_list_add_string(font_prop, "MS Gothic",   "MS Gothic");
    // Universal
    obs_property_list_add_string(font_prop, "Noto Sans JP", "Noto Sans JP");
    obs_property_list_add_string(font_prop, "Arial",        "Arial");
    obs_property_list_add_string(font_prop, "Impact",       "Impact");

    obs_properties_add_int_slider(props, "font_size",
        obs_module_text("Telop.FontSize"), 12, 120, 1);
    obs_properties_add_color(props, "text_color", obs_module_text("Telop.TextColor"));

    obs_property_t* pos = obs_properties_add_list(props, "position",
        obs_module_text("Telop.Position"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(pos, "Bottom Center", "bottom_center");
    obs_property_list_add_string(pos, "Top Center",    "top_center");
    obs_property_list_add_string(pos, "Center",        "center");

    obs_property_t* outline_prop = obs_properties_add_bool(props, "outline", obs_module_text("Telop.Outline"));
    obs_properties_add_int_slider(props, "outline_thickness",
        obs_module_text("Telop.OutlineThickness"), 0, 20, 1);
    obs_properties_add_color(props, "outline_color", obs_module_text("Telop.OutlineColor"));
    obs_property_set_modified_callback(outline_prop,
        [](obs_properties_t* ps, obs_property_t*, obs_data_t* s) -> bool {
            bool on = obs_data_get_bool(s, "outline");
            obs_property_set_enabled(obs_properties_get(ps, "outline_thickness"), on);
            obs_property_set_enabled(obs_properties_get(ps, "outline_color"), on);
            return true;
        });

    obs_properties_add_bool(props, "shadow", obs_module_text("Telop.Shadow"));

    // Background (座布団)
    obs_property_t* bg_prop = obs_properties_add_bool(props, "background", obs_module_text("Telop.Background"));
    obs_properties_add_color_alpha(props, "background_color", obs_module_text("Telop.BackgroundColor"));
    obs_property_set_modified_callback(bg_prop,
        [](obs_properties_t* ps, obs_property_t*, obs_data_t* s) -> bool {
            obs_property_set_enabled(obs_properties_get(ps, "background_color"),
                                     obs_data_get_bool(s, "background"));
            return true;
        });

    obs_property_t* anim = obs_properties_add_list(props, "enter_exit",
        obs_module_text("Telop.EnterExit"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(anim, "Fade", "fade");
    obs_property_list_add_string(anim, "None", "none");

    obs_properties_add_float_slider(props, "anim_duration",
        obs_module_text("Telop.AnimDuration"), 0.0, 2.0, 0.1);
    obs_properties_add_float_slider(props, "display_duration",
        obs_module_text("Telop.DisplayDuration"), 1.0, 30.0, 0.5);

    return props;
}

static struct obs_source_info s_telop_source_info = {};

void telop_source_register() {
    s_telop_source_info.id           = "speech_to_telop_source";
    s_telop_source_info.type         = OBS_SOURCE_TYPE_INPUT;
    s_telop_source_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
    s_telop_source_info.get_name     = get_name;
    s_telop_source_info.create       = source_create;
    s_telop_source_info.destroy      = source_destroy;
    s_telop_source_info.video_tick   = video_tick;
    s_telop_source_info.video_render = video_render;
    s_telop_source_info.get_width    = get_width;
    s_telop_source_info.get_height   = get_height;
    s_telop_source_info.update        = source_update;
    s_telop_source_info.get_defaults  = get_defaults;
    s_telop_source_info.get_properties = get_properties;
    obs_register_source(&s_telop_source_info);
}
