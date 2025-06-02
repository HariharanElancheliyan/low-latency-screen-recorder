#include <iostream>
#include <shlobj.h> 

#include "CaptureEngine.h"
#include "VideoEncoder.h"
#include "ScreenRecorder.h"
#include "Utils.h"


#define APPLICATION_FOLDER_NAME L"ZAScreenRecorder"


ScreenRecorder::ScreenRecorder()
{
	monitor_number_ = 1;
	width_ = 1920;
	height_ = 1080;
	fps_ = 60;
	bitrate_ = 8000000;
	is_initialized_ = false;
}

ScreenRecorder::~ScreenRecorder()
{
	if (video_encoder_)
	{
		video_encoder_->Finalize();
	}
	if (capture_engine_)
	{
		capture_engine_->StopCapture();
	}

	if (video_encoder_)
	{
		video_encoder_.reset();
		video_encoder_ = nullptr;
	}

	if (capture_engine_)
	{
		capture_engine_.reset();
		capture_engine_ = nullptr;
	}

	is_initialized_ = false;
}

bool ScreenRecorder::Initialize(int monitor_number, int width, int height, int fps, int bitrate)
{
	monitor_number_ = monitor_number;
	width_ = width;
	height_ = height;
	bitrate_ = bitrate;
	fps_ = fps;
	
	GetOutputFileName(output_filename_);

	video_encoder_ = std::make_shared<VideoEncoder>(width_, height_, fps_, bitrate_, output_path_, output_filename_);
	if (!video_encoder_->Initialize(VideoCodec::H264))
	{
		return false;
	}

	capture_engine_ = std::make_shared<CaptureEngine>(monitor_number_, width_, height_);
	if (!capture_engine_->Initialize())
	{
		return false;
	}

	is_initialized_ = true;
	return true;
}

bool ScreenRecorder::StartMonitorCapture(HMONITOR monitor)
{
	if (!capture_engine_) return false;
	if (!video_encoder_) return false;

	if (!capture_engine_->CaptureMonitor(monitor))
	{
		return false;
	}

	capture_engine_->StartCapture();
	capture_engine_->SetOutputCallback(std::bind(&VideoEncoder::ProcessFrame, video_encoder_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	return true;
}

bool ScreenRecorder::StartWindowCapture(HWND window_handle)
{
	if (!capture_engine_) return false;
	if (!video_encoder_) return false;

	if (!capture_engine_->CaptureWindow(window_handle))
	{
		return false;
	}

	capture_engine_->StartCapture();
	capture_engine_->SetOutputCallback(std::bind(&VideoEncoder::ProcessFrame, video_encoder_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	return true;

}

bool ScreenRecorder::StopCapture()
{
	if (!capture_engine_) return false;
	if (!video_encoder_) return false;

	if (video_encoder_)
	{
		video_encoder_->Finalize();
	}

	if (capture_engine_)
	{
		capture_engine_->StopCapture();
	}

	return true;
}

bool ScreenRecorder::CreateOutputFolder(const std::wstring& folder_path)
{
	PWSTR user_video_folder;
	std::wstring output_folder_path;
	
	if (folder_path.empty())
	{
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &user_video_folder);

		if (SUCCEEDED(hr))
		{
			output_folder_path = user_video_folder;
		}

		CoTaskMemFree(user_video_folder);
	}
	else
	{
		output_folder_path = folder_path;
	}


	return CreateAndGetApplicationDirectoryPath(APPLICATION_FOLDER_NAME, output_folder_path, output_path_);
}


bool ScreenRecorder::CreateAndGetApplicationDirectoryPath(const std::wstring& folder_name, const std::wstring folder_path, std::wstring& output_full_path)
{
	output_full_path = folder_path + L"\\" + folder_name + L"\\";
	return CreateDirectoryW(output_full_path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;

	return false;
}

bool ScreenRecorder::GetOutputFileName(std::wstring& file_name)
{
	file_name.clear();
	file_name.append(RecorderUtils::GetCurrentDateTime());
	file_name.append(L".mp4");

	return file_name.size() > 0;
}



