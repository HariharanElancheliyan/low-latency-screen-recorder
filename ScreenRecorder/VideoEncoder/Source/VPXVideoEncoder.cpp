#include "VPXVideoEncoder.h"

using namespace screen_recorder;

VPXVideoEncoder::VPXVideoEncoder() 
{
    main_stream_width_ = 1920;
    main_stream_height_ = 1200;

    fps = 30;
    bitrate = 8000;
    keyframe = true;
    quality_mode = false;

    is_initialized = false;
    frame_count = 0;
    vpx_encoder = VPX_TYPE::VP9_444;
    image_format = VPX_IMG_FMT_I444;
}

VPXVideoEncoder::VPXVideoEncoder(VPX_TYPE vpx_encoder) 
{
    main_stream_width_ = 1920;
    main_stream_height_ = 1200;

    fps = 30;
    bitrate = 8000;
    keyframe = true;
    quality_mode = false;

    is_initialized = false;
    frame_count = 0;
    this->vpx_encoder = vpx_encoder;
    image_format = VPX_IMG_FMT_I444;
}

VPXVideoEncoder::~VPXVideoEncoder() { Shutdown(); }

bool VPXVideoEncoder::Initialize() 
{
    std::lock_guard<std::mutex> lock(encoder_mutex);

    if ((vpx_encoder == VPX_TYPE::VP9_420) || (vpx_encoder == VPX_TYPE::VP9_444))
    {
        iface = vpx_codec_vp9_cx();
    } 
    else 
    {
        iface = vpx_codec_vp8_cx();
    }

    vpx_codec_enc_config_default(iface, &cfg, 0);

    cfg.g_w = main_stream_width_;
    cfg.g_h = main_stream_height_;
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = fps;
    cfg.rc_target_bitrate = static_cast<unsigned int>(bitrate);  // in kbps
    cfg.g_pass = VPX_RC_ONE_PASS;
    cfg.g_bit_depth = VPX_BITS_8;
    cfg.g_threads = 1;
    cfg.g_lag_in_frames = 0;
    cfg.g_profile = 0;
    cfg.kf_mode = VPX_KF_AUTO;

    cfg.kf_max_dist = cfg.kf_min_dist = 150;
    cfg.rc_end_usage = VPX_VBR;

    if (vpx_encoder == VPX_TYPE::VP9_444) 
    {
        cfg.g_profile = 1;
        image_format = VPX_IMG_FMT_I444;
    } else 
    {
        image_format = VPX_IMG_FMT_NV12;
    }

    if (quality_mode) {
        cfg.rc_min_quantizer = 10;
        cfg.rc_max_quantizer = 30;
        cfg.rc_end_usage = VPX_CQ;
    }


    // Initialize encoder
    if (vpx_codec_enc_init(&codec, iface, &cfg, 0) == VPX_CODEC_OK) 
    {
        if (vpx_encoder == VPX_TYPE::VP9_444 || vpx_encoder == VPX_TYPE::VP9_420) 
        {
            vpx_codec_control(&codec, VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);
            vpx_codec_control(&codec, VP9E_SET_ROW_MT, 1);
            vpx_codec_control(&codec, VP9E_SET_AQ_MODE, 0);
            vpx_codec_control(&codec, VP8E_SET_CPUUSED, 9);  // Max value is 9

            // vpx_codec_control(&codec, VP9E_SET_LOSSLESS, quality_mode); Takes lot of bandwidth,

        } 
        else 
        {
            const unsigned int screen_content_mode = 1;  // Enable screen content mode
            vpx_codec_control(&codec, VP8E_SET_SCREEN_CONTENT_MODE, screen_content_mode);
            vpx_codec_control(&codec, VP8E_SET_CPUUSED, 16);  // Max value is 16
        }

        is_initialized = keyframe = true;

    }

    return is_initialized;
}



void VPXVideoEncoder::Shutdown() 
{
    std::lock_guard<std::mutex> lock(encoder_mutex);
    if (is_initialized) 
    {
        vpx_codec_destroy(&codec);
        is_initialized = false;
        quality_mode = false;
        keyframe = true;

        iface = nullptr;

    }
}

void VPXVideoEncoder::SetWidthAndHeight(int width, int height) 
{
    main_stream_width_ = width;
    main_stream_height_ = height;
}

bool VPXVideoEncoder::ConfigureBitrateAndFPS(unsigned long long bitrate, int fps) 
{
    std::lock_guard<std::mutex> lock(encoder_mutex);

    if (bitrate > 0 && bitrate != this->bitrate) 
    {
        this->bitrate = (bitrate / 1000);
    }

    if (fps > 0 && fps != this->fps) 
    {
        this->fps = fps;
    }

    if (bitrate > 7000000) 
    {
        quality_mode = true;
    }

    if (is_initialized) 
    {
        Shutdown();
        return Initialize();
    }

	return true;
}

bool VPXVideoEncoder::EncodeFrame(uint8_t* frame_data, bool& keyframe, video_data& encoded_data) 
{
    std::lock_guard<std::mutex> lock(encoder_mutex);
    bool result = false;

    if (!is_initialized) {
        return result;
    }

    vpx_image_t vpx_image;
    if (vpx_img_wrap(&vpx_image, image_format, main_stream_width_, main_stream_height_, 1,
                     frame_data)) {
        vpx_codec_iter_t iter = NULL;
        const vpx_codec_cx_pkt_t* pkt = NULL;

        vpx_enc_frame_flags_t flag = keyframe ? VPX_EFLAG_FORCE_KF : 0;
        vpx_codec_err_t enc_result =
            vpx_codec_encode(&codec, &vpx_image, frame_count++, 1, flag, VPX_DL_REALTIME);

        if (enc_result == VPX_CODEC_OK) {
            while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) != nullptr) {
                if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
                    encoded_data.compressed_data.insert(
                        encoded_data.compressed_data.end(), (byte*)pkt->data.frame.buf,
                        (byte*)pkt->data.frame.buf + pkt->data.frame.sz);
                    encoded_data.size = pkt->data.frame.sz;

                    if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
                        keyframe = true;
                    }
                }
            }

            result = true;
        }
    }

    vpx_img_free(&vpx_image);

    return result;
}
