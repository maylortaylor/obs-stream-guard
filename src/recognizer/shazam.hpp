#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "core/guard.hpp"

namespace sg {

struct Recognition {
	Check check = Check::Error;
	std::string artist;
	std::string title;
	std::string error;
};

Recognition parseResponse(long httpStatus, const std::string &body);

// Fingerprints 12 seconds of 16 bit mono audio and asks the unofficial Shazam service.
// Blocks for up to 15 seconds. Set cancel to end the request early.
Recognition recognize(const std::vector<int16_t> &mono, uint32_t sampleRate, const std::atomic<bool> *cancel = nullptr);

} // namespace sg
