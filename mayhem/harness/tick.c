// Stands in for libobs's graphics thread, which a headless process never starts. Request batches in
// `SERIAL_FRAME` mode block until that thread calls the tick callbacks, so without frame ticks any
// such batch would wait forever. This runs the same loop as tick_sources() in libobs/obs-video.c.
#include <obs-internal.h>
#include <stdlib.h>

#include "env.h"

void HarnessTickFrames(void)
{
	pthread_mutex_lock(&obs->data.draw_callbacks_mutex);

	for (size_t i = obs->data.tick_callbacks.num; i > 0; i--) {
		struct tick_callback *callback = obs->data.tick_callbacks.array + (i - 1);
		callback->tick(callback->param, 0.016f);
	}

	pthread_mutex_unlock(&obs->data.draw_callbacks_mutex);
}

// The plugin reads its persistent data through obs_module_config_path(), which needs a module handle.
static struct obs_module harness_module = {.mod_name = "obs-websocket"};

obs_module_t *obs_current_module(void)
{
	return &harness_module;
}

// libobs's main canvas has a video mix only after obs_reset_video(), which needs a graphics module and
// a GPU. Requests such as GetStats read the mix's video output unconditionally, so the headless
// session gets a bare mix that owns a real (CPU-only) video output and nothing else.
void HarnessVideoStart(void)
{
	struct video_output_info info = {
		.name = "harness",
		.format = VIDEO_FORMAT_NV12,
		.fps_num = 30,
		.fps_den = 1,
		.width = 1280,
		.height = 720,
		.cache_size = 6,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
	};
	struct obs_core_video_mix *mix = bzalloc(sizeof(*mix));
	if (video_output_open(&mix->video, &info) != VIDEO_OUTPUT_SUCCESS)
		abort();
	obs->data.main_canvas->mix = mix;
}

void HarnessVideoStop(void)
{
	struct obs_core_video_mix *mix = obs->data.main_canvas->mix;
	obs->data.main_canvas->mix = NULL;
	video_output_close(mix->video);
	bfree(mix);
}
