#pragma once

#include <memory>
#include <string>
#include "IVideoEncoder.h"


class VideoEncoderFactory
{
public:
	static std::shared_ptr<IVideoEncoder> CreateVideoEncoder(
		EncoderType encoder_type,
		int width, 
		int height, 
		int fps, 
		int bitrate,
		const std::wstring& output_path, 
		const std::wstring& output_filename);

	// Factory method to create H264 encoder using Media Foundation
	static std::shared_ptr<IVideoEncoder> CreateMFTH264VideoEncoder(
		int width, 
		int height, 
		int fps, 
		int bitrate,
		const std::wstring& output_path, 
		const std::wstring& output_filename);

	// Factory method to create VPX encoder (VP8/VP9/AV1)
	static std::shared_ptr<IVideoEncoder> CreateVPXVideoEncoder(
		EncoderType encoder_type,
		int width, 
		int height, 
		int fps, 
		int bitrate,
		const std::wstring& output_path, 
		const std::wstring& output_filename);
};
