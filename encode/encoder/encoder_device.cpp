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
#include <sunshine_util.h>


#include <platform_common.h>
#include <sunshine_config.h>

#include <encoder_session.h>
#include <encoder_thread.h>
#include <sunshine_rtp.h>





namespace encoder
{
    bool
    validate_config(Encoder* encoder, 
                    Config* config) 
    {
        int size;
        int res, ret;
        libav::Packet* av_packet   = NULL;
        libav::Frame* frame = NULL;
        libav::FormatContext* fmtctx   = NULL;
        libav::Stream* stream  = NULL;

        util::Buffer* obj  = NULL;
        util::Buffer* obj_ses = NULL;
        util::QueueArray* packets = NULL;

        Session* session = NULL;
        platf::Image* img = NULL;

        platf::Display* disp = platf::tryget_display( encoder->dev_type, ENCODER_CONFIG->output_name, config->framerate);
        if(!disp) 
            return FALSE;
        

        platf::PixelFormat pix_fmt  = config->dynamicRange == 0 ? 
                                        platf::map_pix_fmt(encoder->static_pix_fmt) : 
                                        platf::map_pix_fmt(encoder->dynamic_pix_fmt);

        platf::Device* device = disp->klass->make_hwdevice(disp,pix_fmt);
        if(!device) 
        {
            LOG_ERROR("Fail to create hwdevice");
            return FALSE;
        }
        
        session = make_session(encoder, config, disp->width, disp->height, device);
        if(!session)
        {
            session_finalize(session);
            return FALSE;
        }
        
        img = disp->klass->alloc_img(disp);
        if(!img || disp->klass->dummy_img(disp,img)) 
        {
            session_finalize(session);
            return FALSE;
        }
        
        if(device->klass->convert(device,img)) 
        {
            session_finalize(session);
            return FALSE;
        }
        

        frame = device->frame;
        frame->pict_type = AV_PICTURE_TYPE_I;

        packets = QUEUE_ARRAY_CLASS->init();
        obj_ses = BUFFER_CLASS->init(session,sizeof(Session),session_finalize);
        while(!QUEUE_ARRAY_CLASS->peek(packets)) {
            if(!encode(1, obj_ses, frame, packets)) 
            {
                LOG_ERROR("fail to encode");
                QUEUE_ARRAY_CLASS->stop(packets);
                BUFFER_CLASS->unref(obj_ses);
                return FALSE;
            }
        }

        av_packet = (libav::Packet*)QUEUE_ARRAY_CLASS->pop(packets,&obj,&size);
        if(size != sizeof(libav::Packet)) {
            LOG_ERROR("wrong datatype");
            QUEUE_ARRAY_CLASS->stop(packets);
            BUFFER_CLASS->unref(obj_ses);
            return FALSE;
        }

        if(!(av_packet->flags & AV_PKT_FLAG_KEY)) {
            LOG_ERROR("First packet type is not an IDR frame");
            QUEUE_ARRAY_CLASS->stop(packets);
            BUFFER_CLASS->unref(obj);
            BUFFER_CLASS->unref(obj_ses);
            return FALSE;
        }

        fmtctx = avformat_alloc_context();
        fmtctx->oformat = av_guess_format("rtp",NULL,NULL);
        snprintf(fmtctx->filename, sizeof(fmtctx->filename), 
            "rtp://%s:%d", "localhost", ENCODER_CONFIG->rtp.port);

        stream = avformat_new_stream (fmtctx, session->encode->codec);

        avcodec_parameters_from_context(stream->codecpar,session->encode->context);
        if (fmtctx->oformat->flags & AVFMT_GLOBALHEADER)
            session->encode->context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;


        fmtctx->streams[0] = stream;
        if (avio_open(&fmtctx->pb, fmtctx->filename, AVIO_FLAG_WRITE) < 0){
            LOG_ERROR("Error opening output file");
            avformat_free_context(fmtctx);
            QUEUE_ARRAY_CLASS->stop(packets);
            BUFFER_CLASS->unref(obj);
            BUFFER_CLASS->unref(obj_ses);
            return FALSE;
        }

        if (avformat_write_header(fmtctx, NULL) < 0){
            LOG_ERROR("Error writing header");
            avformat_free_context(fmtctx);
            QUEUE_ARRAY_CLASS->stop(packets);
            BUFFER_CLASS->unref(obj);
            BUFFER_CLASS->unref(obj_ses);
            return FALSE;
        }

        res = av_write_frame(fmtctx, av_packet);
        if(res != 0) {
            LOG_ERROR("write failed");
            avformat_free_context(fmtctx);
            QUEUE_ARRAY_CLASS->stop(packets);
            BUFFER_CLASS->unref(obj);
            BUFFER_CLASS->unref(obj_ses);
            return FALSE;
        }

        avformat_free_context(fmtctx);
        QUEUE_ARRAY_CLASS->stop(packets);
        BUFFER_CLASS->unref(obj);
        BUFFER_CLASS->unref(obj_ses);
        return TRUE;
    }

    bool 
    validate_encoder(Encoder* encoder) 
    {
        encoder->h264.capabilities.set();
        encoder->hevc.capabilities.set();

        // First, test encoder viability
        {
            Config config_max_ref_frames { 1920, 1080, 60, 1000, 1, 1, 1, 0, 0 };
            Config config_autoselect { 1920, 1080, 60, 1000, 1, 0, 1, 0, 0 };

            retry:
            auto max_ref_frames_h264 = validate_config(encoder, &config_max_ref_frames);
            auto autoselect_h264     = validate_config(encoder, &config_autoselect);

            if(!max_ref_frames_h264 && !autoselect_h264 ) {
                if(encoder->h264.qp && encoder->h264.capabilities[FrameFlags::CBR]) 
                {
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
            Config config_max_ref_frames { 1920, 1080, 60, 1000, 1, 1, 1, 0, 0 };
            Config config_autoselect { 1920, 1080, 60, 1000, 1, 0, 1, 0, 0 };

            config_max_ref_frames.videoFormat = 1;
            config_autoselect.videoFormat     = 1;

            retry_hevc:
            auto max_ref_frames_hevc = validate_config(encoder, &config_max_ref_frames);
            auto autoselect_hevc     = validate_config(encoder, &config_autoselect);

            // If HEVC must be supported, but it is not supported
            if(!max_ref_frames_hevc && !autoselect_hevc) {
                if(encoder->hevc.qp && encoder->hevc.capabilities[FrameFlags::CBR]) {
                    // It's possible the encoder isn't accepting Constant Bit Rate. Turn off CBR and make another attempt
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
            std::vector<std::pair<FrameFlags, Config>> configs {
                { FrameFlags::DYNAMIC_RANGE, { 1920, 1080, 60, 1000, 1, 0, 3, 1, 1 } },
            };

            if (encoder->flags[SINGLE_SLICE_ONLY])
            {
                encoder->h264.capabilities[FrameFlags::SLICE] = false;
                encoder->hevc.capabilities[FrameFlags::SLICE] = false;
            } else {
                configs.emplace_back( std::pair<FrameFlags, Config> { 
                    FrameFlags::SLICE, { 1920, 1080, 60, 1000, 2, 1, 1, 0, 0 } 
                });
            }


            for(auto &[flag, config] : configs) {
                auto h264 = config;
                auto hevc = config;

                h264.videoFormat = 0;
                hevc.videoFormat = 1;

                encoder->h264.capabilities[flag] = validate_config(encoder, &h264);
                if(encoder->hevc.capabilities[FrameFlags::PASSED]) {
                    encoder->hevc.capabilities[flag] = validate_config(encoder, &hevc);
                }
            }
        }

        return true;
    }

}
