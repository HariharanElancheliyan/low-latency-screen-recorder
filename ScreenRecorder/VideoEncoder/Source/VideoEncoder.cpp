#include <mferror.h>
#include <icodecapi.h>
#include <Codecapi.h>
#include <chrono>

#include "VideoEncoder.h"
#include "Utils.h"


using namespace Microsoft::WRL;

VideoEncoder::VideoEncoder(int width, int height, int fps, int bitrate,
    const std::wstring& output_path, const std::wstring& output_filename)
    :   width_(width),
        height_(height),
        fps_(fps),
        bitrate_(bitrate),
        output_path_(output_path),
	    output_filename_(output_filename)
{
    MFStartup(MF_VERSION);
}

VideoEncoder::~VideoEncoder() 
{
    Finalize();
    MFShutdown();
}

bool VideoEncoder::Initialize(VideoCodec codec_type)
{
    switch (codec_type)
    {
        case VideoCodec::H264:
			codec_guid_ = MFVideoFormat_H264;
            break;
        case VideoCodec::H265:
			codec_guid_ = MFVideoFormat_H265;
            break;
        case VideoCodec::VP8:
            codec_guid_ = MFVideoFormat_VP80;
            break;
        case VideoCodec::VP9:
			codec_guid_ = MFVideoFormat_VP90;
            break;
        case VideoCodec::AV1:
			codec_guid_ = MFVideoFormat_AV1;
            break;
        default:
            break;
    }


    return SUCCEEDED(ConfigureSinkWriter());
}

bool VideoEncoder::ProcessFrame(const std::vector<uint8_t>& image_buffer, int width, int height)
{
	return SUCCEEDED(EncodeFrame(image_buffer, width, height));
}

HRESULT VideoEncoder::ConfigureSinkWriter() 
{
    ComPtr<IMFAttributes> attributes;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) return hr;

	std::wstring filename_with_path = output_path_ + output_filename_;

    hr = MFCreateSinkWriterFromURL(filename_with_path.c_str(),
                                   nullptr, 
                                   attributes.Get(),
                                   &sink_writer_);
    if (FAILED(hr)) return hr;

	if (FAILED(ConfigureOutputType())) return E_FAIL;
	if (FAILED(ConfigureInputType())) return E_FAIL;

    return sink_writer_->BeginWriting();
}

HRESULT VideoEncoder::ReConfigureSinkWriter(int width, int height)
{
    width_ = width;
    height_ = height;

    Finalize();
    output_filename_.clear();

    output_filename_.append(RecorderUtils::GetCurrentDateTime());
	output_filename_ = output_filename_ + std::to_wstring(width) + L"x" + std::to_wstring(height) + L".mp4";

    return ConfigureSinkWriter();
}

HRESULT VideoEncoder::ConfigureInputType()
{
    ComPtr<IMFMediaType> input_type;
    HRESULT hr = MFCreateMediaType(&input_type);
    if (FAILED(hr)) return hr;

    int default_stride = width_ * 4;

    input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);

    MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    input_type->SetUINT32(MF_MT_DEFAULT_STRIDE, default_stride);

    hr = sink_writer_->SetInputMediaType(stream_index_, input_type.Get(), nullptr);
    
    return hr;
}

HRESULT VideoEncoder::ConfigureOutputType()
{
    ComPtr<IMFMediaType> output_type;
    HRESULT hr = MFCreateMediaType(&output_type);
    if (FAILED(hr)) return hr;

    output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    output_type->SetGUID(MF_MT_SUBTYPE, codec_guid_);

    MFSetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(output_type.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(output_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    output_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate_);
    output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    hr = sink_writer_->AddStream(output_type.Get(), &stream_index_);
    return hr;
}

HRESULT VideoEncoder::EncodeFrame(const std::vector<uint8_t>& image_buffer, int width, int height) 
{
	if (width != width_ || height != height_)
	{
        width_ = width;
        height_ = height;

		if (FAILED(ReConfigureSinkWriter(width, height))) return E_FAIL;
	}

    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaBuffer> buffer;

    DWORD buffer_size = static_cast<DWORD>(image_buffer.size());
    HRESULT hr = MFCreateMemoryBuffer(buffer_size, &buffer);
    if (FAILED(hr)) return hr;

    BYTE* dest = nullptr;
    DWORD max_len = 0;
    DWORD current_len = 0;

    hr = buffer->Lock(&dest, &max_len, &current_len);
    if (FAILED(hr)) return hr;

    const LONG stride = 4 * width;

    hr = MFCopyImage(
        dest,                      // Destination buffer.
        stride,                    // Destination stride.
        image_buffer.data(),       // First row in source image.
        stride,                    // Source stride.
        stride,                    // Image width in bytes.
        height                     // Image height in pixels.
    );

    if (FAILED(hr)) return hr;

    buffer->SetCurrentLength(buffer_size);
    buffer->Unlock();

    hr = MFCreateSample(&sample);
    if (FAILED(hr)) return hr;

    sample->AddBuffer(buffer.Get());

    const uint64_t kHundredNsPerSecond = 10000000;
    uint64_t duration = kHundredNsPerSecond / fps_;
    uint64_t timestamp = frame_count_ * duration;

    sample->SetSampleTime(timestamp);
    sample->SetSampleDuration(duration);

    hr = E_FAIL;
    if (sink_writer_)
    {
        hr = sink_writer_->WriteSample(stream_index_, sample.Get());
    }

    if (SUCCEEDED(hr))
    {
        ++frame_count_;
    }

    return hr;
}

HRESULT VideoEncoder::Finalize() 
{
    if (sink_writer_) 
    {
        sink_writer_->Finalize();
        sink_writer_ = nullptr;
    }

    return S_OK;
}
