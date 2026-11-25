#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <QObject>
#include <QTimer>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include "core/audio_window.hpp"
#include "core/guard.hpp"
#include "recognizer/shazam.hpp"

namespace sg {

struct Settings {
	bool enabled = false;
	std::string source = "x32 Out";
	bool stopOnMatch = false;
	int unmuteAfter = 120;
};

// Joins the guard to OBS. Every public method runs on the UI thread.
class Controller : public QObject {
public:
	Controller();
	~Controller() override;
	void shutdown();

	const Settings &settings() const { return settings_; }
	void apply(const Settings &settings);
	void setPaused(bool paused);
	void unmuteNow();

	const Guard &guard() const { return guard_; }
	const std::string &lastCheck() const { return lastCheck_; }
	const std::string &lastMatch() const { return lastMatch_; }
	const std::deque<std::string> &events() const { return events_; }

private:
	static void onFrontendEvent(enum obs_frontend_event event, void *data);
	static void onAudio(void *data, obs_source_t *source, const struct audio_data *audio, bool muted);

	void tick();
	void work();
	void onResult(const Recognition &result, double capturedAt);
	void run(const Actions &actions);
	void attach(obs_source_t *source);
	void detach();
	std::string findProblem(obs_source_t *source);
	void mute();
	void unmute();
	void applyPendingRestore();
	void saveSettings();
	void log(const std::string &line);

	Settings settings_;
	Guard guard_;
	QTimer timer_;
	bool ready_ = false;
	bool closed_ = false;

	obs_weak_source_t *attached_ = nullptr;
	std::unique_ptr<AudioWindow> window_;

	// Set while a track bit is removed, and saved to disk so a crash cannot lose it.
	std::string restoreSource_;
	uint32_t restoreBit_ = 0;

	std::string lastCheck_ = "none yet";
	std::string lastMatch_ = "none yet";
	std::deque<std::string> events_;

	std::thread worker_;
	std::mutex workerMutex_;
	std::condition_variable wake_;
	std::atomic<bool> stop_{false};
	std::atomic<bool> checking_{false};
};

} // namespace sg
