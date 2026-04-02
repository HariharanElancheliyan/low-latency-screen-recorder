#pragma once
#include <atomic>
#include <WTypesbase.h>

#include "mkvmuxer/mkvwriter.h"
#include "mkvmuxer/mkvmuxer.h"

#include "IVideoFileWritter.h"

namespace screen_recorder 
{
        class WebmVideoFileWritter : public screen_recorder::IVideoFileWritter
	{
	   public:
		WebmVideoFileWritter();
		virtual ~WebmVideoFileWritter();
                
        virtual bool Initialize(const std::string& folder_path, const std::string& filename,
                                        int width, int height, KVideoCodec video_codec) override;
		virtual bool WriteFrame(video_data& frame_data, uint64_t timestamp,
                                        bool is_keyframe) override;
		virtual void Shutdown() override;

		private:
                void AddLastFrame();
	   private:
        mkvmuxer::MkvWriter writer;
        mkvmuxer::Segment segment;
        uint64_t track_id;
        const uint64_t timecode_scale = 1000000;


        std::atomic<LONGLONG> start_time;
        std::atomic<LONGLONG> lastframe_time;
        std::atomic<LONGLONG> firstframe_time;
        std::atomic<bool> first_frame;

		video_data last_frame_data;	
	};
}  // namespace screen_recorder