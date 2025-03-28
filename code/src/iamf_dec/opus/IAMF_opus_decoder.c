/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

/**
 * @file IAMF_opus_decoder.c
 * @brief opus codec.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include <stdlib.h>

#include "IAMF_codec.h"
#include "IAMF_debug.h"
#include "IAMF_types.h"
#include "bitstream.h"
#include "opus_multistream2_decoder.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_OPUS"

typedef struct IAMF_OPUS_Context {
  void *dec;
  short *out;
} IAMF_OPUS_Context;

static int iamf_opus_close(IAMF_CodecContext *ths);

/**
 *  IAC-OPUS Specific
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |  Version = 1  | Channel Count |           Pre-skip            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                     Input Sample Rate (Hz)                    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Output Gain (Q7.8 in dB)    | Mapping Family|
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  The fields in the ID Header:
 *
 *  unsigned int(8) Version;
 *  unsigned int(8) OutputChannelCount;
 *  unsigned int(16) PreSkip;
 *  unsigned int(32) InputSampleRate;
 *  signed int(16) OutputGain;
 *  unsigned int(8) ChannelMappingFamily;
 *
 *
 *  Constraints:
 *
 *  1, ChannelMappingFamily = 0
 *  2, Channel Count should be set to 2
 *  3, Output Gain shall not be used. In other words, it shall be set to 0dB
 *  4, The byte order of each field in ID Header shall be converted to big
 * endian
 *
 * */
static int iamf_opus_init(IAMF_CodecContext *ths) {
  IAMF_OPUS_Context *ctx = (IAMF_OPUS_Context *)ths->priv;
  int ec = IAMF_OK;
  int ret = 0;
  uint8_t *config = ths->cspec;

  if (!ths->cspec || ths->clen <= 0) {
    return IAMF_ERR_BAD_ARG;
  }

  ths->sample_rate = 48000;  // readu32be(config, 4);
  ths->channel_mapping_family = config[10];

  ctx->dec = opus_multistream2_decoder_create(ths->sample_rate, ths->streams,
                                              ths->coupled_streams,
                                              AUDIO_FRAME_PLANE, &ret);
  if (!ctx->dec) {
    ia_loge("fail to open opus decoder.");
    ec = IAMF_ERR_INVALID_STATE;
  }

  ctx->out = (short *)malloc(sizeof(short) * MAX_OPUS_FRAME_SIZE *
                             (ths->streams + ths->coupled_streams));
  if (!ctx->out) {
    iamf_opus_close(ths);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return ec;
}

static int iamf_opus_decode(IAMF_CodecContext *ths, uint8_t *buf[],
                            uint32_t len[], uint32_t count, void *pcm,
                            const uint32_t frame_size) {
  IAMF_OPUS_Context *ctx = (IAMF_OPUS_Context *)ths->priv;
  OpusMS2Decoder *dec = (OpusMS2Decoder *)ctx->dec;
  int ret = IAMF_OK;

  if (count != ths->streams) {
    return IAMF_ERR_BAD_ARG;
  }
  ret = opus_multistream2_decode(dec, buf, len, ctx->out, frame_size);
  if (ret > 0) {
    float *out = (float *)pcm;
    uint32_t samples = ret * (ths->streams + ths->coupled_streams);
    for (int i = 0; i < samples; ++i) {
      out[i] = ctx->out[i] / 32768.f;
    }
  }
  return ret;
}

int iamf_opus_close(IAMF_CodecContext *ths) {
  IAMF_OPUS_Context *ctx = (IAMF_OPUS_Context *)ths->priv;
  OpusMS2Decoder *dec = (OpusMS2Decoder *)ctx->dec;

  if (dec) {
    opus_multistream2_decoder_destroy(dec);
    ctx->dec = 0;
  }

  if (ctx->out) {
    free(ctx->out);
  }
  return IAMF_OK;
}

const IAMF_Codec iamf_opus_decoder = {
    .cid = IAMF_CODEC_OPUS,
    .priv_size = sizeof(IAMF_OPUS_Context),
    .init = iamf_opus_init,
    .decode = iamf_opus_decode,
    .close = iamf_opus_close,
};
