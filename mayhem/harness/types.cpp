// Source kinds registered into the headless libobs. libobs ships only `scene` and `group`; the
// obs-websocket requests that work on inputs, filters, transitions and media need a kind to act on.
#include <obs.h>
#include <obs-module.h>

#include "env.hpp"

static const char *getName(void *)
{
	return "fuzz";
}

static void *createDummy(obs_data_t *, obs_source_t *)
{
	return bzalloc(1);
}

static void destroyDummy(void *data)
{
	bfree(data);
}

static void updateDummy(void *, obs_data_t *) {}

static void getDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "text", "fuzz");
	obs_data_set_default_int(settings, "count", 3);
	obs_data_set_default_double(settings, "gain", 0.5);
	obs_data_set_default_bool(settings, "enabled", true);
}

static obs_properties_t *getProperties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(props, "text", "Text", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "secret", "Secret", OBS_TEXT_PASSWORD);
	obs_properties_add_int(props, "count", "Count", -10, 100, 1);
	obs_properties_add_float(props, "gain", "Gain", 0.0, 1.0, 0.01);
	obs_properties_add_bool(props, "enabled", "Enabled");
	obs_properties_add_color(props, "color", "Color");
	obs_properties_add_path(props, "path", "Path", OBS_PATH_FILE, "*.*", nullptr);
	obs_property_t *list = obs_properties_add_list(props, "mode", "Mode", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, "A", "a");
	obs_property_list_add_string(list, "B", "b");
	obs_property_t *ilist = obs_properties_add_list(props, "level", "Level", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(ilist, "One", 1);
	obs_property_list_add_int(ilist, "Two", 2);
	obs_properties_add_button(props, "press", "Press", [](obs_properties_t *, obs_property_t *, void *) { return true; });
	return props;
}

static uint32_t getSize(void *)
{
	return 64;
}

static void mediaPlayPause(void *, bool) {}
static void mediaRestart(void *) {}
static void mediaStop(void *) {}
static void mediaNextPrev(void *) {}
static int64_t mediaDuration(void *)
{
	return 10000;
}
static int64_t mediaTime(void *)
{
	return 1000;
}
static void mediaSetTime(void *, int64_t) {}
static enum obs_media_state mediaState(void *)
{
	return OBS_MEDIA_STATE_PAUSED;
}

static void transitionRender(void *, gs_effect_t *) {}
static bool transitionAudioRender(void *, uint64_t *, struct obs_source_audio_mix *, uint32_t, size_t, size_t)
{
	return false;
}

static const char *serviceName(void *)
{
	return "Custom";
}
static const char *serviceProtocol(void *)
{
	return "RTMP";
}
static void *serviceCreate(obs_data_t *, obs_service_t *)
{
	return bzalloc(1);
}
static void serviceDestroy(void *data)
{
	bfree(data);
}

static obs_source_info baseInfo(const char *id, enum obs_source_type type, uint32_t flags)
{
	obs_source_info info = {};
	info.id = id;
	info.type = type;
	info.output_flags = flags;
	info.get_name = getName;
	info.create = createDummy;
	info.destroy = destroyDummy;
	info.update = updateDummy;
	info.get_defaults = getDefaults;
	info.get_properties = getProperties;
	info.get_width = getSize;
	info.get_height = getSize;
	return info;
}

void HarnessRegisterTypes()
{
	obs_source_info input = baseInfo("fuzz_input", OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_VIDEO);
	obs_register_source(&input);

	obs_source_info audio = baseInfo("fuzz_audio_input", OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_AUDIO);
	obs_register_source(&audio);

	obs_source_info media = baseInfo("fuzz_media_input", OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_VIDEO | OBS_SOURCE_CONTROLLABLE_MEDIA);
	media.media_play_pause = mediaPlayPause;
	media.media_restart = mediaRestart;
	media.media_stop = mediaStop;
	media.media_next = mediaNextPrev;
	media.media_previous = mediaNextPrev;
	media.media_get_duration = mediaDuration;
	media.media_get_time = mediaTime;
	media.media_set_time = mediaSetTime;
	media.media_get_state = mediaState;
	obs_register_source(&media);

	obs_source_info filter = baseInfo("fuzz_filter", OBS_SOURCE_TYPE_FILTER, OBS_SOURCE_VIDEO);
	obs_register_source(&filter);

	obs_source_info transition = baseInfo("fuzz_transition", OBS_SOURCE_TYPE_TRANSITION, OBS_SOURCE_VIDEO);
	transition.video_render = transitionRender;
	transition.audio_render = transitionAudioRender;
	transition.get_width = nullptr;
	transition.get_height = nullptr;
	obs_register_source(&transition);

	obs_service_info service = {};
	service.id = "rtmp_custom";
	service.get_name = serviceName;
	service.get_protocol = serviceProtocol;
	service.create = serviceCreate;
	service.destroy = serviceDestroy;
	obs_register_service(&service);
}
