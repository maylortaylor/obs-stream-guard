#include "core/audio_window.hpp"

#include <algorithm>
#include <cmath>

namespace sg {

AudioWindow::AudioWindow(uint32_t sampleRate, uint32_t seconds)
	: sampleRate_(sampleRate),
	  ring_(static_cast<size_t>(sampleRate) * seconds)
{
}

void AudioWindow::push(const float *const *planes, size_t channels, size_t frames)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (size_t i = 0; i < frames; i++) {
		float sum = 0.0f;
		size_t used = 0;
		for (size_t c = 0; c < channels; c++) {
			if (planes[c]) {
				sum += planes[c][i];
				used++;
			}
		}
		const float mono = used ? sum / static_cast<float>(used) : 0.0f;
		ring_[next_] = static_cast<int16_t>(std::clamp(mono, -1.0f, 1.0f) * 32767.0f);
		next_ = (next_ + 1) % ring_.size();
	}
	total_ += frames;
}

bool AudioWindow::snapshot(std::vector<int16_t> &out, double &dbfs) const
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (ring_.empty() || total_ < ring_.size())
			return false;
		const auto split = ring_.begin() + static_cast<std::ptrdiff_t>(next_);
		out.assign(split, ring_.end());
		out.insert(out.end(), ring_.begin(), split);
	}
	double squares = 0;
	for (int16_t sample : out) {
		const double v = sample / 32768.0;
		squares += v * v;
	}
	const double mean = squares / static_cast<double>(out.size());
	dbfs = mean > 0 ? 10.0 * std::log10(mean) : -120.0;
	return true;
}

uint64_t AudioWindow::totalFrames() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return total_;
}

} // namespace sg
