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
 * @file IAMF_decoder_private.h
 * @brief IAMF decoder internal APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DECODER_PRIVATE_H
#define IAMF_DECODER_PRIVATE_H

#include <stdint.h>

#include "IAMF_OBU.h"
#include "IAMF_core_decoder.h"
#include "IAMF_decoder.h"
#include "IAMF_defines.h"
#include "IAMF_types.h"
#include "ae_rdr.h"
#include "arch.h"
#include "audio_effect_peak_limiter.h"
#include "demixer.h"
#include "downmix_renderer.h"
#include "queue_t.h"
#include "speex_resampler.h"

#define IAMF_FLAG_MAGIC_CODE 0x01
#define IAMF_FLAG_CODEC_CONFIG 0x02
#define IAMF_FLAG_AUDIO_ELEMENT 0x04
#define IAMF_FLAG_MIX_PRESENTATION 0x08
#define IAMF_FLAG_CONFIG 0x10
#define IAMF_FLAG_FRAME_START 0x20
#define IAMF_FLAG_DESCRIPTORS                                                \
  (IAMF_FLAG_MAGIC_CODE | IAMF_FLAG_CODEC_CONFIG | IAMF_FLAG_AUDIO_ELEMENT | \
   IAMF_FLAG_MIX_PRESENTATION)

#define DEC_BUF_CNT 3

typedef enum {
  IA_CH_GAIN_RTF,
  IA_CH_GAIN_LTF,
  IA_CH_GAIN_RS,
  IA_CH_GAIN_LS,
  IA_CH_GAIN_R,
  IA_CH_GAIN_L,
  IA_CH_GAIN_COUNT
} IAOutputGainChannel;

typedef enum {
  IAMF_DECODER_STATUS_UNINIT,
  IAMF_DECODER_STATUS_INIT,
  IAMF_DECODER_STATUS_CONFIGURE,
  IAMF_DECODER_STATUS_RECONFIGURE,
  IAMF_DECODER_STATUS_RECEIVE,
  IAMF_DECODER_STATUS_RUN,
} IAMF_DecoderStatus;

/* >>>>>>>>>>>>>>>>>> DATABASE >>>>>>>>>>>>>>>>>> */

typedef void (*IAMF_Free)(void *);

typedef struct ObjectSet {
  void **items;
  int count;
  int capacity;
  IAMF_Free objFree;
} ObjectSet;

typedef struct MixGainUnit {
  int count;
  float constant_gain;
  float *gains;
} MixGainUnit;

typedef struct MixGain {
  float default_mix_gain;
  int use_default;
} MixGain;

typedef struct ParameterValue {
  queue_t *params;
  union {
    MixGain mix_gain;
  };
} ParameterValue;

typedef struct ParameterItem {
  uint64_t id;
  uint64_t type;
  uint64_t timestamp;
  uint64_t duration;
  uint64_t elapse;
  uint64_t parent_id;
  int rate;
  int enabled;

  IAMF_ParameterParam param;
  ParameterValue value;
} ParameterItem;

typedef struct ElementItem {
  uint64_t id;

  IAMF_CodecConf *codecConf;
  IAMF_Element *element;

  uint64_t demixingPid;
  uint64_t reconGainPid;
  uint64_t mixGainPid;
} ElementItem;

typedef void (*free_tp)(void *);

typedef struct Viewer {
  void **items;
  int count;
  free_tp freeF;
} Viewer;

typedef struct IAMF_DataBase {
  IAMF_Object *version;

  ObjectSet *codecConf;
  ObjectSet *element;
  ObjectSet *mixPresentation;

  Viewer eViewer;
  Viewer pViewer;

  IAMF_Profile profile;
} IAMF_DataBase;

/* <<<<<<<<<<<<<<<<<< DATABASE <<<<<<<<<<<<<<<<<< */

typedef struct LayoutInfo {
  IAMF_SP_LAYOUT sp;
  IAMF_Layout layout;
  int channels;
} LayoutInfo;

typedef struct IAMF_OutputGain {
  uint32_t flags;
  float gain;
} IAMF_OutputGain;

typedef struct IAMF_ReconGain {
  uint16_t flags;
  uint16_t nb_channels;
  float recon_gain[IA_CH_RE_COUNT];
  IAChannel order[IA_CH_RE_COUNT];
} IAMF_ReconGain;

typedef struct SubLayerConf {
  uint32_t layout;
  uint8_t nb_channels;
  uint8_t nb_substreams;
  uint8_t nb_coupled_substreams;
  float loudness;
  IAMF_OutputGain *output_gain;
  IAMF_ReconGain *recon_gain;
} SubLayerConf;

typedef struct ChannelLayerContext {
  int nb_layers;
  SubLayerConf *conf_s;

  int is_scaleable_stream;
  int layer;
  int layout;
  int channels;
  IAChannel channels_order[IA_CH_LAYOUT_MAX_CHANNELS];

  int dmx_mode;
  int dmx_default_mode;
  int dmx_default_w_idx;
} ChannelLayerContext;

typedef struct AmbisonicsContext {
  int mode;
  uint8_t *mapping;
  int mapping_size;
} AmbisonicsContext;

typedef struct IAMF_Stream {
  uint64_t element_id;
  uint64_t codecConf_id;
  IAMF_CodecID codec_id;

  int sampling_rate;
  uint8_t scheme;  // audio element type: 0, CHANNEL_BASED; 1, SCENE_BASED

  int nb_channels;
  int nb_substreams;
  int nb_coupled_substreams;

  LayoutInfo *final_layout;
  void *priv;

  uint64_t timestamp;  // sync time

  uint64_t trimming_start;
  uint64_t trimming_end;

  uint32_t max_frame_size;

  uint8_t headphones_rendering_mode;
} IAMF_Stream;

typedef struct ScalableChannelDecoder {
  int nb_layers;
  IAMF_CoreDecoder **sub_decoders;
  Demixer *demixer;
} ScalableChannelDecoder;

typedef struct AmbisonicsDecoder {
  IAMF_CoreDecoder *decoder;
} AmbisonicsDecoder;

#define IAMF_FRAME_EXTRA_DEMIX_MODE 0x01
#define IAMF_FRAME_EXTRA_RECON_GAIN 0x02
#define IAMF_FRAME_EXTRA_MIX_GAIN 0x04

typedef struct Packet {
  uint8_t **sub_packets;
  uint32_t *sub_packet_sizes;
  uint32_t nb_sub_packets;
  uint32_t count;
} Packet;

typedef struct Frame {
  uint64_t id;
  uint64_t pts;
  uint32_t samples;
  uint64_t strim;
  uint64_t etrim;
  int channels;

  float *data;
} Frame;

typedef struct IAMF_StreamDecoder {
  union {
    ScalableChannelDecoder *scale;
    AmbisonicsDecoder *ambisonics;
  };

  IAMF_Stream *stream;
  float *buffers[DEC_BUF_CNT];

  Packet packet;
  Frame frame;

  uint32_t frame_size;
  int frame_padding;
  int delay;

} IAMF_StreamDecoder;

typedef struct IAMF_StreamRenderer {
  IAMF_Stream *stream;
  DMRenderer *downmixer;
  uint32_t offset;
  uint32_t frame_size;
  uint8_t headphones_rendering_mode;
  struct {
    IAMF_SP_LAYOUT *layout;
    union {
      struct m2m_rdr_t mmm;
      struct h2m_rdr_t hmm;
    };
    int in_channel_map[IA_CH_LAYOUT_MAX_CHANNELS];
  } renderer;
} IAMF_StreamRenderer;

typedef struct IAMF_Mixer {
  uint64_t *element_ids;
  int nb_elements;
  Frame **frames;
} IAMF_Mixer;

typedef struct IAMF_Resampler {
  float *buffer;
  uint32_t rest_flag;
  SpeexResamplerState *speex_resampler;
} IAMF_Resampler;

typedef struct IAMF_Presentation {
  IAMF_MixPresentation *obj;

  IAMF_Stream **streams;
  uint32_t nb_streams;
  IAMF_StreamDecoder **decoders;
  IAMF_StreamRenderer **renderers;
  IAMF_Resampler *resampler;
  IAMF_Mixer mixer;
  uint64_t output_gain_id;
  Frame frame;
} IAMF_Presentation;

typedef struct IAMF_DecoderContext {
  IAMF_DataBase db;
  uint32_t flags;
  IAMF_DecoderStatus status;

  IAMF_Layout layout;
  LayoutInfo *output_layout;
  int sampling_rate;

  uint64_t mix_presentation_id;
  IAMF_Presentation *presentation;

  float loudness;
  float normalization_loudness;
  uint32_t bit_depth;
  float threshold_db;
  IAMF_StreamInfo info;

  uint32_t need_configure;

  // PTS
  uint64_t duration;
  int64_t pts;
  uint32_t pts_time_base;
  uint32_t last_frame_size;
  uint32_t time_precision;

  IAMF_extradata metadata;

} IAMF_DecoderContext;

struct IAMF_Decoder {
  IAMF_DecoderContext ctx;
  AudioEffectPeakLimiter *limiter;
  Arch *arch;
};

#endif /* IAMF_DECODER_PRIVATE_H */
