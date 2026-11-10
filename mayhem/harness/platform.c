// libobs's X11 platform layer opens a display during obs_startup(), which a headless process does not
// have. The Dockerfile leaves obs-nix-x11.c.o out of the static libobs, and this supplies its two entry
// points with a platform that has no input devices.
#include <obs-internal.h>
#include <obs-nix.h>

void obs_nix_x11_log_info(void) {}

static bool init(struct obs_core_hotkeys *hotkeys)
{
	(void)hotkeys;
	return true;
}

static void release(struct obs_core_hotkeys *hotkeys)
{
	(void)hotkeys;
}

static bool is_pressed(obs_hotkeys_platform_t *context, obs_key_t key)
{
	(void)context;
	(void)key;
	return false;
}

static void key_to_str(obs_key_t key, struct dstr *dstr)
{
	(void)key;
	dstr_copy(dstr, "Unknown");
}

static obs_key_t key_from_virtual_key(int sym)
{
	(void)sym;
	return OBS_KEY_NONE;
}

static int key_to_virtual_key(obs_key_t key)
{
	(void)key;
	return 0;
}

static const struct obs_nix_hotkeys_vtable headless_hotkeys_vtable = {
	.init = init,
	.free = release,
	.is_pressed = is_pressed,
	.key_to_str = key_to_str,
	.key_from_virtual_key = key_from_virtual_key,
	.key_to_virtual_key = key_to_virtual_key,
};

const struct obs_nix_hotkeys_vtable *obs_nix_x11_get_hotkeys_vtable(void)
{
	return &headless_hotkeys_vtable;
}
