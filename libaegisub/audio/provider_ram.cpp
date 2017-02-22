// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#include "libaegisub/audio/provider.h"

#include "libaegisub/make_unique.h"

#include <array>
#include <boost/container/stable_vector.hpp>
#include <thread>

namespace {
using namespace agi;

#define CacheBits 22
#define CacheBlockSize (1 << CacheBits)

class RAMAudioProvider final : public AudioProviderWrapper {
	std::atomic<bool> cancelled = {false};
	std::thread decoder;
	char *buffer;

	void FillBuffer(void *buf, int64_t start, int64_t count) const override;

public:
	RAMAudioProvider(std::unique_ptr<AudioProvider> src)
	: AudioProviderWrapper(std::move(src))
	{
		decoded_samples = 0;

		try {
			buffer = new char[source->GetNumSamples() * source->GetChannels() * source->GetBytesPerSample()];
		}
		catch (std::bad_alloc const&) {
			throw AudioProviderError("Not enough memory available to cache in RAM");
		}

		decoder = std::thread([&] {
			int64_t actual_read = 2<<10;
			for (size_t i = 0; i < num_samples; i+=actual_read) {
				actual_read = std::min<int64_t>(actual_read, num_samples - i);
				if (cancelled) break;
				source->GetAudio(&buffer[i * bytes_per_sample * channels], i, actual_read);
				decoded_samples += actual_read;
			}
		});
	}

	~RAMAudioProvider() {
		cancelled = true;
		if (buffer) {
			delete buffer;
			buffer = NULL;
		}
		decoder.join();
	}
};

void RAMAudioProvider::FillBuffer(void *buf, int64_t start, int64_t count) const {
	memcpy(buf, buffer + start * source->GetChannels() * source->GetBytesPerSample(),
			                 count * source->GetChannels() * source->GetBytesPerSample());
}
}

namespace agi {
std::unique_ptr<AudioProvider> CreateRAMAudioProvider(std::unique_ptr<AudioProvider> src) {
	return agi::make_unique<RAMAudioProvider>(std::move(src));
}
}
