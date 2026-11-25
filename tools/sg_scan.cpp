// Replays a recording through the plugin's window, recognizer and guard, as the plugin would hear it.
//   ffmpeg -i show.mkv -f f32le -ac 1 -ar 48000 audio.f32
//   sg_scan audio.f32 48000 [stop]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>

#include "core/audio_window.hpp"
#include "core/guard.hpp"
#include "recognizer/shazam.hpp"

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::puts("usage: sg_scan <mono f32le file> <sample rate> [stop]");
		return 2;
	}
	const auto rate = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
	std::ifstream file(argv[1], std::ios::binary);
	if (!file || rate == 0) {
		std::puts("cannot read the input");
		return 2;
	}
	curl_global_init(CURL_GLOBAL_DEFAULT);

	sg::AudioWindow window(rate, 12);
	sg::Guard guard;
	guard.setConfig({120, 60, argc > 3});
	guard.setEnabled(true, 0);

	std::vector<float> chunk(rate);
	std::vector<int16_t> samples;
	for (int second = 1; file.read(reinterpret_cast<char *>(chunk.data()),
				       static_cast<std::streamsize>(chunk.size() * sizeof(float)));
	     second++) {
		const float *planes[1] = {chunk.data()};
		window.push(planes, 1, chunk.size());
		double db = 0;
		if (second < 12 || (second - 12) % 10 != 0 || !window.snapshot(samples, db))
			continue;

		sg::Recognition result;
		result.check = sg::Check::Silent;
		if (db > -60) {
			result = sg::recognize(samples, rate);
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
		const auto actions = guard.onResult(result.check, second, second, true);
		const char *names[] = {"clean", "silent", "MATCH", "ERROR"};
		std::printf("%6ds %6.1f dB %-6s %s%s%s%s", second, db, names[static_cast<int>(result.check)],
			    result.artist.c_str(), result.title.empty() ? "" : " | ", result.title.c_str(),
			    result.error.c_str());
		for (sg::Action a : actions)
			std::printf("  -> %s", a == sg::Action::Mute     ? "MUTE"
					       : a == sg::Action::Unmute ? "UNMUTE"
									 : "STOP");
		std::printf("\n");
		std::fflush(stdout);
	}
	curl_global_cleanup();
	return 0;
}
