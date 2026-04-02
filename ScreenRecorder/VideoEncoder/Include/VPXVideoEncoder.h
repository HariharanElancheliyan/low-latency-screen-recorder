#pragma once

#include <vpx/vp8cx.h>
#include <vpx/vpx_codec.h>
#include <vpx/vpx_encoder.h>

#include <mutex>

#include "video_data.h"

namespace screen_recorder
{
	enum class VPX_TYPE { VP8 = 0, VP9_420, VP9_444 };

	class VPXVideoEncoder 
	{
	public:
		VPXVideoEncoder();
		VPXVideoEncoder(VPX_TYPE vpx_encoder);

		virtual ~VPXVideoEncoder();

		bool Initialize();
		bool IsInitialized() const { return is_initialized; }

		void SetWidthAndHeight(int width, int height);
		bool ConfigureBitrateAndFPS(unsigned long long bitrate, int fps);
		bool EncodeFrame(uint8_t* frame_data, bool& keyframe, video_data& encoded_data);

		void Shutdown();

	private:
		vpx_codec_ctx_t codec;
		vpx_codec_enc_cfg_t cfg;
		vpx_codec_iface_t* iface;
		vpx_img_fmt image_format;

		std::mutex encoder_mutex;

		unsigned long long frame_count;

		bool keyframe;
		bool quality_mode;

		int main_stream_width_ = 1920;
		int main_stream_height_ = 1200;
		volatile int fps;
		volatile unsigned long long bitrate;

		VPX_TYPE vpx_encoder;
		bool is_initialized = false;
	};
}  // namespace screen_recorder