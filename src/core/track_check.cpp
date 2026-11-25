#include "core/track_check.hpp"

namespace sg {

std::string trackProblem(const TrackSetup &s)
{
	if (!s.sourceFound)
		return "Audio source not found. Pick it below.";
	if (!s.advancedMode)
		return "Set Settings > Output > Output Mode to Advanced.";
	if (s.streamTrack < 1 || s.streamTrack > 6)
		return "Pick a streaming Audio Track in Settings > Output > Streaming.";
	if (s.flvRecording && !s.ffmpegRecording)
		return "Set the Recording Format to MKV.";

	const unsigned streamBit = 1u << (s.streamTrack - 1);
	unsigned recording = s.ffmpegRecording ? s.ffmpegMixes : s.recordTracks;
	// OBS records the stream track when no recording track is ticked.
	if (recording == 0)
		recording = streamBit;

	const std::string track = std::to_string(s.streamTrack);
	if (recording & streamBit)
		return "Recording uses stream track " + track + ". Pick another track on the Recording tab.";
	if (!(s.sourceMixers & streamBit))
		return "Tick track " + track + " for the source in Advanced Audio Properties.";
	if (!(s.sourceMixers & recording))
		return "Tick a recording track for the source in Advanced Audio Properties.";
	return "";
}

} // namespace sg
