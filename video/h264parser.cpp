#include "bitstream.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct
{
	int profile_idc;
	int constraint_set_flat;
	int level_idc;
	int seq_parameter_set_id;
	int chroma_format_idc;
	struct{ // profile_idc=[100, 110, 122, 244, 44, 83, 86, 118, 128]
		int separate_colour_plane_flag;
		int bit_depth_luma_minus8;
		int bit_depth_chroma_minus8;
		int qpprime_y_zero_transform_bypass_flag;
		int seq_scaling_matrix_present_flag;
		int ScalingList4x4[6][16];
		int ScalingList8x8[6][64];
		int UseDefaultScalingMatrix4x4Flag[6];
		int UseDefaultScalingMatrix8x8Flag[6];
		int pic_scaling_list_present_flag[12];
	} chroma;
	int log2_max_frame_num_minus4;
	int pic_order_cnt_type;
	int log2_max_pic_order_cnt_lsb_minus4;
	int delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	int num_ref_frames_in_pic_order_cnt_cycle;
	int *offset_for_ref_frame;
	int max_num_ref_frames;
	int gaps_in_frame_num_value_allowed_flag;
	int pic_width_in_mbs_minus1;
	int pic_height_in_map_units_minus1;
	int frame_mbs_only_flag;
	int mb_adaptive_frame_field_flag;
	int direct_8x8_inference_flag;
	int frame_cropping_flag;
	struct{
		int frame_crop_left_offset;
		int frame_crop_right_offset;
		int frame_crop_top_offset;
		int frame_crop_bottom_offset;
	} frame_cropping;
	int vui_parameters_present_flag;
} h264_sps_t;

static void print_h264_sps(h264_sps_t* sps)
{
	printf("seq_parameter_set");
	printf(" profile_idc: %d\n", sps->profile_idc);
	printf(" constraint_set_flat: %02x\n", sps->constraint_set_flat);
	printf(" level_idc: %d\n", sps->level_idc);
	printf(" seq_parameter_set_id: %d\n", sps->seq_parameter_set_id);
	printf(" chroma_format_idc: %d\n", sps->chroma_format_idc);
	if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
		sps->profile_idc == 122 || sps->profile_idc == 244 || sps->profile_idc == 44  ||
		sps->profile_idc == 83  || sps->profile_idc == 86  || sps->profile_idc == 118 ||
		sps->profile_idc == 128 ) 
	{
		if(3 == sps->chroma_format_idc)
			printf("   separate_colour_plane_flag: %d\n", sps->chroma.separate_colour_plane_flag);
		printf("   bit_depth_luma_minus8: %d\n", sps->chroma.bit_depth_luma_minus8);
		printf("   bit_depth_chroma_minus8: %d\n", sps->chroma.bit_depth_chroma_minus8);
		printf("   qpprime_y_zero_transform_bypass_flag: %d\n", sps->chroma.qpprime_y_zero_transform_bypass_flag);
		printf("   seq_scaling_matrix_present_flag: %d\n", sps->chroma.seq_scaling_matrix_present_flag);
	}
	printf(" log2_max_frame_num_minus4: %d\n", sps->log2_max_frame_num_minus4);
	printf(" pic_order_cnt_type: %d\n", sps->pic_order_cnt_type);
	if(0 == sps->pic_order_cnt_type)
	{
		printf(" log2_max_pic_order_cnt_lsb_minus4: %d\n", sps->log2_max_pic_order_cnt_lsb_minus4);
	}
	else if(1 == sps->pic_order_cnt_type)
	{
		printf(" delta_pic_order_always_zero_flag: %d\n", sps->delta_pic_order_always_zero_flag);
		printf(" offset_for_non_ref_pic: %d\n", sps->offset_for_non_ref_pic);
		printf(" offset_for_top_to_bottom_field: %d\n", sps->offset_for_top_to_bottom_field);
		printf(" num_ref_frames_in_pic_order_cnt_cycle: %d\n", sps->num_ref_frames_in_pic_order_cnt_cycle);
	}
	printf(" max_num_ref_frames: %d\n", sps->max_num_ref_frames);
	printf(" gaps_in_frame_num_value_allowed_flag: %d\n", sps->gaps_in_frame_num_value_allowed_flag);
	printf(" pic_width_in_mbs_minus1: %d\n", sps->pic_width_in_mbs_minus1);
	printf(" pic_height_in_map_units_minus1: %d\n", sps->pic_height_in_map_units_minus1);
	printf(" frame_mbs_only_flag: %d\n", sps->frame_mbs_only_flag);
	if(!sps->frame_mbs_only_flag)
		printf(" mb_adaptive_frame_field_flag: %d\n", sps->mb_adaptive_frame_field_flag);
	printf(" direct_8x8_inference_flag: %d\n", sps->direct_8x8_inference_flag);
	printf(" frame_cropping_flag: %d\n", sps->frame_cropping_flag);
	if(sps->frame_cropping_flag)
	{
		printf("   frame_crop_left_offset: %d\n", sps->frame_cropping.frame_crop_left_offset);
		printf("   frame_crop_right_offset: %d\n", sps->frame_cropping.frame_crop_right_offset);
		printf("   frame_crop_top_offset: %d\n", sps->frame_cropping.frame_crop_top_offset);
		printf("   frame_crop_bottom_offset: %d\n", sps->frame_cropping.frame_crop_bottom_offset);
	}
	printf(" vui_parameters_present_flag: %d\n", sps->vui_parameters_present_flag);
}

static void rbsp_trailing_bits(bitstream_t* stream)
{
	int rbsp_stop_one_bit;
	int rbsp_alignment_zero_bit;
	int bytes, bits, i;
	
	rbsp_stop_one_bit = bitstream_read_bit(stream);
	assert(1 == rbsp_stop_one_bit);

	bitstream_get_offset(stream, &bytes, &bits);
	for(i=bits; i<8; i++)
	{
		rbsp_alignment_zero_bit = bitstream_read_bit(stream);
		assert(0 == rbsp_alignment_zero_bit);
	}
}

static bool more_rbsp_data(bitstream_t* stream)
{
	int rbsp_next_bits;
	int bytes, bits, n;

	bitstream_get_offset(stream, &bytes, &bits);
	if(bytes+1 >= stream->bytes && bits+1 >= 8)
		return false; // no more data

	n = bits < 8 ? 8-bits : 8;
	rbsp_next_bits = bitstream_next_bits(stream, n);
	return rbsp_next_bits != (1<<(n-1));
}

static void scaling_list(bitstream_t* stream, int *scalingList, int sizeOfScalingList, int *useDefaultScalingMatrixFlag)
{
	int lastScale = 8;
	int nextScale = 8;
	for(int j=0; j<sizeOfScalingList; j++)
	{
		if(nextScale != 0)
		{
			int delta_scale = bitstream_read_se(stream);
			nextScale = (lastScale + delta_scale + 256) % 256;
			*useDefaultScalingMatrixFlag = (j==0 && nextScale==0) ? 1 : 0;
		}
		scalingList[j] = 0==nextScale ? lastScale : nextScale;
		lastScale = scalingList[j];
	}
}

static void hrd_parameters(bitstream_t* stream)
{
	int bit_rate_value_minus1[32];
	int cpb_size_value_minus1[32];
	int cbr_flag[32];

	int cpb_cnt_minus1 = bitstream_read_ue(stream);
	int bit_rate_scale = bitstream_read_bits(stream, 4);
	int cpb_size_scale = bitstream_read_bits(stream, 4);
	for(int SchedSelIdx=0; SchedSelIdx<=cpb_cnt_minus1; ++SchedSelIdx)
	{
		bit_rate_value_minus1[ SchedSelIdx ] = bitstream_read_ue(stream);
		cpb_size_value_minus1[ SchedSelIdx ] = bitstream_read_ue(stream);
		cbr_flag[ SchedSelIdx ] = bitstream_read_bit(stream);
	}

	int initial_cpb_removal_delay_length_minus1 = bitstream_read_bits(stream, 5);
	int cpb_removal_delay_length_minus1 = bitstream_read_bits(stream, 5);
	int dpb_output_delay_length_minus1 = bitstream_read_bits(stream, 5);
	int time_offset_length = bitstream_read_bits(stream, 5);
}

#define Extended_SAR 255
static void vui_parameters(bitstream_t* stream)
{
	int aspect_ratio_info_present_flag = bitstream_read_bit(stream);
	if(aspect_ratio_info_present_flag)
	{
		int aspect_ratio_idc = bitstream_read_bits(stream, 8);
		if(aspect_ratio_idc == Extended_SAR)
		{
			int sar_width = bitstream_read_bits(stream, 16);
			int sar_height = bitstream_read_bits(stream, 16);
		}
	}

	int overscan_info_present_flag = bitstream_read_bit(stream);
	if(overscan_info_present_flag)
	{
		int overscan_appropriate_flag = bitstream_read_bit(stream);
	}

	int video_signal_type_present_flag = bitstream_read_bit(stream);
	if(video_signal_type_present_flag)
	{
		int video_format = bitstream_read_bits(stream, 3);
		int video_full_range_flag = bitstream_read_bit(stream);
		int colour_description_present_flag = bitstream_read_bit(stream);
		if(colour_description_present_flag)
		{
			int colour_primaries = bitstream_read_bits(stream, 8);
			int transfer_characteristics = bitstream_read_bits(stream, 8);
			int matrix_coefficients = bitstream_read_bits(stream, 8);
		}
	}

	int chroma_loc_info_present_flag = bitstream_read_bit(stream);
	if(chroma_loc_info_present_flag)
	{
		int chroma_sample_loc_type_top_field = bitstream_read_ue(stream);
		int chroma_sample_loc_type_bottom_field = bitstream_read_ue(stream);
	}

	int timing_info_present_flag = bitstream_read_bit(stream);
	if(timing_info_present_flag)
	{
		int num_units_in_tick = bitstream_read_bits(stream, 32);
		int time_scale = bitstream_read_bits(stream, 32);
		int fixed_frame_rate_flag = bitstream_read_bit(stream);
	}

	int nal_hrd_parameters_present_flag = bitstream_read_bit(stream);
	if(nal_hrd_parameters_present_flag)
	{
		hrd_parameters(stream);
	}

	int vcl_hrd_parameters_present_flag = bitstream_read_bit(stream);
	if(vcl_hrd_parameters_present_flag)
	{
		hrd_parameters(stream);
	}

	if(nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
	{
		int low_delay_hrd_flag = bitstream_read_bit(stream);
	}

	int pic_struct_present_flag = bitstream_read_bit(stream);
	int bitstream_restriction_flag = bitstream_read_bit(stream);
	if(bitstream_restriction_flag)
	{
		int motion_vectors_over_pic_boundaries_flag = bitstream_read_bit(stream);
		int max_bytes_per_pic_denom = bitstream_read_ue(stream);
		int max_bits_per_mb_denom = bitstream_read_ue(stream);
		int log2_max_mv_length_horizontal = bitstream_read_ue(stream);
		int log2_max_mv_length_vertical = bitstream_read_ue(stream);
		int max_num_reorder_frames = bitstream_read_ue(stream);
		int max_dec_frame_buffering = bitstream_read_ue(stream);
	}
}

static void seq_parameter_set_data(bitstream_t* stream, h264_sps_t* sps)
{
	sps->chroma_format_idc = 1;
	sps->profile_idc = bitstream_read_bits(stream, 8);
	sps->constraint_set_flat = bitstream_read_bits(stream, 8);
	sps->level_idc = bitstream_read_bits(stream, 8);
	sps->seq_parameter_set_id = bitstream_read_ue(stream);
	if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
		sps->profile_idc == 122 || sps->profile_idc == 244 || sps->profile_idc == 44  ||
		sps->profile_idc == 83  || sps->profile_idc == 86  || sps->profile_idc == 118 ||
		sps->profile_idc == 128 ) 
	{
		sps->chroma_format_idc = bitstream_read_ue(stream);
		if(3 == sps->chroma_format_idc)
			sps->chroma.separate_colour_plane_flag = bitstream_read_bit(stream);
		sps->chroma.bit_depth_luma_minus8 = bitstream_read_ue(stream);
		sps->chroma.bit_depth_chroma_minus8 = bitstream_read_ue(stream);
		sps->chroma.qpprime_y_zero_transform_bypass_flag = bitstream_read_bit(stream);
		sps->chroma.seq_scaling_matrix_present_flag = bitstream_read_bit(stream);
		if(sps->chroma.seq_scaling_matrix_present_flag)
		{
			for(int i=0; i<((sps->chroma_format_idc!=3)?8:12); i++)
			{
				sps->chroma.pic_scaling_list_present_flag[ i ] = bitstream_read_bit(stream);
				if(sps->chroma.pic_scaling_list_present_flag[ i ])
				{
					if(i < 6)
					{
						scaling_list(stream, sps->chroma.ScalingList4x4[i], 16, &sps->chroma.UseDefaultScalingMatrix4x4Flag[i]);
					}
					else
					{
						scaling_list(stream, sps->chroma.ScalingList8x8[i-6], 64, &sps->chroma.UseDefaultScalingMatrix8x8Flag[i-6]);
					}
				}
			}
		}
	}

	sps->log2_max_frame_num_minus4 = bitstream_read_ue(stream);
	sps->pic_order_cnt_type = bitstream_read_ue(stream);
	if(0 == sps->pic_order_cnt_type)
	{
		sps->log2_max_pic_order_cnt_lsb_minus4 = bitstream_read_ue(stream);
	}
	else if(1 == sps->pic_order_cnt_type)
	{
		sps->delta_pic_order_always_zero_flag = bitstream_read_bit(stream);
		sps->offset_for_non_ref_pic = bitstream_read_se(stream);
		sps->offset_for_top_to_bottom_field = bitstream_read_se(stream);
		sps->num_ref_frames_in_pic_order_cnt_cycle = bitstream_read_ue(stream);
		sps->offset_for_ref_frame = (int*)malloc(sps->num_ref_frames_in_pic_order_cnt_cycle * sizeof(int));
		for(int i=0; i<sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
			sps->offset_for_ref_frame[i] = bitstream_read_se(stream);
	}

	sps->max_num_ref_frames = bitstream_read_ue(stream);
	sps->gaps_in_frame_num_value_allowed_flag = bitstream_read_bit(stream);
	sps->pic_width_in_mbs_minus1 = bitstream_read_ue(stream);
	sps->pic_height_in_map_units_minus1 = bitstream_read_ue(stream);
	sps->frame_mbs_only_flag = bitstream_read_bit(stream);
	if(!sps->frame_mbs_only_flag)
		sps->mb_adaptive_frame_field_flag = bitstream_read_bit(stream);
	sps->direct_8x8_inference_flag = bitstream_read_bit(stream);
	sps->frame_cropping_flag = bitstream_read_bit(stream);
	if(sps->frame_cropping_flag)
	{
		sps->frame_cropping.frame_crop_left_offset	= bitstream_read_ue(stream);
		sps->frame_cropping.frame_crop_right_offset	= bitstream_read_ue(stream);
		sps->frame_cropping.frame_crop_top_offset	= bitstream_read_ue(stream);
		sps->frame_cropping.frame_crop_bottom_offset= bitstream_read_ue(stream);
	}
	sps->vui_parameters_present_flag = bitstream_read_bit(stream);
	if(sps->vui_parameters_present_flag)
	{
		vui_parameters(stream);
	}

	rbsp_trailing_bits(stream);
}

static void pic_parameter_set_rbsp(bitstream_t* stream)
{
	int pic_parameter_set_id = bitstream_read_ue(stream);
	int seq_parameter_set_id = bitstream_read_ue(stream);
	int entropy_coding_mode_flag = bitstream_read_bit(stream);
	int bottom_field_pic_order_in_frame_present_flag = bitstream_read_bit(stream);
	int num_slice_groups_minus1 = bitstream_read_ue(stream);
	if(num_slice_groups_minus1 > 0)
	{
		int slice_group_map_type = bitstream_read_ue(stream);
		if(0 == slice_group_map_type)
		{
			int run_length_minus1[64];
			for(int iGroup=0; iGroup<num_slice_groups_minus1; ++iGroup)
			{
				run_length_minus1[ iGroup ] = bitstream_read_ue(stream);
			}
		}
		else if(2 == slice_group_map_type)
		{
			int top_left[64];
			int bottom_right[64];
			for(int iGroup=0; iGroup<num_slice_groups_minus1; ++iGroup)
			{
				top_left[ iGroup ] = bitstream_read_ue(stream);
				bottom_right[ iGroup ] = bitstream_read_ue(stream);
			}
		}
		else if(3==slice_group_map_type || 4==slice_group_map_type || 5==slice_group_map_type)
		{
			int slice_group_change_direction_flag = bitstream_read_bit(stream);
			int slice_group_change_rate_minus1 = bitstream_read_ue(stream);
		}
		else if(6 == slice_group_map_type)
		{
			int slice_group_id[64];
			int pic_size_in_map_units_minus1 = bitstream_read_ue(stream);
			for(int i=0; i<pic_size_in_map_units_minus1; i++)
			{
				slice_group_id[ i ] = bitstream_read_ue(stream);
			}
		}
	}

	int num_ref_idx_l0_default_active_minus1 = bitstream_read_ue(stream);
	int num_ref_idx_l1_default_active_minus1 = bitstream_read_ue(stream);
	int weighted_pred_flag = bitstream_read_bit(stream);
	int weighted_bipred_idc = bitstream_read_bits(stream, 2);
	int pic_init_qp_minus26 = bitstream_read_se(stream);
	int pic_init_qs_minus26 = bitstream_read_se(stream);
	int chroma_qp_index_offset = bitstream_read_se(stream);
	int deblocking_filter_control_present_flag = bitstream_read_bit(stream);
	int constrained_intra_pred_flag = bitstream_read_bit(stream);
	int redundant_pic_cnt_present_flag = bitstream_read_bit(stream);

	if(more_rbsp_data(stream))
	{
		int transform_8x8_mode_flag = bitstream_read_bit(stream);
		int pic_scaling_matrix_present_flag = bitstream_read_bit(stream);
		if(pic_scaling_matrix_present_flag)
		{
			int ScalingList4x4[16][64];
			int ScalingList8x8[64][64];
			int UseDefaultScalingMatrix4x4Flag[16];
			int UseDefaultScalingMatrix8x8Flag[16];

			int pic_scaling_list_present_flag[64];
			int chroma_format_idc = 1; // 4:2:0
			for(int i=0; i<6 + ( (chroma_format_idc != 3 ) ? 2 : 6 ) * transform_8x8_mode_flag; i++)
			{
				pic_scaling_list_present_flag[ i ] = bitstream_read_bit(stream);
				if(pic_scaling_list_present_flag[ i ])
				{
					if(i < 6)
					{
						scaling_list(stream, ScalingList4x4[i], 16, &UseDefaultScalingMatrix4x4Flag[i]);
					}
					else
					{
						scaling_list(stream, ScalingList8x8[i-6], 64, &UseDefaultScalingMatrix8x8Flag[i-6]);
					}
				}
			}
		}
		int second_chroma_qp_index_offset = bitstream_read_se(stream);
	}

	rbsp_trailing_bits(stream);
}

extern "C" void nal_unit(bitstream_t* stream)
{
	int forbidden_zero_bit = bitstream_read_bit(stream);
	int nal_ref_idc = bitstream_read_bits(stream, 2);
	int nal_unit_type = bitstream_read_bits(stream, 5);

	h264_sps_t sps;
	forbidden_zero_bit,nal_unit_type,nal_ref_idc;
	switch(nal_unit_type)
	{
	case 7:
		seq_parameter_set_data(stream, &sps);
		print_h264_sps(&sps);
		break;

	case 8:
		pic_parameter_set_rbsp(stream);
		break;
	}

	//width = ((pic_width_in_mbs_minus1 +1)*16) - frame_crop_left_offset*2 - frame_crop_right_offset*2;
	//height= ((2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 +1) * 16) - (frame_crop_top_offset * 2) - (frame_crop_bottom_offset * 2)
}

unsigned char* search_start_code(unsigned char* stream, int bytes)
{
	unsigned char *p;
	for(p = stream; p+3<stream+bytes; p++)
	{
		if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
			return p;
	}
	return NULL;
}
