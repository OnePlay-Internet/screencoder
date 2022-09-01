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
                    Config config) 
    {
        platf::Display** disps = display::get_all_display(encoder);
        platf::Display* disp = *(disps);

        if(!disp)
            return FALSE;

        

        platf::PixelFormat pix_fmt  = config.dynamicRangeOption == DynamicRange::ENABLE? 
                                        platf::map_pix_fmt(encoder->dynamic_pix_fmt):
                                        platf::map_pix_fmt(encoder->static_pix_fmt);

        platf::Device* device = disp->klass->make_hwdevice(disp,pix_fmt);
        if(!device) {
            return FALSE;
        }
        
        EncodeContext* session = make_encode_context(encoder, &config,
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

    Capabilities
    validate_encoder(Encoder* encoder) 
    {
        Config config = {0};
        config.bitrate = 1000;

        encoder->codec_config.capabilities.set().flip(); // assume all capabilities is not meet

        { // basic test case
            encoder->codec_config.capabilities[FrameFlags::PASSED] = TRUE;
            config.avcolor = (AVColorRange)encoder::LibavColor::JPEG;
            config.scalecolor = encoder::LibscaleColor::REC_601;
            config.dynamicRangeOption = DynamicRange::DISABLE;
            config.slicesPerFrame = SlicePerFrame::ONE;
            bool result = validate_config(encoder,config);
            if (!result) {
                encoder->codec_config.capabilities[FrameFlags::PASSED] = FALSE;
                goto done;
            }
        }

        { // basic test case
            encoder->codec_config.capabilities[FrameFlags::CBR] = TRUE;
            config.avcolor = (AVColorRange)encoder::LibavColor::JPEG;
            config.scalecolor = encoder::LibscaleColor::REC_601;
            config.dynamicRangeOption = DynamicRange::DISABLE;
            config.slicesPerFrame = SlicePerFrame::ONE;
            bool result = validate_config(encoder,config);
            if (!result) {
                encoder->codec_config.capabilities[FrameFlags::CBR] = FALSE;
            }
        }

        { // basic test case
            encoder->codec_config.capabilities[FrameFlags::DYNAMIC_RANGE] = TRUE;
            config.avcolor = (AVColorRange)encoder::LibavColor::JPEG;
            config.scalecolor = encoder::LibscaleColor::REC_2020;
            config.dynamicRangeOption = DynamicRange::ENABLE;
            config.slicesPerFrame = SlicePerFrame::ONE;
            bool result = validate_config(encoder,config);
            if (!result) {
                encoder->codec_config.capabilities[FrameFlags::DYNAMIC_RANGE] = FALSE;
            }
        }

        { // basic test case
            encoder->codec_config.capabilities[FrameFlags::SLICE] = TRUE;
            config.avcolor = (AVColorRange)encoder::LibavColor::JPEG;
            config.scalecolor = encoder::LibscaleColor::REC_601;
            config.dynamicRangeOption = DynamicRange::DISABLE;
            config.slicesPerFrame = SlicePerFrame::TWO;
            bool result = validate_config(encoder,config);
            if (!result) {
                encoder->codec_config.capabilities[FrameFlags::SLICE] = FALSE;
            }
        }

    done:
        return encoder->codec_config.capabilities;
    }
}
