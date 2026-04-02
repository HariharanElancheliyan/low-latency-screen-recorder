#pragma once
#include <string>


#include "encoder_types.h"
#include "video_data.h"

#define SCREEN_RECORDING_FOLDER_PATH "ScreenRecordings/"

namespace screen_recorder 
{
	inline std::string ConstructFileName(const std::string folder_path, const std::string& base_name,
									const std::string& extension) 
	{
		std::string full_name = folder_path;
		
		if (!full_name.empty() && full_name.back() != '\\' && full_name.back() != '/') 
		{
			full_name += '\\';
		}
		
		full_name += base_name + extension;
		return full_name;
	}

	class IVideoFileWritter 
	{
	   public:
        virtual ~IVideoFileWritter() = default;
            virtual bool Initialize(const std::string& folder_path, const std::string& filename,
                                    int width, int height, KVideoCodec video_codec) = 0;
		virtual bool WriteFrame(video_data& frame_data, uint64_t timestamp, bool is_keyframe) = 0;
		virtual void Shutdown() = 0;
		virtual bool IsInitialized() const { return is_initialized; }

		protected:
            std::string folder_path = SCREEN_RECORDING_FOLDER_PATH;
			std::string filename;
            std::string file_extension;

			int width;
			int height;


			bool is_initialized = false;


	};
}