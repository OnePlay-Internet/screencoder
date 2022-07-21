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

#include <platform_common.h>
#include <sunshine_config.h>

#include <encoder_thread.h>




namespace encoder
{


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
