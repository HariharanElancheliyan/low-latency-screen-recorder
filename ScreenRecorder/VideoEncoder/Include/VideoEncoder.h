#pragma once

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>


enum class VideoCodec
{
	H264,
	H265,
	VP8,
	VP9,
	AV1
};

class VideoEncoder 
{
public:
    VideoEncoder(int width, int height, int fps, int bitrate,
        const std::wstring& output_path, const std::wstring& output_filename);
    ~VideoEncoder();

    bool Initialize(VideoCodec codec_type);
    bool ProcessFrame(const std::vector<uint8_t>& image_buffer, int width, int height);
    HRESULT Finalize();

private:

    HRESULT ConfigureSinkWriter();
	HRESULT ReConfigureSinkWriter(int width, int height);

	HRESULT ConfigureInputType();
	HRESULT ConfigureOutputType();

    HRESULT EncodeFrame(const std::vector<uint8_t>& image_buffer, int width, int height);

    int width_;
    int height_;
    int fps_;
    int bitrate_;
    uint64_t frame_count_ = 0;

    std::wstring output_path_;
    std::wstring output_filename_;
    Microsoft::WRL::ComPtr<IMFSinkWriter> sink_writer_;
    DWORD stream_index_ = 0;

	GUID codec_guid_ = MFVideoFormat_H264;
};

