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

static TelopStyle style_from_settings(obs_data_t* settings) {
    TelopStyle s;
    const char* font = obs_data_get_string(settings, "font");
    if (font && *font) s.font_family = font;
    s.font_size   = static_cast<float>(obs_data_get_int(settings, "font_size"));
    s.outline     = obs_data_get_bool(settings, "outline");
    s.outline_thickness = static_cast<float>(obs_data_get_int(settings, "outline_thickness"));
    uint32_t oc = static_cast<uint32_t>(obs_data_get_int(settings, "outline_color"));
    s.outline_color = oc | 0xFF000000u;  // ensure alpha=FF
    s.shadow      = obs_data_get_bool(settings, "shadow");
    s.per_char_color = obs_data_get_bool(settings, "per_char_color");
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

    return d;
}

static void source_destroy(void* data) {
    delete static_cast<TelopSourceData*>(data);
}

static void video_tick(void* data, float seconds) {
    auto* d = static_cast<TelopSourceData*>(data);
    d->animation->tick(seconds);

    Pipeline* pipeline = PipelineRegistry::global().find(d->linked_pipeline_id);
    if (!pipeline) return;

    std::lock_guard<std::mutex> lock(pipeline->text_mutex);
    if (pipeline->text_updated) {
        pipeline->text_updated = false;
        d->displayed_text = pipeline->latest_text;
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
    obs_data_set_default_string(settings, "position", "bottom_center");
    obs_data_set_default_bool(settings, "outline", true);
    obs_data_set_default_int(settings, "outline_thickness", 3);
    obs_data_set_default_int(settings, "outline_color", 0x000000);
    obs_data_set_default_bool(settings, "shadow", true);
    obs_data_set_default_bool(settings, "per_char_color", false);
    obs_data_set_default_bool(settings, "pop_in", false);
    obs_data_set_default_string(settings, "enter_exit", "fade");
    obs_data_set_default_double(settings, "anim_duration", 0.5);
    obs_data_set_default_double(settings, "display_duration", 5.0);
}

static bool populate_pipeline_list(obs_properties_t*, obs_property_t* prop, void*) {
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
    populate_pipeline_list(props, pipeline_prop, data);

    obs_properties_add_text(props, "font", obs_module_text("Telop.Font"), OBS_TEXT_DEFAULT);
    obs_properties_add_int_slider(props, "font_size",
        obs_module_text("Telop.FontSize"), 12, 120, 1);

    obs_property_t* pos = obs_properties_add_list(props, "position",
        obs_module_text("Telop.Position"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(pos, "Bottom Center", "bottom_center");
    obs_property_list_add_string(pos, "Top Center",    "top_center");
    obs_property_list_add_string(pos, "Center",        "center");

    obs_properties_add_bool(props, "outline",  obs_module_text("Telop.Outline"));
    obs_properties_add_int_slider(props, "outline_thickness",
        obs_module_text("Telop.OutlineThickness"), 0, 20, 1);
    obs_properties_add_color(props, "outline_color", obs_module_text("Telop.OutlineColor"));
    obs_properties_add_bool(props, "shadow",   obs_module_text("Telop.Shadow"));
    obs_properties_add_bool(props, "per_char_color", obs_module_text("Telop.PerCharColor"));
    obs_properties_add_bool(props, "pop_in",   obs_module_text("Telop.PopIn"));

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
    s_telop_source_info.get_defaults = get_defaults;
    s_telop_source_info.get_properties = get_properties;
    obs_register_source(&s_telop_source_info);
}
