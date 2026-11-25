#include "obs/controller.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <QDateTime>
#include <QMetaObject>

#include <util/config-file.h>
#include <util/platform.h>

#include <plugin-support.h>

#include "core/track_check.hpp"

namespace sg {

static constexpr uint32_t kWindowSeconds = 12;
static constexpr int kCheckSeconds = 10;
static constexpr double kSilenceDb = -60.0;

static double nowSeconds()
{
	return static_cast<double>(os_gettime_ns()) / 1e9;
}

static std::string configFile(const char *name)
{
	char *path = obs_module_config_path(name);
	std::string out = path ? path : "";
	bfree(path);
	return out;
}

static void ensureConfigDir()
{
	const std::string dir = configFile("");
	if (!dir.empty())
		os_mkdirs(dir.c_str());
}

static bool streaming()
{
	obs_output_t *output = obs_frontend_get_streaming_output();
	const bool active = output && obs_output_active(output);
	obs_output_release(output);
	return active;
}

Controller::Controller()
{
	ensureConfigDir();
	const std::string path = configFile("settings.json");
	obs_data_t *data = obs_data_create_from_json_file_safe(path.c_str(), "bak");
	if (data) {
		obs_data_set_default_string(data, "source", settings_.source.c_str());
		obs_data_set_default_int(data, "unmute_after", settings_.unmuteAfter);
		settings_.enabled = obs_data_get_bool(data, "enabled");
		settings_.source = obs_data_get_string(data, "source");
		settings_.stopOnMatch = obs_data_get_bool(data, "stop_on_match");
		settings_.unmuteAfter = static_cast<int>(obs_data_get_int(data, "unmute_after"));
		restoreSource_ = obs_data_get_string(data, "restore_source");
		restoreBit_ = static_cast<uint32_t>(obs_data_get_int(data, "restore_bit"));
		obs_data_release(data);
	}
	guard_.setConfig({static_cast<double>(settings_.unmuteAfter), 60.0, settings_.stopOnMatch});

	obs_frontend_add_event_callback(onFrontendEvent, this);
	connect(&timer_, &QTimer::timeout, this, [this]() { tick(); });
	timer_.start(1000);
	worker_ = std::thread([this]() { work(); });
}

Controller::~Controller()
{
	shutdown();
}

void Controller::shutdown()
{
	if (closed_)
		return;
	closed_ = true;
	timer_.stop();
	{
		std::lock_guard<std::mutex> lock(workerMutex_);
		stop_ = true;
	}
	wake_.notify_all();
	if (worker_.joinable())
		worker_.join();
	detach();
	obs_frontend_remove_event_callback(onFrontendEvent, this);
}

void Controller::saveSettings()
{
	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "enabled", settings_.enabled);
	obs_data_set_string(data, "source", settings_.source.c_str());
	obs_data_set_bool(data, "stop_on_match", settings_.stopOnMatch);
	obs_data_set_int(data, "unmute_after", settings_.unmuteAfter);
	obs_data_set_string(data, "restore_source", restoreSource_.c_str());
	obs_data_set_int(data, "restore_bit", restoreBit_);
	const std::string path = configFile("settings.json");
	if (!obs_data_save_json_safe(data, path.c_str(), "tmp", "bak"))
		obs_log(LOG_ERROR, "cannot save %s", path.c_str());
	obs_data_release(data);
}

void Controller::log(const std::string &line)
{
	const std::string stamped =
		QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString() + "  " + line;
	obs_log(LOG_INFO, "%s", line.c_str());
	events_.push_front(stamped);
	if (events_.size() > 50)
		events_.pop_back();
	std::ofstream file(std::filesystem::u8path(configFile("events.log")), std::ios::app);
	file << stamped << '\n';
}

void Controller::apply(const Settings &next)
{
	// Give the old source its track back before anything points at a new one.
	if (next.source != settings_.source && guard_.muted())
		run(guard_.manualUnmute(nowSeconds()));
	settings_ = next;
	guard_.setConfig({static_cast<double>(settings_.unmuteAfter), 60.0, settings_.stopOnMatch});
	saveSettings();
	tick();
}

void Controller::setPaused(bool paused)
{
	run(guard_.setPaused(paused, nowSeconds()));
	log(paused ? "Paused" : "Resumed");
}

void Controller::unmuteNow()
{
	log("Unmute button pressed");
	run(guard_.manualUnmute(nowSeconds()));
}

void Controller::onAudio(void *data, obs_source_t *, const struct audio_data *audio, bool)
{
	auto *self = static_cast<Controller *>(data);
	const float *planes[MAX_AV_PLANES] = {};
	const size_t channels = std::min<size_t>(audio_output_get_channels(obs_get_audio()), MAX_AV_PLANES);
	for (size_t i = 0; i < channels; i++)
		planes[i] = reinterpret_cast<const float *>(audio->data[i]);
	self->window_->push(planes, channels, audio->frames);
}

void Controller::attach(obs_source_t *source)
{
	obs_source_t *current = obs_weak_source_get_source(attached_);
	const bool same = current == source;
	obs_source_release(current);
	if (same && (source || !attached_))
		return;

	detach();
	if (!source)
		return;
	std::lock_guard<std::mutex> lock(workerMutex_);
	window_ = std::make_unique<AudioWindow>(audio_output_get_sample_rate(obs_get_audio()), kWindowSeconds);
	attached_ = obs_source_get_weak_source(source);
	obs_source_add_audio_capture_callback(source, onAudio, this);
}

void Controller::detach()
{
	obs_source_t *source = obs_weak_source_get_source(attached_);
	if (source) {
		obs_source_remove_audio_capture_callback(source, onAudio, this);
		obs_source_release(source);
	}
	obs_weak_source_release(attached_);
	attached_ = nullptr;
	std::lock_guard<std::mutex> lock(workerMutex_);
	window_.reset();
}

std::string Controller::findProblem(obs_source_t *source)
{
	config_t *profile = obs_frontend_get_profile_config();
	if (!profile)
		return "OBS profile not loaded.";
	const char *mode = config_get_string(profile, "Output", "Mode");
	const char *recType = config_get_string(profile, "AdvOut", "RecType");
	const char *recFormat = config_get_string(profile, "AdvOut", "RecFormat2");

	TrackSetup setup{};
	setup.sourceFound = source != nullptr;
	setup.advancedMode = mode && std::string(mode) == "Advanced";
	setup.flvRecording = recFormat && std::string(recFormat) == "flv";
	setup.ffmpegRecording = recType && std::string(recType) == "FFmpeg";
	setup.streamTrack = static_cast<int>(config_get_int(profile, "AdvOut", "TrackIndex"));
	setup.recordTracks = static_cast<unsigned>(config_get_int(profile, "AdvOut", "RecTracks"));
	setup.ffmpegMixes = static_cast<unsigned>(config_get_int(profile, "AdvOut", "FFAudioMixes"));
	setup.sourceMixers = source ? obs_source_get_audio_mixers(source) : 0;
	// Judge the setup as it is without our own mute.
	if (guard_.muted() && restoreSource_ == settings_.source)
		setup.sourceMixers |= restoreBit_;
	return trackProblem(setup);
}

void Controller::mute()
{
	obs_source_t *source = obs_get_source_by_name(settings_.source.c_str());
	config_t *profile = obs_frontend_get_profile_config();
	if (source && profile) {
		const auto track = config_get_int(profile, "AdvOut", "TrackIndex");
		const uint32_t bit = 1u << (std::clamp<int>(static_cast<int>(track), 1, 6) - 1);
		restoreSource_ = settings_.source;
		restoreBit_ = bit;
		// Saved first, so a crash after the mute can still be undone.
		saveSettings();
		obs_source_set_audio_mixers(source, obs_source_get_audio_mixers(source) & ~bit);
		log("Muted stream track " + std::to_string(track) + " for " + settings_.source);
	} else {
		log("MUTE FAILED: source not found");
	}
	obs_source_release(source);
}

void Controller::applyPendingRestore()
{
	if (!restoreBit_)
		return;
	obs_source_t *source = obs_get_source_by_name(restoreSource_.c_str());
	if (!source)
		return;
	obs_source_set_audio_mixers(source, obs_source_get_audio_mixers(source) | restoreBit_);
	obs_source_release(source);
	log("Stream audio restored for " + restoreSource_);
	restoreBit_ = 0;
	restoreSource_.clear();
	saveSettings();
}

void Controller::unmute()
{
	applyPendingRestore();
}

void Controller::run(const Actions &actions)
{
	for (Action action : actions) {
		if (action == Action::Mute) {
			mute();
		} else if (action == Action::Unmute) {
			unmute();
		} else {
			obs_output_t *output = obs_frontend_get_streaming_output();
			if (output && obs_output_active(output)) {
				// A forced stop throws away the delayed video instead of sending it.
				obs_output_force_stop(output);
				log("Stream stopped and delayed video discarded");
			}
			obs_output_release(output);
		}
	}
}

void Controller::tick()
{
	if (!ready_ || closed_)
		return;
	const double now = nowSeconds();
	obs_source_t *source = obs_get_source_by_name(settings_.source.c_str());
	attach(source);
	run(guard_.setBlocked(findProblem(source), now));
	run(guard_.setEnabled(settings_.enabled, now));

	if (!guard_.muted()) {
		applyPendingRestore();
	} else if (source && restoreBit_ && (obs_source_get_audio_mixers(source) & restoreBit_)) {
		// Something ticked the track again during a mute.
		obs_source_set_audio_mixers(source, obs_source_get_audio_mixers(source) & ~restoreBit_);
	}
	obs_source_release(source);
	checking_ = guard_.wantsChecks();
}

void Controller::work()
{
	uint64_t seenFrames = 0;
	std::unique_lock<std::mutex> lock(workerMutex_);
	while (!stop_) {
		wake_.wait_for(lock, std::chrono::seconds(kCheckSeconds), [this]() { return stop_.load(); });
		if (stop_)
			break;
		if (!checking_ || !window_) {
			seenFrames = 0;
			continue;
		}
		const uint64_t frames = window_->totalFrames();
		const uint32_t rate = window_->sampleRate();
		std::vector<int16_t> samples;
		double db = 0;
		const bool full = window_->snapshot(samples, db);
		const bool fresh = frames != seenFrames;
		seenFrames = frames;
		if (!full && fresh)
			continue;
		lock.unlock();

		const double capturedAt = nowSeconds();
		Recognition result;
		if (!fresh)
			result.error = "No audio is arriving from the source";
		else if (db <= kSilenceDb)
			result.check = Check::Silent;
		else
			result = recognize(samples, rate, &stop_);

		if (!stop_)
			QMetaObject::invokeMethod(
				this, [this, result, capturedAt]() { onResult(result, capturedAt); },
				Qt::QueuedConnection);
		lock.lock();
	}
}

void Controller::onResult(const Recognition &result, double capturedAt)
{
	if (closed_ || !ready_)
		return;
	const std::string time = QDateTime::currentDateTime().toString("HH:mm:ss").toStdString();
	switch (result.check) {
	case Check::Match:
		lastMatch_ = time + "  " + result.artist + " - " + result.title;
		lastCheck_ = time + "  match";
		log("Match: " + result.artist + " - " + result.title);
		break;
	case Check::Clean:
		lastCheck_ = time + "  no match";
		break;
	case Check::Silent:
		lastCheck_ = time + "  silent";
		break;
	case Check::Error:
		lastCheck_ = time + "  failed: " + result.error;
		log("Check failed: " + result.error);
		break;
	}
	run(guard_.onResult(result.check, capturedAt, nowSeconds(), streaming()));
	checking_ = guard_.wantsChecks();
}

void Controller::onFrontendEvent(enum obs_frontend_event event, void *data)
{
	auto *self = static_cast<Controller *>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		self->ready_ = true;
		self->tick();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		self->run(self->guard_.streamStarted(nowSeconds()));
		self->tick();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
		// The saved restore entry stays until the source is back.
		self->run(self->guard_.manualUnmute(nowSeconds()));
		self->ready_ = false;
		self->checking_ = false;
		self->detach();
		break;
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT:
		self->ready_ = false;
		self->shutdown();
		break;
	default:
		break;
	}
}

} // namespace sg
