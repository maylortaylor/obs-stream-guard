#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sg {

enum class State { Off, Blocked, Guarding, MutedMatch, MutedFailure, Paused, Stopped };
enum class Check { Clean, Silent, Match, Error };
enum class Action { Mute, Unmute, StopStream };
using Actions = std::vector<Action>;

struct Config {
	double unmuteAfter = 120;
	double failureAfter = 60;
	bool stopOnMatch = false;
};

// Decides when to mute and stop. Knows nothing about OBS. Times are seconds on one monotonic clock.
class Guard {
public:
	void setConfig(const Config &config) { config_ = config; }
	Actions setEnabled(bool enabled, double now);
	// An empty reason removes the block.
	Actions setBlocked(const std::string &reason, double now);
	Actions setPaused(bool paused, double now);
	Actions manualUnmute(double now);
	Actions streamStarted(double now);
	Actions onResult(Check check, double capturedAt, double now, bool streaming);

	State state() const { return state_; }
	const std::string &reason() const { return reason_; }
	bool muted() const;
	bool wantsChecks() const;

private:
	Actions enter(State next, double now);
	Actions onMatch(double now, bool streaming);

	Config config_;
	State state_ = State::Off;
	std::string reason_;
	bool enabled_ = false;
	bool paused_ = false;
	double resetAt_ = 0;
	double quietSince_ = 0;
	std::optional<double> errorsSince_;
};

} // namespace sg
