#include "WebmVideoFileWritter.h"
#include "ScreenRecorderUtils.h"

screen_recorder::WebmVideoFileWritter::WebmVideoFileWritter() 
{ 
    first_frame = true;
    filename = "RecordingFile";
    file_extension = ".webm";
    width = 1920;
    height = 1080;
    track_id = 0;
}

screen_recorder::WebmVideoFileWritter::~WebmVideoFileWritter() { Shutdown(); }

bool screen_recorder::WebmVideoFileWritter::Initialize(const std::string& folder_path,
                                                          const std::string& filename, int width,
                                                          int height, KVideoCodec video_codec) {
    this->filename = filename;
    this->width = width;
    this->height = height;
    this->folder_path = folder_path;

    std::string full_path = ConstructFileName(folder_path, this->filename, file_extension);

    if (!writer.Open(full_path.c_str())) 
    {
        return false;
    }

    if (!segment.Init(&writer)) 
    {
        return false;
    }

    segment.set_mode(mkvmuxer::Segment::kFile);
    segment.OutputCues(true);
    track_id = segment.AddVideoTrack(static_cast<int32_t>(width), static_cast<int32_t>(height), static_cast<int32_t>(track_id));
    if (track_id == 0) {
        return false;
    }

    mkvmuxer::VideoTrack* const video =
        static_cast<mkvmuxer::VideoTrack*>(segment.GetTrackByNumber(track_id));

    if (video_codec == KVideoCodec::KVideoCodec_VP8) {
        video->set_codec_id(mkvmuxer::Tracks::kVp8CodecId);
    }
    else if (video_codec == KVideoCodec::KVideoCodec_VP9 || video_codec == KVideoCodec::KVideoCodec_VP9_444) {
        video->set_codec_id(mkvmuxer::Tracks::kVp9CodecId);
    } 
    else if (video_codec == KVideoCodec::KVideoCodec_AV1) {
        video->set_codec_id(mkvmuxer::Tracks::kAv1CodecId);
    } 
    else {
        return false;
    }

    mkvmuxer::SegmentInfo* info = segment.GetSegmentInfo();
    info->set_timecode_scale(timecode_scale);
    info->set_writing_app("ScreenRecorder");

    is_initialized = true;
    return true;
}

bool screen_recorder::WebmVideoFileWritter::WriteFrame(video_data& frame_data,
                                                          uint64_t frame_time, bool is_keyframe) 
{
    if (first_frame) 
    {
        start_time = frame_time;
        lastframe_time = frame_time;
        first_frame = false;
    }

    uint64_t timecode = frame_time - start_time;

    if (!segment.AddFrame(frame_data.compressed_data.data(), frame_data.size, track_id, timecode,
                          is_keyframe)) {
        return false;
    }

    lastframe_time = frame_time;

    return true;
}

void screen_recorder::WebmVideoFileWritter::Shutdown() 
{
    if (is_initialized) 
	{
		AddLastFrame();
        segment.Finalize();
        writer.Close();
    }
}

void screen_recorder::WebmVideoFileWritter::AddLastFrame() 
{
	if (!last_frame_data.compressed_data.empty())
	{
        WriteFrame(last_frame_data, screen_recorder::GetCurrentNanoseconds() , false);
	}
}
