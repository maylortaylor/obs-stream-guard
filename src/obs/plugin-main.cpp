#include <curl/curl.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include "obs/controller.hpp"
#include "obs/dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static sg::Controller *controller = nullptr;

bool obs_module_load(void)
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
	controller = new sg::Controller();
	obs_frontend_add_dock_by_id("obs-stream-guard", "Stream Guard", sg::createDock(controller));
	obs_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	delete controller;
	controller = nullptr;
	curl_global_cleanup();
	obs_log(LOG_INFO, "plugin unloaded");
}
