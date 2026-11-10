// Headless stand-in for the parts of src/obs-websocket.cpp (the OBS plugin entry point) that the
// protocol code reaches through its global accessors. The real file needs Qt Designer output
// (ui_ConnectInfo.h) and the OBS frontend, neither of which exists in a harness.
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <memory>
#include <obs-module.h>
#include <util/platform.h>

#include "env.hpp"
#include "obs-websocket.h"
#include "Config.h"
#include "WebSocketApi.h"
#include "websocketserver/WebSocketServer.h"

static os_cpu_usage_info_t *_cpuUsageInfo = nullptr;
static ConfigPtr _config;
static WebSocketApiPtr _webSocketApi;
static WebSocketServerPtr _webSocketServer;

os_cpu_usage_info_t *GetCpuUsageInfo()
{
	return _cpuUsageInfo;
}

ConfigPtr GetConfig()
{
	return _config;
}

EventHandlerPtr GetEventHandler()
{
	return nullptr;
}

WebSocketApiPtr GetWebSocketApi()
{
	return _webSocketApi;
}

WebSocketServerPtr GetWebSocketServer()
{
	return _webSocketServer;
}

bool IsDebugEnabled()
{
	return !_config || _config->DebugEnabled;
}

bool IsServerOffBySafeMode()
{
	return false;
}

void ClearServerOffBySafeMode() {}

extern "C" const char *obs_module_text(const char *lookup)
{
	return lookup;
}

static std::string scratchDir;

static void silentLog(int, const char *, va_list, void *) {}

static void runUiTaskInline(obs_task_t task, void *param, bool)
{
	task(param);
}

void HarnessObsStart()
{
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::remove_all(scratchDir, ec);
	fs::create_directories(scratchDir + "/config/obs-websocket", ec);
	fs::create_directories(scratchDir + "/profile", ec);
	if (!getenv("HARNESS_VERBOSE")) base_set_log_handler(silentLog, nullptr);
	obs_startup("en-US", (scratchDir + "/config").c_str(), nullptr);
	obs_set_ui_task_handler(runUiTaskInline);
	_cpuUsageInfo = os_cpu_usage_info_start();
	HarnessVideoStart();
	HarnessRegisterTypes();
	HarnessFrontendStart(scratchDir);
	_webSocketApi = std::make_shared<WebSocketApi>();
	_webSocketApi->SetObsReady(true);
	_webSocketServer->SetObsReady(true);
}

static bool removeSource(void *, obs_source_t *source)
{
	obs_source_remove(source);
	return true;
}

static bool removeCanvas(void *, obs_canvas_t *canvas)
{
	if (!(obs_canvas_get_flags(canvas) & MAIN))
		obs_canvas_remove(canvas);
	return true;
}

void HarnessObsStop()
{
	_webSocketApi.reset();
	HarnessFrontendStop();
	obs_enum_scenes(removeSource, nullptr);
	obs_enum_sources(removeSource, nullptr);
	obs_enum_canvases(removeCanvas, nullptr);
	HarnessVideoStop();
	os_cpu_usage_info_destroy(_cpuUsageInfo);
	_cpuUsageInfo = nullptr;
	obs_shutdown();
}

void HarnessEnvInit()
{
	const char *tmp = getenv("TMPDIR");
	std::string pattern = std::string(tmp && *tmp ? tmp : "/tmp") + "/obsws-fuzz-XXXXXX";
	if (!mkdtemp(pattern.data()))
		abort();
	scratchDir = pattern;
	_config = std::make_shared<Config>();
	_config->AlertsEnabled = false;
	_config->AuthRequired = false;
	_webSocketServer = std::make_shared<WebSocketServer>();
}

Config &HarnessConfig()
{
	return *_config;
}

WebSocketServer &HarnessServer()
{
	return *_webSocketServer;
}
