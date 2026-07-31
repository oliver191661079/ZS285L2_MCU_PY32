/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/util.h>
#include <stdint.h>
#include "dsp_inner.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_DSP_FUNCTION

static struct dsp_session_buf *buf_dsp2cpu(uint32_t dsp_buf)
{
	return (struct dsp_session_buf *)dsp_data_to_mcu_address(dsp_buf);
}

static void print_ringbuf_info(const char *str, uint32_t buf, int type)
{
	struct dsp_session_buf *dsp_buf = buf_dsp2cpu(buf);

	if (type == 0) {
		printk("\t\t%s=%p (length=%u/%u ptr=%p/%p)\n",
			str, dsp_buf,
			dsp_session_buf_length(dsp_buf),
			dsp_session_buf_size(dsp_buf),
			acts_ringbuf_head_ptr(&dsp_buf->buf, NULL),
			acts_ringbuf_tail_ptr(&dsp_buf->buf, NULL));
	} else {
		printk("\t\t%s=%p (space=%u/%u ptr=%p/%p)\n",
			str, dsp_buf,
			dsp_session_buf_space(dsp_buf),
			dsp_session_buf_size(dsp_buf),
			acts_ringbuf_head_ptr(&dsp_buf->buf, NULL),
			acts_ringbuf_tail_ptr(&dsp_buf->buf, NULL));
	}
}

static void dump_media_session(void *info)
{
#if 0
	struct media_dspssn_info *media_info = info;
	struct media_dspssn_params *params = &media_info->params;
	printk("\nsession media (id=%u):\n", DSP_SESSION_MEDIA);
	printk("\taec_en=%d\n", params->aec_en);
	printk("\taec_delay=%d samples\n", params->aec_delay);
	if (media_info->aec_refbuf)
		printk("\taec_refbuf=%p (length=%u/%u)\n",
		       buf_dsp2cpu(media_info->aec_refbuf),
		       dsp_session_buf_length(buf_dsp2cpu(media_info->aec_refbuf)),
		       dsp_session_buf_size(buf_dsp2cpu(media_info->aec_refbuf)));
#endif
}

void dsp_session_dump_info(struct dsp_session *session, void *info)
{
	switch (session->id) {
	case DSP_SESSION_MEDIA:
		dump_media_session(info);
		break;
	default:
		break;
	}
}
#if 0
static void dump_test_function(void *info, bool dump_debug)
{
	struct test_dspfunc_info *test_info = info;
	struct test_dspfunc_params *params = &test_info->params;
	struct test_dspfunc_runtime *runtime = &test_info->runtime;

	printk("\nfunction (id=%u):\n", DSP_FUNCTION_TEST);
	printk("\tparameters:\n");
	printk("\t\t(%p)inbuf.length=%u/%u\n", buf_dsp2cpu(params->inbuf),
	       dsp_session_buf_length(buf_dsp2cpu(params->inbuf)),
	       dsp_session_buf_size(buf_dsp2cpu(params->inbuf)));
	printk("\t\t(%p)outbuf.space=%u/%u\n", buf_dsp2cpu(params->outbuf),
	       dsp_session_buf_space(buf_dsp2cpu(params->outbuf)),
	       dsp_session_buf_size(buf_dsp2cpu(params->outbuf)));

	printk("\truntime:\n");
	printk("\t\tsample_count=%u\n", runtime->sample_count);
}
#endif
static void dump_decoder_function(void *info, bool dump_debug)
{
	struct decoder_dspfunc_info *decoder_info = info;
	struct decoder_dspfunc_params *params = &decoder_info->params;
//      struct decoder_dspfunc_ext_params *ext_params = &decoder_info->ext_params;
	struct decoder_dspfunc_runtime *runtime = &decoder_info->runtime;
	struct decoder_dspfunc_debug *debug = &decoder_info->debug;

	if(!info){
		return;
	}

	printk("\nfunction decoder (id=%u):\n", DSP_FUNCTION_DECODER);
	printk("\tparameters:\n");
	printk("\t\tformat=%u\n", params->format);
	printk("\t\tsample_rate(in)=%u\n", params->resample_param.input_sr);
	printk("\t\tsample_rate(out)=%u\n", params->resample_param.output_sr);
	print_ringbuf_info("inbuf", params->inbuf, 0);
	print_ringbuf_info("outbuf", params->outbuf, 1);
//      printk("\t\tformat_pbuf=%x\n", ext_params->format_pbuf);
//      printk("\t\tformat_pbufsz=%u\n", ext_params->format_pbufsz);
//      if (ext_params->evtbuf)
//      	print_ringbuf_info("evtbuf", ext_params->evtbuf, 1);

	printk("\truntime:\n");
	printk("\t\tframe_size=%u\n", runtime->frame_size);
	printk("\t\tchannels=%u\n", runtime->channels);
	printk("\t\tsample_count=%u\n", runtime->sample_count);
	printk("\t\tdatalost_count=%u\n", runtime->datalost_count);
	printk("\t\traw_count=%u\n", runtime->raw_count);

	if (!dump_debug)
		return;

	printk("\tdebug:\n");
	if (debug->stream) {
		print_ringbuf_info("stream", debug->stream, 1);
	}
	if (debug->pcmbuf) {
		print_ringbuf_info("pcmbuf", debug->stream, 1);
	}
	if (debug->plcbuf) {
		print_ringbuf_info("plc_data", debug->plcbuf, 1);
	}
}

static void dump_encoder_function(void *info, bool dump_debug)
{
	struct encoder_dspfunc_info *encoder_info = info;
	struct encoder_dspfunc_params *params = &encoder_info->params;
	struct encoder_dspfunc_runtime *runtime = &encoder_info->runtime;
	struct encoder_dspfunc_debug *debug = &encoder_info->debug;

	printk("\nfunction encoder (id=%u):\n", DSP_FUNCTION_ENCODER);
	printk("\tparameters:\n");
	printk("\t\tformat=%u\n", params->format);
	printk("\t\tsample_rate=%u\n", params->sample_rate);
	printk("\t\tbit_rate=%u\n", params->bit_rate);
	printk("\t\tcomplexityt=%u\n", params->complexity);
	printk("\t\tchannels=%u\n", params->channels);
	printk("\t\tframe_size=%u\n", params->frame_size);
    print_ringbuf_info("inbuf", params->inbuf, 0);
	print_ringbuf_info("outbuf", params->outbuf, 1);

	printk("\truntime:\n");
	printk("\t\tframe_size=%u\n", runtime->frame_size);
	printk("\t\tchannels=%u\n", runtime->channels);
	printk("\t\tcompression_ratio=%u\n", runtime->compression_ratio);
	printk("\t\tsample_count=%u\n", runtime->sample_count);

	if (!dump_debug)
		return;

	printk("\tdebug:\n");
	if (debug->pcmbuf) {
        print_ringbuf_info("pcmbuf", debug->pcmbuf, 1);
	}
	if (debug->stream) {
	    print_ringbuf_info("stream", debug->stream, 1);
	}
}

static void dump_streamout_function(void *info, bool dump_debug)
{
	struct streamout_dspfunc_info *streamout_info = info;
	struct streamout_dspfunc_params *params = &streamout_info->params;

	if(!info){
		return;
	}

	printk("\nfunction streamout (id=%u):\n", DSP_FUNCTION_STREAMOUT);
	printk("\tparameters:\n");
	printk("\t\tdae.pbuf=%08x\n", dsp_data_to_mcu_address(params->dae.pbuf));
	printk("\t\tdae.pbufsz=%u\n", params->dae.pbufsz);

	printk("\t\tmisc.auto_mute=%u\n", params->misc.auto_mute);
	printk("\t\tmisc.auto_mute_threshold=%u\n", params->misc.auto_mute_threshold);

	if (params->output.outbuf)
		print_ringbuf_info("output.outbuf", params->output.outbuf, 1);
	if (params->output.outbuf_subwoofer)
		print_ringbuf_info("output.outbuf_subwoofer", params->output.outbuf_subwoofer, 1);

	printk("\t\tmix.channel_num=%u\n", params->mix.channel_num);
	if (params->mix.channel_num > 0 && params->mix.channel_params[0].chan_buf)
		print_ringbuf_info("mix.channel_params[0].chan_buf", params->mix.channel_params[0].chan_buf, 0);

	if (params->energy.energy_buf)
		print_ringbuf_info("energy.energy_buf", params->energy.energy_buf, 1);
}


static void dump_player_function(void *info, bool dump_debug)
{
	struct player_dspfunc_info *player_info = info;
	struct decoder_dspfunc_info *decoder_info =
			(void *)dsp_data_to_mcu_address(POINTER_TO_UINT(player_info->decoder_info));
	//struct decoder_dspfunc_info *streamout_info =
	//		(void *)dsp_data_to_mcu_address(POINTER_TO_UINT(player_info->streamout_info));
//	struct player_dspfunc_debug *debug = &player_info->debug;

	printk("\nfunction player (id=%u):\n", DSP_FUNCTION_PLAYER);
	printk("\tparameters:\n");
	printk("\t\tdae.pbuf=%p\n", (void *)dsp_data_to_mcu_address(player_info->dae.pbuf));
	printk("\t\tdae.pbufsz=%u\n", player_info->dae.pbufsz);

#if 0
	if (player_info->aec.enable) {
		printk("\t\taec.delay=%d\n", player_info->aec.delay);
		printk("\t\taec.refbuf=%p (space=%u/%u)\n",
			buf_dsp2cpu(player_info->aec.channel[0].refbuf),
			dsp_session_buf_space(buf_dsp2cpu(player_info->aec.channel[0].refbuf)),
			dsp_session_buf_size(buf_dsp2cpu(player_info->aec.channel[0].refbuf)));
	}
#endif
#if 0
    print_ringbuf_info("dacbuf", player_info->output.outbuf, 1);

	if (!dump_debug) {
		dump_decoder_function(decoder_info, false);
		return;
	}

	printk("\tdebug:\n");
	if (debug->decode_stream) {
        print_ringbuf_info("decode_stream", debug->decode_stream, 1);
	}
	if (debug->decode_data) {
	    print_ringbuf_info("decode_data", debug->decode_data, 1);
	}
	if (debug->plc_data) {
	    print_ringbuf_info("plc_data", debug->plc_data, 1);
	}
#endif
	if (player_info->decoder_info){
		dump_decoder_function(decoder_info, false);
	}

	//if (player_info->streamout_info){
	//	dump_streamout_function(streamout_info, false);
	//}
}

static void dump_recorder_function(void *info, bool dump_debug)
{
	struct recorder_dspfunc_info *recorder_info = info;
	struct encoder_dspfunc_info *encoder_info =
		(void *)dsp_data_to_mcu_address((u32_t)recorder_info->encoder_info);
	struct recorder_dspfunc_debug *debug = &recorder_info->debug;

	printk("\nfunction recorder (id=%u):\n", DSP_FUNCTION_RECORDER);
	printk("\tparameters:\n");
	printk("\t\tdae.pbuf=%p\n", (void *)dsp_data_to_mcu_address(recorder_info->dae.pbuf));
	printk("\t\tdae.pbufsz=%u\n", recorder_info->dae.pbufsz);
	if (recorder_info->aec.enable) {
	    print_ringbuf_info("aec.refbuf", recorder_info->aec.refbuf[0], 0);
	}
	if (recorder_info->aec.inbuf)
    	print_ringbuf_info("adcbuf", recorder_info->aec.inbuf, 0);

	if (!dump_debug) {
		dump_encoder_function(encoder_info, false);
		return;
	}

	printk("\tdebug:\n");
	if (debug->mic1_data) {
        print_ringbuf_info("mic1_data", debug->mic1_data, 1);
	}
	if (debug->mic2_data) {
        print_ringbuf_info("mic2_data", debug->mic2_data, 1);

	}
	if (debug->ref_data) {
        print_ringbuf_info("ref_data", debug->ref_data, 1);
	}
	if (debug->aec1_data) {
        print_ringbuf_info("aec1_data", debug->aec1_data, 1);
	}
	if (debug->aec2_data) {
        print_ringbuf_info("aec2_data", debug->aec2_data, 1);
	}
	if (debug->encode_data) {
        print_ringbuf_info("encode_data", debug->encode_data, 1);
	}
	if (debug->encode_stream) {
	    print_ringbuf_info("encode_stream", debug->encode_stream, 1);
	}

	dump_encoder_function(encoder_info, false);
}

void dsp_session_dump_function(struct dsp_session *session, unsigned int func)
{
	struct dsp_request_function request = { .id = func, };
	dsp_request_userinfo(session->dev, DSP_REQUEST_FUNCTION_INFO, &request);

	if (request.info == NULL)
		return;

	switch (func) {
#if 0
	case DSP_FUNCTION_TEST:
		dump_test_function(request.info, true);
		break;
#endif
	case DSP_FUNCTION_ENCODER:
		dump_encoder_function(request.info, false);
		break;
	case DSP_FUNCTION_DECODER:
		dump_decoder_function(request.info, false);
		break;
	case DSP_FUNCTION_PLAYER:
		dump_player_function(request.info, true);
		break;
	case DSP_FUNCTION_RECORDER:
		dump_recorder_function(request.info, true);
		break;
	case DSP_FUNCTION_STREAMOUT:
		dump_streamout_function(request.info, true);
		break;
	default:
		break;
	}
}

unsigned int dsp_session_get_samples_count(struct dsp_session *session, unsigned int func)
{
	union {
		struct test_dspfunc_info *test;
		struct encoder_dspfunc_info *encoder;
		struct decoder_dspfunc_info *decoder;
	} func_info;

	if (func == DSP_FUNCTION_PLAYER)
		func = DSP_FUNCTION_DECODER;
	else if (func == DSP_FUNCTION_RECORDER)
		func = DSP_FUNCTION_ENCODER;

	struct dsp_request_function request = { .id = func, };
	dsp_request_userinfo(session->dev, DSP_REQUEST_FUNCTION_INFO, &request);
	if (request.info == NULL)
		return 0;

	switch (func) {
#if 0
	case DSP_FUNCTION_TEST:
		func_info.test = request.info;
		return func_info.test->runtime.sample_count;
#endif
	case DSP_FUNCTION_ENCODER:
		func_info.encoder = request.info;
		return func_info.encoder->runtime.sample_count;
	case DSP_FUNCTION_DECODER:
		func_info.decoder = request.info;
		return func_info.decoder->runtime.sample_count;
	default:
		return 0;
	}
}

unsigned int dsp_session_get_datalost_count(struct dsp_session *session, unsigned int func)
{
	struct dsp_request_function request = { .id = DSP_FUNCTION_DECODER, };

	if (func != DSP_FUNCTION_PLAYER && func != DSP_FUNCTION_DECODER)
		return 0;

	dsp_request_userinfo(session->dev, DSP_REQUEST_FUNCTION_INFO, &request);
	if (request.info) {
		struct decoder_dspfunc_info *decoder = request.info;
		return decoder->runtime.datalost_count;
	}

	return 0;
}

unsigned int dsp_session_get_raw_count(struct dsp_session *session, unsigned int func)
{
	struct dsp_request_function request = { .id = DSP_FUNCTION_DECODER, };

	if (func != DSP_FUNCTION_PLAYER && func != DSP_FUNCTION_DECODER)
		return 0;

	dsp_request_userinfo(session->dev, DSP_REQUEST_FUNCTION_INFO, &request);
	if (request.info) {
		struct decoder_dspfunc_info *decoder = request.info;
		return decoder->runtime.raw_count;
	}

	return 0;
}

int dsp_session_get_recoder_param(struct dsp_session *session, int param_type, void *param)
{
	struct dsp_request_session session_request;

	struct dsp_request_function request = { .id = DSP_FUNCTION_RECORDER, };

    dsp_request_userinfo(session->dev, DSP_REQUEST_SESSION_INFO, &session_request);

    if(!(session_request.func_enabled & DSP_FUNC_BIT(DSP_FUNCTION_RECORDER))){
        return -EINVAL;
    }

    dsp_request_userinfo(session->dev, DSP_REQUEST_FUNCTION_INFO, &request);
	if (request.info) {
		struct recorder_dspfunc_info *encoder = request.info;
		if(param_type == DSP_CONFIG_AEC){
            memcpy(param, &encoder->aec, sizeof(struct aec_dspfunc_params));
            return 0;
		}
	}

	return -EINVAL;
}

int dsp_session_get_function_runable(struct dsp_session *session, int function_type)
{
	struct dsp_request_session session_request;

    dsp_request_userinfo(session->dev, DSP_REQUEST_SESSION_INFO, &session_request);

    if((session_request.func_runnable & DSP_FUNC_BIT(function_type))){
        return true;
    }

	return false;
}

int dsp_session_get_function_enable(struct dsp_session *session, int function_type)
{
	struct dsp_request_session session_request;

    dsp_request_userinfo(session->dev, DSP_REQUEST_SESSION_INFO, &session_request);

    if((session_request.func_enabled & DSP_FUNC_BIT(function_type))){
        return true;
    }

	return false;
}


