#pragma once

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <chrono>

#include "IVideoEncoder.h"


class MFTH264VideoEncoder : public IVideoEncoder
{
public:
    MFTH264VideoEncoder(int width, int height, int fps, int bitrate,
        const std::wstring& output_path, const std::wstring& output_filename);
    ~MFTH264VideoEncoder() override;

    bool Initialize(EncoderType codec_type) override;
    bool ProcessFrame(const std::vector<uint8_t>& image_buffer, int width, int height) override;
    HRESULT Finalize() override;

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

    // Frame timing
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point last_frame_time_;
    bool is_first_frame_ = true;
};
