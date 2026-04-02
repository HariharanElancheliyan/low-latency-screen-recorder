#include <mferror.h>
#include <icodecapi.h>
#include <Codecapi.h>
#include <chrono>

#include "MFTH264VideoEncoder.h"
#include "Utils.h"


using namespace Microsoft::WRL;

MFTH264VideoEncoder::MFTH264VideoEncoder(int width, int height, int fps, int bitrate,
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

MFTH264VideoEncoder::~MFTH264VideoEncoder() 
{
    Finalize();
    MFShutdown();
}

bool MFTH264VideoEncoder::Initialize(EncoderType codec_type)
{
    switch (codec_type)
    {
	case EncoderType::MFT_H264:
			codec_guid_ = MFVideoFormat_H264;
            break;
        case EncoderType::MFT_H265:
			codec_guid_ = MFVideoFormat_H265;
            break;
        case EncoderType::VPX_VP8:
            codec_guid_ = MFVideoFormat_VP80;
            break;
        case EncoderType::VPX_VP9:
			codec_guid_ = MFVideoFormat_VP90;
            break;
        case EncoderType::VPX_AV1:
			codec_guid_ = MFVideoFormat_AV1;
            break;
        default:
            break;
    }

    return SUCCEEDED(ConfigureSinkWriter());
}

bool MFTH264VideoEncoder::ProcessFrame(const std::vector<uint8_t>& image_buffer, int width, int height)
{
	return SUCCEEDED(EncodeFrame(image_buffer, width, height));
}

HRESULT MFTH264VideoEncoder::ConfigureSinkWriter() 
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

HRESULT MFTH264VideoEncoder::ReConfigureSinkWriter(int width, int height)
{
    width_ = width;
    height_ = height;

    Finalize();
    output_filename_.clear();

    output_filename_.append(RecorderUtils::GetCurrentDateTime());
	output_filename_ = output_filename_ + std::to_wstring(width) + L"x" + std::to_wstring(height) + L".mp4";

    return ConfigureSinkWriter();
}

HRESULT MFTH264VideoEncoder::ConfigureInputType()
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

HRESULT MFTH264VideoEncoder::ConfigureOutputType()
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

HRESULT MFTH264VideoEncoder::EncodeFrame(const std::vector<uint8_t>& image_buffer, int width, int height) 
{
	if (width != width_ || height != height_)
	{
        width_ = width;
        height_ = height;

		if (FAILED(ReConfigureSinkWriter(width, height))) return E_FAIL;
        
        // Reset timing on reconfiguration
        is_first_frame_ = true;
	}

    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaBuffer> buffer;

    const LONG stride = 4 * width; 
    const DWORD expected_size = stride * height;
    const DWORD buffer_size = static_cast<DWORD>(image_buffer.size());
    
    if (buffer_size < expected_size)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = MFCreateMemoryBuffer(expected_size, &buffer);
    if (FAILED(hr)) return hr;

    BYTE* dest = nullptr;
    DWORD max_len = 0;
    DWORD current_len = 0;

    hr = buffer->Lock(&dest, &max_len, &current_len);
    if (FAILED(hr)) return hr;

    hr = MFCopyImage(
        dest,                      // Destination buffer
        stride,                    // Destination stride
        image_buffer.data(),       // Source buffer (first row)
        stride,                    // Source stride (should match since we cleaned it)
        stride,                    // Width in bytes
        height                     // Height in pixels
    );

    if (FAILED(hr)) 
    {
        buffer->Unlock();
        return hr;
    }

    buffer->SetCurrentLength(expected_size);
    buffer->Unlock();

    hr = MFCreateSample(&sample);
    if (FAILED(hr)) return hr;

    sample->AddBuffer(buffer.Get());

    auto current_time = std::chrono::high_resolution_clock::now();
    
    uint64_t timestamp = 0;
    uint64_t duration = 0;
    
    if (is_first_frame_)
    {
        start_time_ = current_time;
        last_frame_time_ = current_time;
        is_first_frame_ = false;
        timestamp = 0;
        
        const uint64_t kHundredNsPerSecond = 10000000;
        duration = kHundredNsPerSecond / fps_;
    }
    else
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time_);
        timestamp = elapsed.count() * 10;
        
        // Calculate duration from last frame
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time_);
        duration = frame_duration.count() * 10; // Convert microseconds to 100-nanosecond units
        
        last_frame_time_ = current_time;
    }

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

HRESULT MFTH264VideoEncoder::Finalize() 
{
    if (sink_writer_) 
    {
        sink_writer_->Finalize();
        sink_writer_ = nullptr;
    }

    return S_OK;
}
