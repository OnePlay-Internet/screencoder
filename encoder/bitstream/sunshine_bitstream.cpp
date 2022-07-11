/**
 * @file sunshine_bitstream.cpp
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <sunshine_bitstream.h>
#include <sunshine_util.h>

extern "C" {
#include <cbs/cbs_h264.h>
#include <cbs/cbs_h265.h>
#include <cbs/video_levels.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

#include <cbs.h>
#include <string.h>



namespace bitstream {
    typedef CodedBitstreamContext       Context;
    typedef CodedBitstreamFragment      Fragment;


    void 
    close(Context *c) {
        ff_cbs_close(&c);
    }


    void
    init_fragment(Fragment* o) 
    {

        // TODO
        // std::copy((std::uint8_t *)&o, (std::uint8_t *)(&o + 1), (std::uint8_t *));

        o->data  = nullptr;
        o->units = nullptr;
    };

    // frag_t() 
    // {
    //     std::fill_n((std::uint8_t *)this, sizeof(*this), 0);
    // }

    // frag_t &operator=(frag_t &&o) {
    //     std::copy((std::uint8_t *)&o, (std::uint8_t *)(&o + 1), (std::uint8_t *)this);

    //     o.data  = nullptr;
    //     o.units = nullptr;

    //     return *this;
    // };


    /**
     * @brief 
     * 
     * @param fragment 
     */
    void
    fragment_free(Fragment* fragment) {
        if(fragment->data || fragment->units) {
            ff_cbs_fragment_free(fragment);
        }
    }

    util::Object*
    write_with_context(Context *cbs_ctx, 
          uint8 nal, 
          void *uh, 
          libav::CodecID codec_id) 
    {
        Fragment frag;
        int err = ff_cbs_insert_unit_content(&frag, -1, nal, uh, nullptr);
        if(err < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
           LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, err));
            return {};
        }

        err = ff_cbs_write_fragment_data(cbs_ctx, &frag);
        if(err < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, err));
            return {};
        }

        OBJECT_DUPLICATE(obj,frag.data_size,frag.data,temp);
        return obj;
    }

    util::Object*
    write(uint8 nal, 
          void *uh, 
          libav::CodecID codec_id) 
    {
        Context* cbs_ctx;
        ff_cbs_init(&cbs_ctx, codec_id, NULL);
        return write_with_context(cbs_ctx, nal, uh, codec_id);
    }

    util::Object*
    make_sps_h264(const AVCodecContext *ctx) 
    {
        H264RawSPS sps {};

        /* b_per_p == ctx->max_b_frames for h264 */
        /* desired_b_depth == avoption("b_depth") == 1 */
        /* max_b_depth == std::min(av_log2(ctx->b_per_p) + 1, desired_b_depth) ==> 1 */
        auto max_b_depth = 1;
        auto dpb_frame   = ctx->gop_size == 1 ? 0 : 1 + max_b_depth;
        auto mb_width    = (FFALIGN(ctx->width, 16) / 16) * 16;
        auto mb_height   = (FFALIGN(ctx->height, 16) / 16) * 16;


        sps.nal_unit_header.nal_ref_idc   = 3;
        sps.nal_unit_header.nal_unit_type = H264_NAL_SPS;

        sps.profile_idc = FF_PROFILE_H264_HIGH & 0xFF;

        sps.constraint_set1_flag = 1;

        if(ctx->level != FF_LEVEL_UNKNOWN) 
        {
            sps.level_idc = ctx->level;
        }
        else 
        {
            auto framerate = ctx->framerate;

            auto level = ff_h264_guess_level(
            sps.profile_idc,
            ctx->bit_rate,
            framerate.num / framerate.den,
            mb_width,
            mb_height,
            dpb_frame);

            if(!level) 
            {
                LOG_ERROR("Could not guess h264 level");
                return {};
            }
            sps.level_idc = level->level_idc;
        }

        sps.seq_parameter_set_id = 0;
        sps.chroma_format_idc    = 1;

        sps.log2_max_frame_num_minus4         = 3; //4;
        sps.pic_order_cnt_type                = 0;
        sps.log2_max_pic_order_cnt_lsb_minus4 = 0; //4;

        sps.max_num_ref_frames = dpb_frame;

        sps.pic_width_in_mbs_minus1        = mb_width / 16 - 1;
        sps.pic_height_in_map_units_minus1 = mb_height / 16 - 1;

        sps.frame_mbs_only_flag       = 1;
        sps.direct_8x8_inference_flag = 1;

        if(ctx->width != mb_width || ctx->height != mb_height) {
            sps.frame_cropping_flag      = 1;
            sps.frame_crop_left_offset   = 0;
            sps.frame_crop_top_offset    = 0;
            sps.frame_crop_right_offset  = (mb_width - ctx->width) / 2;
            sps.frame_crop_bottom_offset = (mb_height - ctx->height) / 2;
        }

        sps.vui_parameters_present_flag = 1;

        auto &vui = sps.vui;

        vui.video_format                    = 5;
        vui.colour_description_present_flag = 1;
        vui.video_signal_type_present_flag  = 1;
        vui.video_full_range_flag           = ctx->color_range == AVCOL_RANGE_JPEG;
        vui.colour_primaries                = ctx->color_primaries;
        vui.transfer_characteristics        = ctx->color_trc;
        vui.matrix_coefficients             = ctx->colorspace;

        vui.low_delay_hrd_flag = 1 - vui.fixed_frame_rate_flag;

        vui.bitstream_restriction_flag              = 1;
        vui.motion_vectors_over_pic_boundaries_flag = 1;
        vui.log2_max_mv_length_horizontal           = 15;
        vui.log2_max_mv_length_vertical             = 15;
        vui.max_num_reorder_frames                  = max_b_depth;
        vui.max_dec_frame_buffering                 = max_b_depth + 1;

        return write(sps.nal_unit_header.nal_unit_type, (void *)&sps.nal_unit_header, AV_CODEC_ID_H264);
    }

    HEVC
    make_sps_hevc(const libav::CodecContext *avctx, 
                  const libav::Packet*packet) 
    {
        Context* ctx;
        if(ff_cbs_init(&ctx, AV_CODEC_ID_H265, nullptr)) {
            return {};
        }

        Fragment frag;

        int err = ff_cbs_read_packet(ctx, &frag, packet);
        if(err < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, err));
            return {};
        }


        auto vps_p = ((CodedBitstreamH265Context *)ctx->priv_data)->active_vps;
        auto sps_p = ((CodedBitstreamH265Context *)ctx->priv_data)->active_sps;

        H265RawSPS sps { *sps_p };
        H265RawVPS vps { *vps_p };

        vps.profile_tier_level.general_profile_compatibility_flag[4] = 1;
        sps.profile_tier_level.general_profile_compatibility_flag[4] = 1;

        memset((void*)&sps.vui, 0, sizeof(H265RawVUI));

        sps.vui_parameters_present_flag = 1;

        // skip sample aspect ratio

        sps.vui.video_format                    = 5;
        sps.vui.colour_description_present_flag = 1;
        sps.vui.video_signal_type_present_flag  = 1;
        sps.vui.video_full_range_flag           = avctx->color_range == AVCOL_RANGE_JPEG;
        sps.vui.colour_primaries                = avctx->color_primaries;
        sps.vui.transfer_characteristics        = avctx->color_trc;
        sps.vui.matrix_coefficients             = avctx->colorspace;


        sps.vui.vui_timing_info_present_flag        = vps.vps_timing_info_present_flag;
        sps.vui.vui_num_units_in_tick               = vps.vps_num_units_in_tick;
        sps.vui.vui_time_scale                      = vps.vps_time_scale;
        sps.vui.vui_poc_proportional_to_timing_flag = vps.vps_poc_proportional_to_timing_flag;
        sps.vui.vui_num_ticks_poc_diff_one_minus1   = vps.vps_num_ticks_poc_diff_one_minus1;
        sps.vui.vui_hrd_parameters_present_flag     = 0;

        sps.vui.bitstream_restriction_flag              = 1;
        sps.vui.motion_vectors_over_pic_boundaries_flag = 1;
        sps.vui.restricted_ref_pic_lists_flag           = 1;
        sps.vui.max_bytes_per_pic_denom                 = 0;
        sps.vui.max_bits_per_min_cu_denom               = 0;
        sps.vui.log2_max_mv_length_horizontal           = 15;
        sps.vui.log2_max_mv_length_vertical             = 15;

        Context* write_ctx;
        ff_cbs_init(&write_ctx, AV_CODEC_ID_H265, NULL);


        return HEVC{
            NAL
            {
                write_with_context(write_ctx, vps.nal_unit_header.nal_unit_type, (void *)&vps.nal_unit_header, AV_CODEC_ID_H265),
                write_with_context(ctx, vps_p->nal_unit_header.nal_unit_type, (void *)&vps_p->nal_unit_header, AV_CODEC_ID_H265),
            },
            NAL 
            {
                write_with_context(write_ctx, sps.nal_unit_header.nal_unit_type, (void *)&sps.nal_unit_header, AV_CODEC_ID_H265),
                write_with_context(ctx, sps_p->nal_unit_header.nal_unit_type, (void *)&sps_p->nal_unit_header, AV_CODEC_ID_H265),
            },
        };
    }

    util::Object*
    read_sps_h264(const AVPacket *packet) 
    {
        Context* ctx;
        if(ff_cbs_init(&ctx, AV_CODEC_ID_H264, nullptr)) {
            return {};
        }

        Fragment frag;
        int err = ff_cbs_read_packet(ctx, &frag, &*packet);

        if(err < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, err));
            return {};
        }

        H264RawNALUnitHeader* h264 = (H264RawNALUnitHeader *)((CodedBitstreamH264Context *)ctx->priv_data)->active_sps;

        return write(h264->nal_unit_type, (void *)h264, AV_CODEC_ID_H264);
    }

    H264 
    make_sps_h264(const libav::CodecContext*ctx, 
                  const libav::Packet *packet) 
    {
        H264 ret = {0};
        ret.sps._new  =    make_sps_h264(ctx);
        ret.sps.old =    read_sps_h264(packet);
        return ret;
    }

    bool 
    validate_sps(libav::Packet* packet, 
                 int codec_id) 
    {
        Context* ctx;
        if(ff_cbs_init(&ctx, (AVCodecID)codec_id, nullptr)) {
            return false;
        }

        Fragment frag;
        int err = ff_cbs_read_packet(ctx, &frag, packet);
        if(err < 0) {
            char err_str[AV_ERROR_MAX_STRING_SIZE] { 0 };
            LOG_ERROR(av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, err));
            return false;
        }

        if(codec_id == AV_CODEC_ID_H264) {
            CodedBitstreamH264Context* h264 = (CodedBitstreamH264Context *)ctx->priv_data;
            if(!h264->active_sps->vui_parameters_present_flag) {
                return false;
            }
            return true;
        }

        return ((CodedBitstreamH265Context *)ctx->priv_data)->active_sps->vui_parameters_present_flag;
    }
} // namespace cbs