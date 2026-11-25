#include "core/guard.hpp"

namespace sg {

static bool isMuted(State s)
{
	return s == State::MutedMatch || s == State::MutedFailure || s == State::Stopped;
}

bool Guard::muted() const
{
	return isMuted(state_);
}

bool Guard::wantsChecks() const
{
	return state_ == State::Guarding || state_ == State::MutedMatch || state_ == State::MutedFailure;
}

// Moves to a state that follows a user or setup change. Old results and errors no longer count.
Actions Guard::enter(State next, double now)
{
	const bool wasMuted = muted();
	state_ = next;
	resetAt_ = now;
	errorsSince_.reset();
	return wasMuted ? Actions{Action::Unmute} : Actions{};
}

Actions Guard::setEnabled(bool enabled, double now)
{
	if (enabled == enabled_)
		return {};
	enabled_ = enabled;
	paused_ = false;
	if (!enabled)
		return enter(State::Off, now);
	return enter(reason_.empty() ? State::Guarding : State::Blocked, now);
}

Actions Guard::setBlocked(const std::string &reason, double now)
{
	if (reason == reason_)
		return {};
	reason_ = reason;
	if (!enabled_)
		return {};
	if (!reason_.empty())
		return enter(State::Blocked, now);
	return enter(paused_ ? State::Paused : State::Guarding, now);
}

Actions Guard::setPaused(bool paused, double now)
{
	if (!enabled_ || paused == paused_)
		return {};
	paused_ = paused;
	if (state_ == State::Blocked)
		return {};
	return enter(paused ? State::Paused : State::Guarding, now);
}

Actions Guard::manualUnmute(double now)
{
	if (!muted())
		return {};
	return enter(State::Guarding, now);
}

Actions Guard::streamStarted(double now)
{
	if (state_ == State::Paused)
		return setPaused(false, now);
	paused_ = false;
	if (state_ == State::Stopped)
		return enter(State::Guarding, now);
	return {};
}

Actions Guard::onMatch(double now, bool streaming)
{
	const bool wasMuted = muted();
	quietSince_ = now;
	Actions actions;
	if (!wasMuted)
		actions.push_back(Action::Mute);
	if (config_.stopOnMatch && streaming) {
		actions.push_back(Action::StopStream);
		state_ = State::Stopped;
	} else {
		state_ = State::MutedMatch;
	}
	return actions;
}

Actions Guard::onResult(Check check, double capturedAt, double now, bool streaming)
{
	if (!wantsChecks() || capturedAt < resetAt_)
		return {};

	if (check == Check::Error) {
		if (!errorsSince_)
			errorsSince_ = now;
		if (state_ == State::MutedMatch)
			quietSince_ = now;
		if (state_ == State::Guarding && now - *errorsSince_ >= config_.failureAfter) {
			state_ = State::MutedFailure;
			return {Action::Mute};
		}
		return {};
	}

	// Silent audio is not sent out, so it proves nothing about the detector.
	if (check != Check::Silent)
		errorsSince_.reset();

	if (check == Check::Match)
		return onMatch(now, streaming);

	const bool detectorBack = state_ == State::MutedFailure && check == Check::Clean;
	const bool quietEnough = state_ == State::MutedMatch && now - quietSince_ >= config_.unmuteAfter;
	if (detectorBack || quietEnough) {
		state_ = State::Guarding;
		return {Action::Unmute};
	}
	return {};
}

} // namespace sg
