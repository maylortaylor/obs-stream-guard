#pragma once

#include <string>

namespace sg {

// Track masks use bit 0 for OBS track 1.
struct TrackSetup {
	bool sourceFound;
	bool advancedMode;
	bool flvRecording;
	bool ffmpegRecording;
	int streamTrack; // 1 to 6
	unsigned recordTracks;
	unsigned ffmpegMixes;
	unsigned sourceMixers;
};

// Returns why a mute is not safe. An empty string means it is safe.
std::string trackProblem(const TrackSetup &setup);

} // namespace sg
