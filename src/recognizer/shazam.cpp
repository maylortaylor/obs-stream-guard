// Request format follows cli/communication/shazam.cpp in BayernMuller/vibra (GPL-3.0).
#include "recognizer/shazam.hpp"

#include <chrono>
#include <memory>
#include <random>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <vibra.h>

namespace sg {

using json = nlohmann::json;

static std::string stringField(const json &object, const char *key)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_string() ? it->get<std::string>() : "";
}

Recognition parseResponse(long httpStatus, const std::string &body)
{
	Recognition result;
	if (httpStatus != 200) {
		result.error = "HTTP " + std::to_string(httpStatus);
		return result;
	}
	const json data = json::parse(body, nullptr, false);
	if (!data.is_object() || !data.contains("matches")) {
		result.error = "Unreadable answer";
		return result;
	}
	const auto track = data.find("track");
	if (track == data.end() || !track->is_object()) {
		result.check = Check::Clean;
		return result;
	}
	result.check = Check::Match;
	result.title = stringField(*track, "title");
	result.artist = stringField(*track, "subtitle");
	return result;
}

static std::string uuid()
{
	static thread_local std::mt19937 generator{std::random_device{}()};
	std::uniform_int_distribution<int> digit(0, 15);
	std::string out = "xxxxxxxx-xxxx-4xxx-8xxx-xxxxxxxxxxxx";
	for (char &c : out) {
		if (c == 'x')
			c = "0123456789abcdef"[digit(generator)];
	}
	return out;
}

static size_t collect(char *data, size_t size, size_t count, void *target)
{
	static_cast<std::string *>(target)->append(data, size * count);
	return size * count;
}

static int abortWhenCancelled(void *flag, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	const auto *cancel = static_cast<const std::atomic<bool> *>(flag);
	return cancel && cancel->load() ? 1 : 0;
}

Recognition recognize(const std::vector<int16_t> &mono, uint32_t sampleRate, const std::atomic<bool> *cancel)
{
	Recognition result;
	std::unique_ptr<Fingerprint, void (*)(Fingerprint *)> print(
		vibra_get_fingerprint_from_signed_pcm(reinterpret_cast<const char *>(mono.data()),
						      static_cast<int>(mono.size() * sizeof(int16_t)),
						      static_cast<int>(sampleRate), 16, 1),
		vibra_free_fingerprint);
	if (!print) {
		result.error = "Fingerprint failed";
		return result;
	}

	const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
				 std::chrono::system_clock::now().time_since_epoch())
				 .count();
	const json request = {
		{"geolocation", {{"altitude", 300}, {"latitude", 45}, {"longitude", 2}}},
		{"signature",
		 {{"samplems", vibra_get_sample_ms_from_fingerprint(print.get())},
		  {"timestamp", now},
		  {"uri", vibra_get_uri_from_fingerprint(print.get())}}},
		{"timestamp", now},
		{"timezone", "Europe/Paris"},
	};
	const std::string content = request.dump();
	const std::string url = "https://amp.shazam.com/discovery/v5/fr/FR/android/-/tag/" + uuid() + "/" + uuid() +
				"?sync=true&webv3=true&sampling=true&connected=&shazamapiversion=v3"
				"&sharehub=true&video=v3";

	std::unique_ptr<CURL, void (*)(CURL *)> curl(curl_easy_init(), curl_easy_cleanup);
	if (!curl) {
		result.error = "Network library failed";
		return result;
	}
	curl_slist *list = curl_slist_append(nullptr, "Content-Type: application/json");
	list = curl_slist_append(list, "Content-Language: en_US");
	std::unique_ptr<curl_slist, void (*)(curl_slist *)> headers(list, curl_slist_free_all);

	std::string body;
	curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, content.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
	curl_easy_setopt(curl.get(), CURLOPT_USERAGENT,
			 "Dalvik/2.1.0 (Linux; U; Android 6.0.1; SM-G920F Build/MMB29K)");
	curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, collect);
	curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, abortWhenCancelled);
	curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, const_cast<std::atomic<bool> *>(cancel));

	const CURLcode code = curl_easy_perform(curl.get());
	if (code != CURLE_OK) {
		result.error = curl_easy_strerror(code);
		return result;
	}
	long status = 0;
	curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
	return parseResponse(status, body);
}

} // namespace sg
