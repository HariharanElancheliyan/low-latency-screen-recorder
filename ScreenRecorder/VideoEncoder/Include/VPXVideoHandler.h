#pragma once
#include <memory>
#include <chrono>
#include <queue>

#include "IVideoEncoder.h"
#include "VPXVideoEncoder.h"
#include "IVideoFileWritter.h"

class VPXVideoHandler : public IVideoEncoder
{
public:
	VPXVideoHandler() {
		video_encoder = nullptr;
		file_writter = nullptr;
	}

	VPXVideoHandler(int width, int height)
		: width(width), height(height), is_initialized(false), bitrate(700000), fps(24) {
		video_encoder = nullptr;
		file_writter = nullptr;
	}

	VPXVideoHandler(int width, int height, int fps, int bitrate, 
		const std::wstring& output_path, const std::wstring& output_filename);

	~VPXVideoHandler() override {}

	bool Initialize(EncoderType enc_type) override;
	bool Initialize(const std::string& folder_path, const std::string& filename, int width,
		int height,
		screen_recorder::KVideoCodec video_codec, long bitrate, int fps);

	bool ProcessFrame(const std::vector<uint8_t>& image_buffer, int width, int height) override;
	HRESULT Finalize() override;

	bool ReInitialize(int width, int height);
	bool IsInitialized() const { return is_initialized; }

	void AddFrameToQueue(uint8_t* frame_data, size_t frame_size);
	void ClearFrameQueue();

	void StartThreadToProcess();
	void ProcessingThread();
	void StopThread();

private:
	bool EncodeAndWriteFrame(uint8_t* frame_data, uint64_t frame_time);

private:
	std::unique_ptr<screen_recorder::VPXVideoEncoder> video_encoder;
	std::unique_ptr<screen_recorder::IVideoFileWritter> file_writter;

	int width;
	int height;
	std::string filename;
	std::wstring output_path_;
	std::wstring output_filename_;
	long bitrate;
	int fps;

	std::mutex queue_mutex;
	std::condition_variable queue_cv;
	std::queue<std::pair<uint8_t*, uint64_t>> frame_queue;  // <frame_buffer, timestamp>
	std::atomic<bool> processing_thread_running;
	std::thread processing_thread;
	EncoderType encoder_type;

	bool is_initialized = false;
};
