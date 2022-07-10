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

#include <encoder_packet.h>

#include <common.h>
#include <sunshine_config.h>
#include <sunshine_bitstream.h>
#include <encoder.h>




namespace encoder
{
    enum ValidateFlags{
        VUI_PARAMS     = 0x01,
        NALU_PREFIX_5b = 0x02,
    };

    int 
    validate_config(platf::Display* disp, 
                    Encoder* encoder, 
                    config::Encoder* config) 
    {
        reset_display(disp, encoder->dev_type, config->output_name, config->framerate);
        if(!disp) {
            return -1;
        }

        platf::PixelFormat pix_fmt  = config.dynamicRange == 0 ? 
                                        map_pix_fmt(encoder.static_pix_fmt) : 
                                        map_pix_fmt(encoder.dynamic_pix_fmt);

        platf::HWDevice* hwdevice = disp->klass->make_hwdevice(disp,pix_fmt);
        if(!hwdevice) {
            return -1;
        }

        Session* session = NULL;
        make_session(session, encoder, config, disp->width, disp->height, hwdevice);
        if(!session) {
            return -1;
        }

        platf::Image* img = disp->klass->alloc_img(disp);

        if(!img || disp->klass->dummy_img(disp,img)) 
            return -1;
        
        if(session->device->klass->convert(session->device,img)) 
            return -1;
        

        libav::Frame* frame = session->device->frame;
        frame->pict_type = AV_PICTURE_TYPE_I;

        util::QueueArray* packets = QUEUE_ARRAY_CLASS->init();
        while(!QUEUE_ARRAY_CLASS->peek(packets)) {
            if(encode(1, session, frame, packets, NULL)) {
                return -1;
            }
        }

        OBJECT_HOLDER(obj);
        QUEUE_ARRAY_CLASS->pop(packets,obj);
        Packet* packet    = (Packet*)OBJECT_CLASS->ref(obj);
        libav::Packet* av_packet = packet->packet;
        OBJECT_CLASS->unref(obj);

        if(!(av_packet->flags & AV_PKT_FLAG_KEY)) {
            // BOOST_LOG(error) << "First packet type is not an IDR frame"sv;
            return -1;
        }

        int flag = 0;
        if(bitstream::validate_sps(av_packet, config.videoFormat ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264)) {
            flag |= ValidateFlags::VUI_PARAMS;
        }

        char* hevc_nalu = "\000\000\000\001(";
        char* h264_nalu = "\000\000\000\001e";
        char* nalu_prefix = config.videoFormat ? hevc_nalu : h264_nalu;
        std::string_view payload { (char *)av_packet->data, (std::size_t)av_packet->size };

        // TODO search
        // if(std::search(std::begin(payload), std::end(payload), std::begin(nalu_prefix), std::end(nalu_prefix)) != std::end(payload)) 
        //     flag |= ValidateFlags::NALU_PREFIX_5b;

        return flag;
    }

    bool 
    validate_encoder(Encoder* encoder) 
    {
        platf::Display* disp;

        // BOOST_LOG(info) << "Trying encoder ["sv << encoder->name << ']';
        // auto fg = util::fail_guard([&]() {
        //     BOOST_LOG(info) << "Encoder ["sv << encoder->name << "] failed"sv;
        // });

        bool force_hevc = ENCODER_CONFIG->hevc_mode >= 2;
        bool test_hevc  = force_hevc || (ENCODER_CONFIG->hevc_mode == 0 && !(encoder->flags & H264_ONLY));

        encoder->h264.capabilities.set();
        encoder->hevc.capabilities.set();

        encoder->hevc.capabilities[FrameFlags::PASSED] = test_hevc;

        // First, test encoder viability
        Config config_max_ref_frames { 1920, 1080, 60, 1000, 1, 1, 1, 0, 0 };
        Config config_autoselect { 1920, 1080, 60, 1000, 1, 0, 1, 0, 0 };

        retry:
        auto max_ref_frames_h264 = validate_config(disp, encoder, config_max_ref_frames);
        auto autoselect_h264     = validate_config(disp, encoder, config_autoselect);

        if(max_ref_frames_h264 < 0 && autoselect_h264 < 0) {
            if(encoder->h264.has_qp && encoder->h264.capabilities[FrameFlags::CBR]) 
            {
                // It's possible the encoder isn't accepting Constant Bit Rate. Turn off CBR and make another attempt
                encoder->h264.capabilities.set();
                encoder->h264.capabilities[FrameFlags::CBR] = FALSE;
                goto retry;
            }
            return false;
        }

        std::vector<std::pair<validate_flag_e, FrameFlags::flag_e>> packet_deficiencies {
            { ValidateFlags::VUI_PARAMS, FrameFlags::VUI_PARAMETERS },
            { ValidateFlags::NALU_PREFIX_5b, FrameFlags::NALU_PREFIX_5b },
        };

        for(auto [validate_flag, encoder_flag] : packet_deficiencies) {
            encoder->h264.capabilities[encoder_flag] = (max_ref_frames_h264 & validate_flag && autoselect_h264 & validate_flag);
        }

        encoder->h264.capabilities[FrameFlags::REF_FRAMES_RESTRICT]   = max_ref_frames_h264 >= 0;
        encoder->h264.capabilities[FrameFlags::REF_FRAMES_AUTOSELECT] = autoselect_h264 >= 0;
        encoder->h264.capabilities[FrameFlags::PASSED]                = true;

        encoder->h264.capabilities[FrameFlags::SLICE] = validate_config(disp, encoder, config_max_ref_frames);
        if(test_hevc) {
            config_max_ref_frames.videoFormat = 1;
            config_autoselect.videoFormat     = 1;

        retry_hevc:
            auto max_ref_frames_hevc = validate_config(disp, encoder, config_max_ref_frames);
            auto autoselect_hevc     = validate_config(disp, encoder, config_autoselect);

            // If HEVC must be supported, but it is not supported
            if(max_ref_frames_hevc < 0 && autoselect_hevc < 0) {
            if(encoder->hevc.has_qp && encoder->hevc.capabilities[FrameFlags::CBR]) {
                // It's possible the encoder isn't accepting Constant Bit Rate. Turn off CBR and make another attempt
                encoder->hevc.capabilities.set();
                encoder->hevc.capabilities[FrameFlags::CBR] = false;
                goto retry_hevc;
            }

            if(force_hevc) {
                return false;
            }
            }

            for(auto [validate_flag, encoder_flag] : packet_deficiencies) {
            encoder->hevc.capabilities[encoder_flag] = (max_ref_frames_hevc & validate_flag && autoselect_hevc & validate_flag);
            }

            encoder->hevc.capabilities[FrameFlags::REF_FRAMES_RESTRICT]   = max_ref_frames_hevc >= 0;
            encoder->hevc.capabilities[FrameFlags::REF_FRAMES_AUTOSELECT] = autoselect_hevc >= 0;

            encoder->hevc.capabilities[FrameFlags::PASSED] = max_ref_frames_hevc >= 0 || autoselect_hevc >= 0;
        }

        std::vector<std::pair<FrameFlags::flag_e, config_t>> configs {
            { FrameFlags::DYNAMIC_RANGE, { 1920, 1080, 60, 1000, 1, 0, 3, 1, 1 } },
        };

        if(!(encoder->flags & SINGLE_SLICE_ONLY)) {
            configs.emplace_back(
            std::pair<FrameFlags, config_t> { FrameFlags::SLICE, { 1920, 1080, 60, 1000, 2, 1, 1, 0, 0 } });
        }

        for(auto &[flag, config] : configs) {
            auto h264 = config;
            auto hevc = config;

            h264.videoFormat = 0;
            hevc.videoFormat = 1;

            encoder->h264.capabilities[flag] = validate_config(disp, encoder, h264) >= 0;
            if(encoder->hevc.capabilities[FrameFlags::PASSED]) {
                encoder->hevc.capabilities[flag] = validate_config(disp, encoder, hevc) >= 0;
            }
        }

        if(encoder->flags & SINGLE_SLICE_ONLY) {
            encoder->h264.capabilities[FrameFlags::SLICE] = false;
            encoder->hevc.capabilities[FrameFlags::SLICE] = false;
        }

        encoder->h264.capabilities[FrameFlags::VUI_PARAMETERS] = encoder->h264.capabilities[FrameFlags::VUI_PARAMETERS] && !config::sunshine.flags[config::flag::FORCE_VIDEO_HEADER_REPLACE];
        encoder->hevc.capabilities[FrameFlags::VUI_PARAMETERS] = encoder->hevc.capabilities[FrameFlags::VUI_PARAMETERS] && !config::sunshine.flags[config::flag::FORCE_VIDEO_HEADER_REPLACE];

        if(!encoder->h264.capabilities[FrameFlags::VUI_PARAMETERS]) {
            // BOOST_LOG(warning) << encoder->name << ": h264 missing sps->vui parameters"sv;
        }
        if(encoder->hevc.capabilities[FrameFlags::PASSED] && !encoder->hevc.capabilities[FrameFlags::VUI_PARAMETERS]) {
            // BOOST_LOG(warning) << encoder->name << ": hevc missing sps->vui parameters"sv;
        }

        if(!encoder->h264.capabilities[FrameFlags::NALU_PREFIX_5b]) {
            // BOOST_LOG(warning) << encoder->name << ": h264: replacing nalu prefix data"sv;
        }
        if(encoder->hevc.capabilities[FrameFlags::PASSED] && !encoder->hevc.capabilities[FrameFlags::NALU_PREFIX_5b]) {
            // BOOST_LOG(warning) << encoder->name << ": hevc: replacing nalu prefix data"sv;
        }

        fg.disable();
        return true;
    }

    bool
    init_encoder_base(encoder::Encoder* encoder)
    {
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////////"sv;
        // BOOST_LOG(info) << "//                                                              //"sv;
        // BOOST_LOG(info) << "//   Testing for available encoders, this may generate errors.  //"sv;
        // BOOST_LOG(info) << "//   You can safely ignore those errors.                        //"sv;
        // BOOST_LOG(info) << "//                                                              //"sv;
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////////"sv;

        if( (!ENCODER_CONFIG->encoder && encoder->name != ENCODER_CONFIG->encoder) ||  // if config do not specify encoder and encoder name not equal one in config file
            (config::video.hevc_mode == 3 && !pos->hevc[FrameFlags::DYNAMIC_RANGE]) ||
            !validate_encoder(encoder)) 
            return FALSE;


        // BOOST_LOG(info);
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////"sv;
        // BOOST_LOG(info) << "//                                                          //"sv;
        // BOOST_LOG(info) << "// Ignore any errors mentioned above, they are not relevant //"sv;
        // BOOST_LOG(info) << "//                                                          //"sv;
        // BOOST_LOG(info) << "//////////////////////////////////////////////////////////////"sv;
        // BOOST_LOG(info);

        // BOOST_LOG(debug) << "------  h264 ------"sv;
        for(int x = 0; x < FrameFlags::MAX_FLAGS; ++x) {
            auto flag = (FrameFlags)x;
            // BOOST_LOG(debug) << FrameFlags::from_flag(flag) << (encoder->h264.capabilities[flag] ? ": supported"sv : ": unsupported"sv);
        }
        // BOOST_LOG(debug) << "-------------------"sv;

        if(encoder->hevc.capabilities[FrameFlags::PASSED]) {
            // BOOST_LOG(debug) << "------  hevc ------"sv;
            for(int x = 0; x < FrameFlags::MAX_FLAGS; ++x) {
            auto flag = (FrameFlags)x;
                // BOOST_LOG(debug) << FrameFlags::from_flag(flag) << (encoder->hevc.capabilities[flag] ? ": supported"sv : ": unsupported"sv);
            }
            // BOOST_LOG(debug) << "-------------------"sv;

            // BOOST_LOG(info) << "Found encoder "sv << encoder->name << ": ["sv << encoder->h264.name << ", "sv << encoder->hevc.name << ']';
        }
        else {
            // BOOST_LOG(info) << "Found encoder "sv << encoder->name << ": ["sv << encoder->h264.name << ']';
        }

        if(config::video.hevc_mode == 0) {
            config::video.hevc_mode = encoder->hevc.capabilities[FrameFlags::PASSED] ? (encoder->hevc.capabilities[FrameFlags::DYNAMIC_RANGE] ? 3 : 2) : 1;
        }

        return 0;
    }
}