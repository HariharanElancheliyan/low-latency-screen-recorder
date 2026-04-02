#include "VideoEncoderFactory.h"
#include "MFTH264VideoEncoder.h"
#include "VPXVideoHandler.h"

std::shared_ptr<IVideoEncoder> VideoEncoderFactory::CreateVideoEncoder(
	EncoderType encoder_type,
	int width,
	int height,
	int fps,
	int bitrate,
	const std::wstring& output_path,
	const std::wstring& output_filename)
{
	switch (encoder_type)
	{
	case EncoderType::MFT_H264:
	case EncoderType::MFT_H265:
		return CreateMFTH264VideoEncoder(width, height, fps, bitrate, output_path, output_filename);

	case EncoderType::VPX_VP8:
	case EncoderType::VPX_VP9:
	case EncoderType::VPX_VP9_444:
	case EncoderType::VPX_AV1:
		return CreateVPXVideoEncoder(encoder_type, width, height, fps, bitrate, output_path, output_filename);

	default:
		return nullptr;
	}
}

std::shared_ptr<IVideoEncoder> VideoEncoderFactory::CreateMFTH264VideoEncoder(
	int width,
	int height,
	int fps,
	int bitrate,
	const std::wstring& output_path,
	const std::wstring& output_filename)
{
	return std::make_shared<MFTH264VideoEncoder>(width, height, fps, bitrate, output_path, output_filename);
}

std::shared_ptr<IVideoEncoder> VideoEncoderFactory::CreateVPXVideoEncoder(
	EncoderType encoder_type,
	int width,
	int height,
	int fps,
	int bitrate,
	const std::wstring& output_path,
	const std::wstring& output_filename)
{
	auto encoder = std::make_shared<VPXVideoHandler>(width, height, fps, bitrate, output_path, output_filename);
	
	return encoder;
}
