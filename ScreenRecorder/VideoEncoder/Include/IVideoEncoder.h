#pragma once

#include <string>
#include <vector>
#include <Windows.h>

enum class EncoderType
{
	MFT_H264,  // Media Foundation Transform H.264
	MFT_H265,  // Media Foundation Transform H.265
	VPX_VP8,   // VPX VP8
	VPX_VP9,   // VPX VP9
	VPX_VP9_444,
	VPX_AV1    // VPX AV1
};

class IVideoEncoder
{
public:
	virtual ~IVideoEncoder() = default;

	virtual bool Initialize(EncoderType encoder_type) = 0;
	virtual bool ProcessFrame(const std::vector<uint8_t>& image_buffer, int width, int height) = 0;
	virtual HRESULT Finalize() = 0;

public:
	bool is_initialized = false;
};
