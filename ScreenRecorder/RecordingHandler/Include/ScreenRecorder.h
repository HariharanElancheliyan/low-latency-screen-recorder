#pragma once
#include <memory>
#include <string>
#include "CaptureEngine.h"
#include "VideoEncoder.h"


struct RecordingParams
{
	int monitor_number;
	int width;
	int height;
	int bitrate;
};

class ScreenRecorder
{
public:
	ScreenRecorder();
	~ScreenRecorder();
	
	bool Initialize(int monitor_number, int width, int height, int fps, int bitrate);

	bool StartMonitorCapture(HMONITOR monitor);
	bool StartWindowCapture(HWND window_handle);
	bool StopCapture();
	bool CreateOutputFolder(const std::wstring& folder_path);
	std::wstring GetOutputPath() const { return output_path_; }
	bool IsInitialized() const { return is_initialized_; }

private:
	bool CreateAndGetApplicationDirectoryPath(const std::wstring& folder_name, const std::wstring folder_path,  std::wstring& output_full_path);
	bool GetOutputFileName(std::wstring& file_name);

private:
	std::shared_ptr<CaptureEngine> capture_engine_;
	std::shared_ptr<VideoEncoder> video_encoder_;
	int width_;
	int height_;
	std::wstring output_path_;
	std::wstring output_filename_;
	int fps_;
	int bitrate_;
	int monitor_number_;
	bool is_initialized_;

};

