// Copyright (c) 2010, Niels Martin Hansen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include "audio_renderer_horizonplot.h"

#include "audio_colorscheme.h"
#include "options.h"

#include <libaegisub/audio/provider.h>

#include <algorithm>
#include <wx/dcmemory.h>

AudioHorizonplotRenderer::AudioHorizonplotRenderer(std::string const& color_scheme_name)
{
	colors.reserve(AudioStyle_MAX);
	for (int i = 0; i < AudioStyle_MAX; ++i)
		colors.emplace_back(6, color_scheme_name, i);
}

AudioHorizonplotRenderer::~AudioHorizonplotRenderer() { }

void AudioHorizonplotRenderer::Render(wxBitmap &bmp, int start, AudioRenderingStyle style)
{
	static int numbands = 4;

	wxMemoryDC dc(bmp);
	wxRect rect(wxPoint(0, 0), bmp.GetSize());
	int channels = provider->GetChannels();
	int height   = rect.height / channels;

	const AudioColorScheme *pal = &colors[style];
	double pixel_samples = pixel_ms * provider->GetSampleRate() / 1000.0;

	assert(provider->GetBytesPerSample() == 2);

	// Fill the background
	dc.SetBrush(wxBrush(pal->get(0.0f)));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(rect);

	// Make sure we've got a buffer to fill with audio data
	if (!audio_buffer)
	{
		size_t buffer_needed = pixel_samples * channels * provider->GetBytesPerSample() + 1;
		audio_buffer.reset(new char[buffer_needed]);
	}

	// Create the colors for the lower and upper bands
	double step = .15/numbands;
	wxPen pen_high[numbands],
	      pen_low[numbands];

	for(size_t i=0; i<numbands; i++) {
		pen_high[i] = wxPen(pal->get(.5 - step*i));
		pen_low[i]  = wxPen(pal->get(.3 - step*i));
	}

	// Draw a line for each of the pixel strips
	double cur_sample = start * pixel_samples;
	for (int x = 0; x < rect.width; ++x)
	{
		int64_t to   = (int64_t) pixel_samples,
				from = (int64_t) cur_sample;

		to += to == 0;

		provider->GetAudio(audio_buffer.get(), from, to);
		cur_sample += pixel_samples;

		for (int channel = 0, basepoint = height;
		     channel < channels;
		     channel++, basepoint += height)
		{
			auto aud = reinterpret_cast<const int16_t *>(audio_buffer.get());
			int peak_min = 0,
			    peak_max = 0;

			aud += channel;

			for (int si = to;  si > 0; --si, aud += channels)
				if (*aud > 0)
					peak_max = std::max(peak_max, (int)*aud);
				else
					peak_min = std::min(peak_min, (int)*aud);

			peak_min = std::max((int)(peak_min * amplitude_scale * height) / 0x8000, -height * numbands);
			peak_max = std::min((int)(peak_max * amplitude_scale * height) / 0x8000, height * numbands);

			// Draw a line on each band
			for (size_t b=0; b < numbands && peak_min <= 0; b++) {
				dc.SetPen(pen_low[b]);
				dc.DrawLine(x, basepoint-height, x, basepoint - height - (-peak_min > height ? -height : peak_min));
				peak_min += height;
			}
			
			for (size_t b=0; b < numbands && peak_max > 0; b++) {
				dc.SetPen(pen_high[b]);
				dc.DrawLine(x, basepoint, x, basepoint - (peak_max > height ? height : peak_max));
				peak_max -= height;
			}
		}


		// Draw Separators between each channel
		dc.SetPen(pal->get(1.0f));
		for (int channel = 0, basepoint = height;
		     channel < channels;
		     channel++, basepoint += height)
			dc.DrawLine(0, basepoint, rect.width, basepoint);
	}
}

void AudioHorizonplotRenderer::RenderBlank(wxDC &dc, const wxRect &rect, AudioRenderingStyle style)
{
	const AudioColorScheme *pal = &colors[style];
	wxColor line(pal->get(1.0));
	wxColor bg(pal->get(0.0));

	// Draw the line as background above and below, and line in the middle, to avoid
	// overdraw flicker (the common theme in all of audio display direct drawing).
	int halfheight = rect.height / 2;

	dc.SetBrush(wxBrush(bg));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(rect.x, rect.y, rect.width, halfheight);
	dc.DrawRectangle(rect.x, rect.y + halfheight + 1, rect.width, rect.height - halfheight - 1);

	dc.SetPen(wxPen(line));
	dc.DrawLine(rect.x, rect.y+halfheight, rect.x+rect.width, rect.y+halfheight);
}
