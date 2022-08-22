/**
 * @file encoder_device.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <encoder_device.h>
#include <screencoder_util.h>


#include <platform_common.h>
#include <screencoder_config.h>

#include <encoder_session.h>
#include <encoder_thread.h>
#include <screencoder_rtp.h>

#include <display_base.h>




namespace encoder
{
    bool
    validate_config(Encoder* encoder, 
                    Config* config) 
    {
        platf::Display** disps = display::get_all_display(encoder);
        platf::Display* disp = *(disps);

        if(!disp)
            return FALSE;

        

        platf::PixelFormat pix_fmt  = !config->enableDynamicRange ? 
                                        platf::map_pix_fmt(encoder->static_pix_fmt) : 
                                        platf::map_pix_fmt(encoder->dynamic_pix_fmt);

        platf::Device* device = disp->klass->make_hwdevice(disp,pix_fmt);
        if(!device) {
            return FALSE;
        }
        
        EncodeContext* session = make_encode_context(encoder, 
                               disp->width, 
                               disp->height, 
                               disp->framerate,
                               device);
        if(!session) 
            return FALSE;

        util::Buffer* frameBuf = make_avframe_buffer(session);
        
        
        util::Buffer* imgBuf = disp->klass->alloc_img(disp);

        platf::Image* img1 = (platf::Image*)BUFFER_REF(imgBuf,NULL);
        error::Error err = disp->klass->dummy_img(disp,img1);
        BUFFER_UNREF(imgBuf);

        if(FILTER_ERROR(err)) {
            free_encode_context(session);
            return FALSE;
        }

        platf::Image* img2 = (platf::Image*)BUFFER_REF(imgBuf,NULL);
        libav::Frame* frame0 = (libav::Frame*)BUFFER_REF(frameBuf,NULL);
        err = device->klass->convert(device,img2,frame0);
        BUFFER_UNREF(frameBuf);
        BUFFER_UNREF(imgBuf);

        if(FILTER_ERROR(err)) {
            free_encode_context(session);
            return FALSE;
        }


        util::Buffer* pktBuf;
    retry:
        libav::Frame* frame = (libav::Frame*)BUFFER_REF(frameBuf,NULL);
        frame->pict_type = AV_PICTURE_TYPE_I;
        pktBuf = encode(1, session, frame);
        BUFFER_UNREF(frameBuf);

        if(FILTER_ERROR(pktBuf)) {
            if(FILTER_ERROR(pktBuf) == error::Error::ENCODER_NEED_MORE_FRAME) {
                goto retry;
            }
            free_encode_context(session);
            return FALSE;
        }

        libav::Packet* av_packet = (libav::Packet*)BUFFER_REF(pktBuf,NULL);
        if(!(av_packet->flags & AV_PKT_FLAG_KEY)) {
            free_encode_context(session);
            return FALSE;
        }

        free_encode_context(session);
        return TRUE;
    }

    bool 
    validate_encoder(Encoder* encoder) 
    {
        encoder->h264.capabilities.set();
        encoder->hevc.capabilities.set();

        // First, test encoder viability
        {
            Config config_max_ref_frames { 1000, 1, 1, 1, VideoFormat::H264, 0 };
            Config config_autoselect     { 1000, 1, 0, 1, VideoFormat::H264, 0 };

            retry:
            auto max_ref_frames_h264 = validate_config(encoder, &config_max_ref_frames);
            auto autoselect_h264     = validate_config(encoder, &config_autoselect);

            if(!max_ref_frames_h264 && !autoselect_h264 ) {
                if(encoder->h264.qp && encoder->h264.capabilities[FrameFlags::CBR]) {
                    // It's possible the encoder isn't accepting Constant Bit Rate. Turn off CBR and make another attempt
                    encoder->h264.capabilities.set();
                    encoder->h264.capabilities[FrameFlags::CBR] = FALSE;
                    goto retry;
                }
                return false;
            }


            encoder->h264.capabilities[FrameFlags::REF_FRAMES_RESTRICT]   = max_ref_frames_h264;
            encoder->h264.capabilities[FrameFlags::REF_FRAMES_AUTOSELECT] = autoselect_h264;
            encoder->h264.capabilities[FrameFlags::PASSED]                = true;
        }
        
        {
            Config config_max_ref_frames { 1000, 1, 1, 1, VideoFormat::H265, 0 };
            Config config_autoselect     { 1000, 1, 0, 1, VideoFormat::H265, 0 };

            retry_hevc:
            auto max_ref_frames_hevc = validate_config(encoder, &config_max_ref_frames);
            auto autoselect_hevc     = validate_config(encoder, &config_autoselect);

            // If HEVC must be supported, but it is not supported
            if(!max_ref_frames_hevc && !autoselect_hevc) {
                if(encoder->hevc.qp && encoder->hevc.capabilities[FrameFlags::CBR]) {
                    // It's possible the encoder isn't accepting Constant Bit Rate. 
                    // Turn off CBR and make another attempt
                    encoder->hevc.capabilities.set();
                    encoder->hevc.capabilities[FrameFlags::CBR] = false;
                    goto retry_hevc;
                }
            }

            encoder->hevc.capabilities[FrameFlags::REF_FRAMES_RESTRICT]   = max_ref_frames_hevc ;
            encoder->hevc.capabilities[FrameFlags::REF_FRAMES_AUTOSELECT] = autoselect_hevc;
            encoder->hevc.capabilities[FrameFlags::PASSED] = max_ref_frames_hevc || autoselect_hevc;
        }

        // test DYNAMIC_RANGE and SLICE
        {
            Config configs[2] = { 
                { 1000, 1, 0, 3, VideoFormat::UNKNOWN, 1 }, 
                { 1000, 2, 1, 1, VideoFormat::UNKNOWN, 0 }
            };
            FrameFlags flags[2] = {
                FrameFlags::DYNAMIC_RANGE,
                FrameFlags::SLICE
            };

            for (int i = 0; i < 2; i++)
            {
                FrameFlags flag = flags[i];
                Config config = configs[i];
                auto h264 = config;
                auto hevc = config;

                h264.videoFormat = VideoFormat::H264;
                hevc.videoFormat = VideoFormat::H265;

                encoder->h264.capabilities[flag] = validate_config(encoder, &h264);
                if(encoder->hevc.capabilities[FrameFlags::PASSED]) {
                    encoder->hevc.capabilities[flag] = validate_config(encoder, &hevc);
                }
            }
        }

        return true;
    }

}
