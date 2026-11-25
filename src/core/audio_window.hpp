#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace sg {

// Keeps the last few seconds of audio as 16 bit mono. Safe to call from two threads.
class AudioWindow {
public:
	AudioWindow(uint32_t sampleRate, uint32_t seconds);
	void push(const float *const *planes, size_t channels, size_t frames);
	// False until the window is full. Samples are oldest first.
	bool snapshot(std::vector<int16_t> &out, double &dbfs) const;
	uint64_t totalFrames() const;
	uint32_t sampleRate() const { return sampleRate_; }

private:
	const uint32_t sampleRate_;
	mutable std::mutex mutex_;
	std::vector<int16_t> ring_;
	size_t next_ = 0;
	uint64_t total_ = 0;
};

} // namespace sg
