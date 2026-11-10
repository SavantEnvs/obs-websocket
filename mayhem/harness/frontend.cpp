// A minimal OBS frontend for the harness. obs-websocket's requests (scenes, transitions, profiles,
// record/stream state, canvases, persistent data) all go through obs-frontend-api, whose calls
// only do anything once a frontend has registered its callbacks; the real one is OBS Studio's Qt UI.
#include <filesystem>
#include <string>
#include <vector>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <obs-frontend-internal.hpp>
#include <util/config-file.h>
#include <util/darray.h>

#include "env.hpp"

namespace {

struct MockFrontend : obs_frontend_callbacks {
	std::string dir;
	OBSSourceAutoRelease currentScene;
	OBSSourceAutoRelease previewScene;
	OBSSourceAutoRelease currentTransition;
	std::vector<OBSSourceAutoRelease> transitions;
	int transitionDuration = 300;
	int tbarPosition = 0;
	bool studioMode = false;
	bool previewEnabled = true;
	bool streaming = false;
	bool recording = false;
	bool recordingPaused = false;
	bool replayBuffer = false;
	bool virtualCam = false;
	std::string collection = "Untitled";
	std::string profile = "Untitled";
	config_t *profileConfig = nullptr;
	config_t *appConfig = nullptr;
	config_t *userConfig = nullptr;
	OBSServiceAutoRelease service;

	explicit MockFrontend(const std::string &scratch) : dir(scratch)
	{
		profileConfig = newConfig("profile.ini");
		appConfig = newConfig("app.ini");
		userConfig = newConfig("user.ini");
		transitions.reserve(2);
		for (const char *name : {"Cut", "Fade"}) {
			transitions.emplace_back(obs_source_create_private("fuzz_transition", name, nullptr));
		}
		currentTransition = obs_source_get_ref(transitions[0]);
		service = obs_service_create_private("rtmp_custom", "default_service", nullptr);
	}

	~MockFrontend() override
	{
		config_close(profileConfig);
		config_close(appConfig);
		config_close(userConfig);
	}

	config_t *newConfig(const char *name)
	{
		config_t *config = nullptr;
		config_open(&config, (dir + "/" + name).c_str(), CONFIG_OPEN_ALWAYS);
		return config;
	}

	static void pushSource(obs_frontend_source_list *list, obs_source_t *source)
	{
		obs_source_t *ref = obs_source_get_ref(source);
		if (ref)
			da_push_back(list->sources, &ref);
	}

	void *obs_frontend_get_main_window() override { return nullptr; }
	void *obs_frontend_get_main_window_handle() override { return nullptr; }
	void *obs_frontend_get_system_tray() override { return nullptr; }

	void obs_frontend_get_scenes(obs_frontend_source_list *sources) override
	{
		obs_enum_scenes(
			[](void *param, obs_source_t *scene) {
				pushSource(static_cast<obs_frontend_source_list *>(param), scene);
				return true;
			},
			sources);
	}
	obs_source_t *obs_frontend_get_current_scene() override { return obs_source_get_ref(currentScene); }
	void obs_frontend_set_current_scene(obs_source_t *scene) override { currentScene = obs_source_get_ref(scene); }

	void obs_frontend_get_transitions(obs_frontend_source_list *sources) override
	{
		for (auto &t : transitions)
			pushSource(sources, t);
	}
	obs_source_t *obs_frontend_get_current_transition() override { return obs_source_get_ref(currentTransition); }
	void obs_frontend_set_current_transition(obs_source_t *transition) override
	{
		currentTransition = obs_source_get_ref(transition);
	}
	int obs_frontend_get_transition_duration() override { return transitionDuration; }
	void obs_frontend_set_transition_duration(int duration) override { transitionDuration = duration; }
	void obs_frontend_release_tbar() override {}
	int obs_frontend_get_tbar_position() override { return tbarPosition; }
	void obs_frontend_set_tbar_position(int position) override { tbarPosition = position; }

	void obs_frontend_get_scene_collections(std::vector<std::string> &strings) override { strings.push_back(collection); }
	char *obs_frontend_get_current_scene_collection() override { return bstrdup(collection.c_str()); }
	void obs_frontend_set_current_scene_collection(const char *name) override { collection = name; }
	bool obs_frontend_add_scene_collection(const char *name) override
	{
		collection = name;
		return true;
	}

	void obs_frontend_get_profiles(std::vector<std::string> &strings) override { strings.push_back(profile); }
	char *obs_frontend_get_current_profile() override { return bstrdup(profile.c_str()); }
	char *obs_frontend_get_current_profile_path() override { return bstrdup((dir + "/profile").c_str()); }
	void obs_frontend_set_current_profile(const char *name) override { profile = name; }
	void obs_frontend_create_profile(const char *name) override { profile = name; }
	void obs_frontend_duplicate_profile(const char *name) override { profile = name; }
	void obs_frontend_delete_profile(const char *) override {}

	void obs_frontend_streaming_start() override { streaming = true; }
	void obs_frontend_streaming_stop() override { streaming = false; }
	bool obs_frontend_streaming_active() override { return streaming; }
	void obs_frontend_recording_start() override { recording = true; }
	void obs_frontend_recording_stop() override
	{
		recording = false;
		recordingPaused = false;
	}
	bool obs_frontend_recording_active() override { return recording; }
	void obs_frontend_recording_pause(bool pause) override { recordingPaused = recording && pause; }
	bool obs_frontend_recording_paused() override { return recordingPaused; }
	bool obs_frontend_recording_split_file() override { return recording; }
	bool obs_frontend_recording_add_chapter(const char *) override { return recording; }
	void obs_frontend_replay_buffer_start() override { replayBuffer = true; }
	void obs_frontend_replay_buffer_save() override {}
	void obs_frontend_replay_buffer_stop() override { replayBuffer = false; }
	bool obs_frontend_replay_buffer_active() override { return replayBuffer; }

	void *obs_frontend_add_tools_menu_qaction(const char *) override { return nullptr; }
	void obs_frontend_add_tools_menu_item(const char *, obs_frontend_cb, void *) override {}
	bool obs_frontend_add_dock_by_id(const char *, const char *, void *) override { return false; }
	void obs_frontend_remove_dock(const char *) override {}
	bool obs_frontend_add_custom_qdock(const char *, void *) override { return false; }
	void obs_frontend_add_event_callback(obs_frontend_event_cb, void *) override {}
	void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *) override {}

	obs_output_t *obs_frontend_get_streaming_output() override { return nullptr; }
	obs_output_t *obs_frontend_get_recording_output() override { return nullptr; }
	obs_output_t *obs_frontend_get_replay_buffer_output() override { return nullptr; }
	config_t *obs_frontend_get_profile_config() override { return profileConfig; }
	config_t *obs_frontend_get_app_config() override { return appConfig; }
	config_t *obs_frontend_get_user_config() override { return userConfig; }

	void obs_frontend_open_projector(const char *, int, const char *, const char *) override {}
	void obs_frontend_save() override {}
	void obs_frontend_defer_save_begin() override {}
	void obs_frontend_defer_save_end() override {}
	void obs_frontend_add_save_callback(obs_frontend_save_cb, void *) override {}
	void obs_frontend_remove_save_callback(obs_frontend_save_cb, void *) override {}
	void obs_frontend_add_preload_callback(obs_frontend_save_cb, void *) override {}
	void obs_frontend_remove_preload_callback(obs_frontend_save_cb, void *) override {}
	void obs_frontend_push_ui_translation(obs_frontend_translate_ui_cb) override {}
	void obs_frontend_pop_ui_translation() override {}

	obs_service_t *obs_frontend_get_streaming_service() override { return service; }
	void obs_frontend_set_streaming_service(obs_service_t *s) override { service = obs_service_get_ref(s); }
	void obs_frontend_save_streaming_service() override {}

	bool obs_frontend_preview_program_mode_active() override { return studioMode; }
	void obs_frontend_set_preview_program_mode(bool enable) override { studioMode = enable; }
	void obs_frontend_preview_program_trigger_transition() override {}
	bool obs_frontend_preview_enabled() override { return previewEnabled; }
	void obs_frontend_set_preview_enabled(bool enable) override { previewEnabled = enable; }
	obs_source_t *obs_frontend_get_current_preview_scene() override
	{
		return studioMode ? obs_source_get_ref(previewScene) : nullptr;
	}
	void obs_frontend_set_current_preview_scene(obs_source_t *scene) override { previewScene = obs_source_get_ref(scene); }

	void on_load(obs_data_t *) override {}
	void on_preload(obs_data_t *) override {}
	void on_save(obs_data_t *) override {}
	void on_event(enum obs_frontend_event) override {}

	void obs_frontend_take_screenshot() override {}
	void obs_frontend_take_source_screenshot(obs_source_t *) override {}
	obs_output_t *obs_frontend_get_virtualcam_output() override { return nullptr; }
	void obs_frontend_start_virtualcam() override { virtualCam = true; }
	void obs_frontend_stop_virtualcam() override { virtualCam = false; }
	bool obs_frontend_virtualcam_active() override { return virtualCam; }
	void obs_frontend_reset_video() override {}

	void obs_frontend_open_source_properties(obs_source_t *) override {}
	void obs_frontend_open_source_filters(obs_source_t *) override {}
	void obs_frontend_open_source_interaction(obs_source_t *) override {}
	void obs_frontend_open_sceneitem_edit_transform(obs_sceneitem_t *) override {}

	char *obs_frontend_get_current_record_output_path() override { return bstrdup((dir + "/recordings").c_str()); }
	const char *obs_frontend_get_locale_string(const char *string) override { return string; }
	bool obs_frontend_is_theme_dark() override { return true; }
	char *obs_frontend_get_last_recording() override { return nullptr; }
	char *obs_frontend_get_last_screenshot() override { return nullptr; }
	char *obs_frontend_get_last_replay() override { return nullptr; }
	void obs_frontend_add_undo_redo_action(const char *, const undo_redo_cb, const undo_redo_cb, const char *, const char *,
					       bool) override
	{
	}

	obs_canvas_t *obs_frontend_add_canvas(const char *name, obs_video_info *ovi, int flags) override
	{
		return obs_canvas_create(name, ovi, flags);
	}
	bool obs_frontend_remove_canvas(obs_canvas_t *canvas) override
	{
		obs_canvas_remove(canvas);
		return true;
	}
	void obs_frontend_get_canvases(obs_frontend_canvas_list *list) override
	{
		obs_enum_canvases(
			[](void *param, obs_canvas_t *canvas) {
				auto *l = static_cast<obs_frontend_canvas_list *>(param);
				obs_canvas_t *ref = obs_canvas_get_ref(canvas);
				if (ref)
					da_push_back(l->canvases, &ref);
				return true;
			},
			list);
	}
	void obs_frontend_copy_sceneitem(obs_sceneitem_t *) override {}
	bool obs_frontend_can_paste_sceneitem(bool) override { return false; }
	void obs_frontend_paste_sceneitem(obs_scene_t *, bool) override {}
};

}

void HarnessFrontendStart(const std::string &scratchDir)
{
	obs_frontend_set_callbacks_internal(new MockFrontend(scratchDir));
}

// obs-frontend-api owns the callbacks object and deletes it when it is replaced.
void HarnessFrontendStop()
{
	obs_frontend_set_callbacks_internal(nullptr);
}
