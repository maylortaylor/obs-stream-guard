#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/audio_window.hpp"
#include "core/guard.hpp"
#include "core/track_check.hpp"
#include "recognizer/shazam.hpp"

using namespace sg;

static int failures = 0;
#define CHECK(x)                                                         \
	do {                                                             \
		if (!(x)) {                                              \
			std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); \
			failures++;                                      \
		}                                                        \
	} while (0)

static Actions none;
static Actions mute{Action::Mute};
static Actions unmute{Action::Unmute};
static Actions muteStop{Action::Mute, Action::StopStream};

static Guard onGuard(bool stopOnMatch = false)
{
	Guard g;
	g.setConfig({120, 60, stopOnMatch});
	g.setEnabled(true, 0);
	return g;
}

static Actions feed(Guard &g, Check c, double t, bool streaming = true)
{
	return g.onResult(c, t, t, streaming);
}

static void guardMatchAndUnmute()
{
	Guard off;
	CHECK(off.state() == State::Off);
	CHECK(feed(off, Check::Match, 1) == none);

	Guard g = onGuard();
	CHECK(g.state() == State::Guarding);
	CHECK(feed(g, Check::Clean, 10) == none);
	CHECK(feed(g, Check::Match, 20) == mute);
	CHECK(g.state() == State::MutedMatch);
	CHECK(feed(g, Check::Clean, 130) == none);
	CHECK(feed(g, Check::Match, 135) == none);
	CHECK(feed(g, Check::Clean, 245) == none);
	CHECK(feed(g, Check::Silent, 255) == unmute);
	CHECK(g.state() == State::Guarding);
}

static void guardStopMode()
{
	Guard g = onGuard(true);
	CHECK(feed(g, Check::Match, 10) == muteStop);
	CHECK(g.state() == State::Stopped);
	CHECK(feed(g, Check::Clean, 500) == none);
	CHECK(g.muted());
	CHECK(g.streamStarted(600) == unmute);
	CHECK(g.state() == State::Guarding);

	Guard idle = onGuard(true);
	CHECK(feed(idle, Check::Match, 10, false) == mute);
	CHECK(idle.state() == State::MutedMatch);
}

static void guardFailure()
{
	Guard g = onGuard(true);
	CHECK(feed(g, Check::Error, 10) == none);
	CHECK(feed(g, Check::Silent, 40) == none);
	CHECK(feed(g, Check::Error, 69) == none);
	CHECK(feed(g, Check::Error, 70) == mute);
	CHECK(g.state() == State::MutedFailure);
	CHECK(feed(g, Check::Silent, 80) == none);
	CHECK(feed(g, Check::Clean, 90) == unmute);
	CHECK(g.state() == State::Guarding);

	CHECK(feed(g, Check::Error, 100) == none);
	CHECK(feed(g, Check::Clean, 110) == none);
	CHECK(feed(g, Check::Error, 165) == none);

	Guard m = onGuard(true);
	feed(m, Check::Error, 10);
	feed(m, Check::Error, 70);
	CHECK((feed(m, Check::Match, 80) == Actions{Action::StopStream}));
	CHECK(m.state() == State::Stopped);
}

static void guardErrorsDelayUnmute()
{
	Guard g = onGuard();
	feed(g, Check::Match, 0);
	CHECK(feed(g, Check::Error, 100) == none);
	CHECK(feed(g, Check::Clean, 130) == none);
	CHECK(feed(g, Check::Clean, 220) == unmute);
}

static void guardManualPauseDisable()
{
	Guard g = onGuard();
	feed(g, Check::Match, 10);
	CHECK(g.manualUnmute(20) == unmute);
	CHECK(g.state() == State::Guarding);
	CHECK(g.onResult(Check::Match, 15, 25, true) == none);
	CHECK(feed(g, Check::Match, 30) == mute);

	CHECK(g.setPaused(true, 40) == unmute);
	CHECK(g.state() == State::Paused);
	CHECK(!g.wantsChecks());
	CHECK(feed(g, Check::Match, 50) == none);
	CHECK(g.streamStarted(60) == none);
	CHECK(g.state() == State::Guarding);

	feed(g, Check::Match, 70);
	CHECK(g.setEnabled(false, 80) == unmute);
	CHECK(g.state() == State::Off);
}

static void guardBlocked()
{
	Guard g = onGuard();
	feed(g, Check::Match, 10);
	CHECK(g.setBlocked("no source", 20) == unmute);
	CHECK(g.state() == State::Blocked);
	CHECK(g.reason() == "no source");
	CHECK(feed(g, Check::Match, 30) == none);
	CHECK(g.setBlocked("", 40) == none);
	CHECK(g.state() == State::Guarding);
}

static void trackRules()
{
	TrackSetup good{true, true, false, false, 1, 0b10, 0, 0b11};
	CHECK(trackProblem(good).empty());
	TrackSetup s = good;
	s.sourceFound = false;
	CHECK(!trackProblem(s).empty());
	s = good;
	s.advancedMode = false;
	CHECK(!trackProblem(s).empty());
	s = good;
	s.recordTracks = 0b11;
	CHECK(!trackProblem(s).empty());
	s = good;
	s.recordTracks = 0;
	CHECK(!trackProblem(s).empty());
	s = good;
	s.sourceMixers = 0b10;
	CHECK(!trackProblem(s).empty());
	s = good;
	s.sourceMixers = 0b01;
	CHECK(!trackProblem(s).empty());
	s = good;
	s.flvRecording = true;
	CHECK(!trackProblem(s).empty());
	s = good;
	s.ffmpegRecording = true;
	s.ffmpegMixes = 0b01;
	CHECK(!trackProblem(s).empty());
}

static void audioWindow()
{
	AudioWindow w(100, 2);
	std::vector<float> left(150, 0.5f), right(150, 0.5f);
	const float *planes[2] = {left.data(), right.data()};
	std::vector<int16_t> out;
	double db = 0;
	w.push(planes, 2, 150);
	CHECK(!w.snapshot(out, db));
	w.push(planes, 2, 150);
	CHECK(w.snapshot(out, db));
	CHECK(out.size() == 200);
	CHECK(std::abs(db - (-6.0)) < 0.2);
	CHECK(w.totalFrames() == 300);
	std::vector<float> zero(200, 0.0f);
	const float *quiet[1] = {zero.data()};
	w.push(quiet, 1, 200);
	CHECK(w.snapshot(out, db));
	CHECK(db < -90);
	CHECK(out[0] == 0);
}

static void parsing()
{
	auto m = parseResponse(200, R"({"matches":[{}],"track":{"title":"T","subtitle":"A"}})");
	CHECK(m.check == Check::Match);
	CHECK(m.title == "T" && m.artist == "A");
	CHECK(parseResponse(200, R"({"matches":[]})").check == Check::Clean);
	CHECK(parseResponse(429, R"({"matches":[]})").check == Check::Error);
	CHECK(parseResponse(200, "<html>").check == Check::Error);
	CHECK(parseResponse(200, "{}").check == Check::Error);
	CHECK(parseResponse(200, "[]").check == Check::Error);
	CHECK(parseResponse(200, R"({"matches":[],"track":{"title":5}})").check == Check::Match);
}

int main()
{
	guardMatchAndUnmute();
	guardStopMode();
	guardFailure();
	guardErrorsDelayUnmute();
	guardManualPauseDisable();
	guardBlocked();
	trackRules();
	audioWindow();
	parsing();
	std::printf(failures ? "%d FAILURES\n" : "ALL TESTS PASSED\n", failures);
	return failures ? 1 : 0;
}
