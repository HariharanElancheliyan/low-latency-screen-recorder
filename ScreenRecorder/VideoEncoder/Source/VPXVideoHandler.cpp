#include <memory>

#include "VPXVideoHandler.h"
#include "VPXVideoEncoder.h"
#include "WebmVideoFileWritter.h"
#include "ScreenRecorderUtils.h"
#include "libyuv.h"


using namespace screen_recorder;

VPXVideoHandler::VPXVideoHandler(int width, int height, int fps, int bitrate,
	const std::wstring& output_path, const std::wstring& output_filename)
	: width(width), height(height), fps(fps), bitrate(bitrate),
	output_path_(output_path), output_filename_(output_filename),
	is_initialized(false), processing_thread_running(false)
{
	video_encoder = nullptr;
	file_writter = nullptr;
	encoder_type = EncoderType::VPX_VP8;
}

bool VPXVideoHandler::Initialize(EncoderType codec_type)
{
	// Convert wide string to narrow string for legacy interface
	std::string narrow_path(output_path_.begin(), output_path_.end());
	std::string narrow_filename(output_filename_.begin(), output_filename_.end());

	KVideoCodec vpx_codec = KVideoCodec_VP8;
	encoder_type = codec_type;

	if (codec_type == EncoderType::VPX_VP9)
	{
		vpx_codec = KVideoCodec_VP9;
	}
	else if (codec_type == EncoderType::VPX_VP9_444)
	{
		vpx_codec = KVideoCodec_VP9_444;
	}


	return Initialize(narrow_path, narrow_filename, width, height, vpx_codec, bitrate, fps);
}

bool VPXVideoHandler::ProcessFrame(const std::vector<uint8_t>& image_buffer, int width, int height)
{
	bool result = false;

	if (!is_initialized)
	{
		return false;
	}

	// Check if dimensions changed
	if (this->width != width || this->height != height)
	{
		if (!ReInitialize(width, height))
		{
			return false;
		}

	}

	if (encoder_type == EncoderType::VPX_VP9_444)
	{
		// For VP9 4:4:4, convert BGRA to I444
		size_t frame_size = width * height * 3; // 3 bytes per pixel for I444
		uint8_t* i444_buffer = new uint8_t[frame_size];

		if (libyuv::ARGBToI444(image_buffer.data(), width * 4,
			i444_buffer, width,
			i444_buffer + width * height, width,
			i444_buffer + width * height * 2, width,
			width, height) == 0)
		{

			AddFrameToQueue(i444_buffer, frame_size);
			result = true;
		}
		else
		{
			result = false;
		}

		delete[] i444_buffer;
	}
	else
	{
		// For VP8 and VP9 4:2:0, convert BGRA to NV12

		int buffer_length = width * height * 3 / 2;
		uint8_t* nv12_buffer = new uint8_t[buffer_length];
		int stride = width * 4;

		if (libyuv::ARGBToNV12(image_buffer.data(), stride, nv12_buffer, width, nv12_buffer + width * height, width, width, height) == 0)
		{
			AddFrameToQueue(nv12_buffer, buffer_length);
			result = true;
		}
		else
		{
			result = false;
		}

		delete[] nv12_buffer;
	}

	return result;
}

bool VPXVideoHandler::Initialize(const std::string& folder_path, const std::string& filename,
	int width, int height,
	KVideoCodec video_codec, long bitrate, int fps) {
	this->filename = filename;
	this->width = width;
	this->height = height;
	this->bitrate = bitrate;
	this->fps = fps;


	if (!video_encoder)
	{
		VPX_TYPE vpx_type = VPX_TYPE::VP8;

		if (video_codec == KVideoCodec_VP9)
		{
			vpx_type = VPX_TYPE::VP9_420;
		}
		else if (video_codec == KVideoCodec_VP9_444)
		{
			vpx_type = VPX_TYPE::VP9_444;
		}

		std::unique_ptr<screen_recorder::VPXVideoEncoder> vpx_encoder =
			std::make_unique<screen_recorder::VPXVideoEncoder>(vpx_type);

		video_encoder = std::move(vpx_encoder);
	}

	if (!file_writter)
	{
		std::unique_ptr<screen_recorder::IVideoFileWritter> webm_writter =
			std::make_unique<screen_recorder::WebmVideoFileWritter>();

		file_writter = std::move(webm_writter);
	}


	if (video_encoder && file_writter)
	{
		if (video_encoder->IsInitialized() == false)
		{
			video_encoder->SetWidthAndHeight(width, height);
			video_encoder->ConfigureBitrateAndFPS(bitrate, fps);

			if (!video_encoder->Initialize())
			{
				return false;
			}
		}

		if (file_writter->IsInitialized() == false)
		{
			if (!file_writter->Initialize(folder_path, filename, width, height, video_codec))
			{
				return false;
			}
		}
		is_initialized = true;
		
		// Start processing thread
		StartThreadToProcess();
		
		return true;
	}

	return false;
}

bool VPXVideoHandler::ReInitialize(int width, int height)
{
	std::lock_guard<std::mutex> lock(queue_mutex);

	if (this->width != width || this->height != height)
	{
		ClearFrameQueue();

		this->width = width;
		this->height = height;

		if (video_encoder)
		{
			video_encoder->SetWidthAndHeight(width, height);
			video_encoder->Shutdown();

			if (!video_encoder->Initialize())
			{
				return false;
			}

			return true;
		}
	}

	return false;
}

void VPXVideoHandler::AddFrameToQueue(uint8_t* frame_data, size_t frame_size)
{
	if (!video_encoder || !file_writter) {
		return;
	}

	uint8_t* frame_buffer = new uint8_t[frame_size];
	memcpy(frame_buffer, frame_data, frame_size);

	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		frame_queue.push({ frame_buffer, screen_recorder::GetCurrentNanoseconds() });
	}
}

void VPXVideoHandler::ClearFrameQueue()
{
	if (frame_queue.empty())
	{
		return;
	}

	std::lock_guard<std::mutex> lock(queue_mutex);
	while (!frame_queue.empty())
	{
		auto frame = frame_queue.front();
		if (frame.first)
		{
			delete[] frame.first;
			frame.first = nullptr;
		}
		frame_queue.pop();
	}
}

void VPXVideoHandler::StartThreadToProcess()
{
	processing_thread_running = true;
	processing_thread = std::thread(&VPXVideoHandler::ProcessingThread, this);
	processing_thread.detach();
}

void VPXVideoHandler::ProcessingThread()
{
	if (!video_encoder || !file_writter)
	{
		return;
	}

	while (processing_thread_running || !frame_queue.empty())
	{
		std::pair<uint8_t*, LONGLONG> frame;

		{
			// Lock only for access the front, Do not include processing inside the lock
			std::lock_guard<std::mutex> lock(queue_mutex);

			if (!frame_queue.empty()) {
				frame = frame_queue.front();
				frame_queue.pop();

			}
			else {
				continue;
			}
		}

		if (frame.first) {
			EncodeAndWriteFrame(frame.first, frame.second);
		}
	}

	if (frame_queue.empty())
	{
		queue_cv.notify_all();
	}
}

bool VPXVideoHandler::EncodeAndWriteFrame(uint8_t* frame_data, uint64_t frame_time)
{
	if (video_encoder && file_writter)
	{
		video_data video_data;
		bool keyframe = false;

		if (!video_encoder->EncodeFrame(frame_data, keyframe, video_data))
		{
			return false;
		}

		if (!file_writter->WriteFrame(video_data, frame_time, keyframe)) {
			return false;
		}

		video_data.Reset();
		if (frame_data)
		{
			delete[] frame_data;
			frame_data = nullptr;
		}

		return true;
	}


	return false;
}


void VPXVideoHandler::StopThread() { processing_thread_running = false; }

HRESULT VPXVideoHandler::Finalize()
{
	StopThread();

	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		queue_cv.wait(lock, [this] { return frame_queue.empty(); });
	}

	if (is_initialized)
	{
		if (video_encoder) {
			video_encoder->Shutdown();
			video_encoder = nullptr;
		}
		if (file_writter) {
			file_writter->Shutdown();
			file_writter = nullptr;
		}

		is_initialized = false;
	}

	return S_OK;
}