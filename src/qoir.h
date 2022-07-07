// Copyright 2022 Nigel Tao.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef QOIR_INCLUDE_GUARD
#define QOIR_INCLUDE_GUARD

// QOIR is a fast, simple image file format.
//
// Most users will want the qoir_decode and qoir_encode functions, which read
// from and write to a contiguous block of memory.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TODO: remove the dependency.
#ifdef QOIR_IMPLEMENTATION
#define QOI_IMPLEMENTATION
#endif
#include "../third_party/qoi/qoi.h"

#ifdef __cplusplus
extern "C" {
#endif

// ================================ +Public Interface

// QOIR ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define QOIR_IMPLEMENTATION before #include'ing or
// compiling it.

// -------- Compile-time Configuration

// Define QOIR_CONFIG__STATIC_FUNCTIONS (combined with QOIR_IMPLEMENTATION) to
// make all of QOIR's functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#if defined(QOIR_CONFIG__STATIC_FUNCTIONS)
#define QOIR_MAYBE_STATIC static
#else
#define QOIR_MAYBE_STATIC
#endif  // defined(QOIR_CONFIG__STATIC_FUNCTIONS)

// -------- Status Messages

extern const char qoir_status_message__error_invalid_argument[];
extern const char qoir_status_message__error_invalid_data[];
extern const char qoir_status_message__error_out_of_memory[];
extern const char qoir_status_message__error_unsupported_pixbuf[];
extern const char qoir_status_message__error_unsupported_pixfmt[];

// -------- Pixel Buffers

// A pixel format combines an alpha transparency choice, a color model choice
// and other configuration (such as pixel byte order).
//
// Values less than 0x10 are directly representable by the file format (and by
// this implementation's API), using the same bit pattern.
//
// Values greater than or equal to 0x10 are representable by the API but not by
// the file format:
//  - the 0x10 bit means 3 (not 4) bytes per (fully opaque) pixel.
//  - the 0x20 bit means RGBA (not BGRA) byte order.

typedef uint32_t qoir_pixel_alpha_transparency;
typedef uint32_t qoir_pixel_color_model;
typedef uint32_t qoir_pixel_format;

// clang-format off

#define QOIR_PIXEL_ALPHA_TRANSPARENCY__OPAQUE                  0x01
#define QOIR_PIXEL_ALPHA_TRANSPARENCY__NONPREMULTIPLIED_ALPHA  0x02
#define QOIR_PIXEL_ALPHA_TRANSPARENCY__PREMULTIPLIED_ALPHA     0x03

#define QOIR_PIXEL_COLOR_MODEL__BGRA  0x00

#define QOIR_PIXEL_FORMAT__MASK_FOR_ALPHA_TRANSPARENCY  0x03
#define QOIR_PIXEL_FORMAT__MASK_FOR_COLOR_MODEL         0x0C

#define QOIR_PIXEL_FORMAT__INVALID         0x00
#define QOIR_PIXEL_FORMAT__BGRX            0x01
#define QOIR_PIXEL_FORMAT__BGRA_NONPREMUL  0x02
#define QOIR_PIXEL_FORMAT__BGRA_PREMUL     0x03
#define QOIR_PIXEL_FORMAT__BGR             0x11
#define QOIR_PIXEL_FORMAT__RGBX            0x21
#define QOIR_PIXEL_FORMAT__RGBA_NONPREMUL  0x22
#define QOIR_PIXEL_FORMAT__RGBA_PREMUL     0x23
#define QOIR_PIXEL_FORMAT__RGB             0x31

// clang-format on

typedef struct qoir_pixel_configuration_struct {
  qoir_pixel_format pixfmt;
  uint32_t width_in_pixels;
  uint32_t height_in_pixels;
} qoir_pixel_configuration;

typedef struct qoir_pixel_buffer_struct {
  qoir_pixel_configuration pixcfg;
  uint8_t* data;
  size_t stride_in_bytes;
} qoir_pixel_buffer;

static inline uint32_t  //
qoir_pixel_format__bytes_per_pixel(qoir_pixel_format pixfmt) {
  return (pixfmt & 0x10) ? 3 : 4;
}

// -------- QOIR Decode / Encode

typedef struct qoir_decode_pixel_configuration_result_struct {
  const char* status_message;
  qoir_pixel_configuration dst_pixcfg;
} qoir_decode_pixel_configuration_result;

QOIR_MAYBE_STATIC qoir_decode_pixel_configuration_result  //
qoir_decode_pixel_configuration(                          //
    uint8_t* src_ptr,                                     //
    size_t src_len);

typedef struct qoir_decode_result_struct {
  const char* status_message;
  void* owned_memory;
  qoir_pixel_buffer dst_pixbuf;
} qoir_decode_result;

typedef struct qoir_decode_options_struct {
  qoir_pixel_format pixfmt;
} qoir_decode_options;

QOIR_MAYBE_STATIC qoir_decode_result  //
qoir_decode(                          //
    const uint8_t* src_ptr,           //
    size_t src_len,                   //
    qoir_decode_options* options);

typedef struct qoir_encode_result_struct {
  const char* status_message;
  void* owned_memory;
  uint8_t* dst_ptr;
  size_t dst_len;
} qoir_encode_result;

typedef struct qoir_encode_options_struct {
  uint32_t todo;
} qoir_encode_options;

QOIR_MAYBE_STATIC qoir_encode_result  //
qoir_encode(                          //
    qoir_pixel_buffer* src_pixbuf,    //
    qoir_encode_options* options);

// ================================ -Public Interface

#ifdef QOIR_IMPLEMENTATION

// ================================ +Private Implementation

static inline uint32_t  //
qoir_private_peek_u32be(const uint8_t* p) {
  return ((uint32_t)(p[0]) << 24) | ((uint32_t)(p[1]) << 16) |
         ((uint32_t)(p[2]) << 8) | ((uint32_t)(p[3]) << 0);
}

static inline uint32_t  //
qoir_private_hash(const uint8_t* p) {
  return 63 & ((0x03 * (uint32_t)p[0]) +  //
               (0x05 * (uint32_t)p[1]) +  //
               (0x07 * (uint32_t)p[2]) +  //
               (0x0B * (uint32_t)p[3]));
}

// -------- Status Messages

const char qoir_status_message__error_invalid_argument[] =  //
    "#qoir: invalid argument";
const char qoir_status_message__error_invalid_data[] =  //
    "#qoir: invalid data";
const char qoir_status_message__error_out_of_memory[] =  //
    "#qoir: out of memory";
const char qoir_status_message__error_unsupported_pixbuf[] =  //
    "#qoir: unsupported pixbuf";
const char qoir_status_message__error_unsupported_pixfmt[] =  //
    "#qoir: unsupported pixfmt";

// -------- QOIR Decode / Encode

static const char*                  //
qoir_private_decode_tile(           //
    qoir_pixel_format dst_pixfmt,   //
    uint32_t dst_width_in_pixels,   //
    uint32_t dst_height_in_pixels,  //
    uint8_t* dst_data,              //
    size_t dst_stride_in_bytes,     //
    qoir_pixel_format src_pixfmt,   //
    const uint8_t* src_ptr,         //
    size_t src_len) {
  if (src_len < 8) {
    return qoir_status_message__error_invalid_data;
  }

  uint32_t run_length = 0;
  // TODO: support dst pixfmt values other than RGB and RGBA_NONPREMUL.
  bool dst_has_alpha = qoir_pixel_format__bytes_per_pixel(dst_pixfmt) == 4;
  // The array-of-four-uint8_t elements are in R, G, B, A order.
  uint8_t color_cache[64][4] = {0};
  uint8_t pixel[4] = {0};
  pixel[3] = 0xFF;

  // TODO: dst pixbuf isn't always tightly packed (so stride != width * bpp).
  uint8_t* dp = dst_data;
  uint8_t* dq = dst_data + (dst_height_in_pixels * dst_stride_in_bytes);
  const uint8_t* sp = src_ptr;
  const uint8_t* sq = src_ptr + src_len - 8;
  while (dp < dq) {
    if (run_length > 0) {
      run_length--;

    } else if (sp < sq) {
      uint8_t s0 = *sp++;
      if (s0 == 0xFE) {  // QOI_OP_RGB
        pixel[0] = *sp++;
        pixel[1] = *sp++;
        pixel[2] = *sp++;
      } else if (s0 == 0xFF) {  // QOI_OP_RGBA
        pixel[0] = *sp++;
        pixel[1] = *sp++;
        pixel[2] = *sp++;
        pixel[3] = *sp++;
      } else {
        switch (s0 >> 6) {
          case 0: {  // QOI_OP_INDEX
            memcpy(pixel, color_cache[s0], 4);
            break;
          }
          case 1: {  // QOI_OP_DIFF
            pixel[0] += ((s0 >> 4) & 0x03) - 2;
            pixel[1] += ((s0 >> 2) & 0x03) - 2;
            pixel[2] += ((s0 >> 0) & 0x03) - 2;
            break;
          }
          case 2: {  // QOI_OP_LUMA
            uint8_t s1 = *sp++;
            uint8_t delta_g = (s0 & 0x3F) - 32;
            pixel[0] += delta_g - 8 + (s1 >> 4);
            pixel[1] += delta_g;
            pixel[2] += delta_g - 8 + (s1 & 0x0F);
            break;
          }
          case 3: {  // QOI_OP_RUN
            run_length = s0 & 0x3F;
            break;
          }
        }
      }
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
    }

    if (dst_has_alpha) {
      memcpy(dp, pixel, 4);
      dp += 4;
    } else {
      memcpy(dp, pixel, 3);
      dp += 3;
    }
  }

  return NULL;
}

QOIR_MAYBE_STATIC qoir_decode_pixel_configuration_result  //
qoir_decode_pixel_configuration(                          //
    uint8_t* src_ptr,                                     //
    size_t src_len) {
  qoir_decode_pixel_configuration_result result = {0};
  result.status_message = "#TODO";
  return result;
}

QOIR_MAYBE_STATIC qoir_decode_result  //
qoir_decode(                          //
    const uint8_t* src_ptr,           //
    size_t src_len,                   //
    qoir_decode_options* options) {
  qoir_decode_result result = {0};
  if ((src_len < 14) || (qoir_private_peek_u32be(src_ptr) != 0x716F6966)) {
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }
  uint32_t width = qoir_private_peek_u32be(src_ptr + 4);
  uint32_t height = qoir_private_peek_u32be(src_ptr + 8);
  qoir_pixel_format src_pixfmt = QOIR_PIXEL_FORMAT__INVALID;
  if (src_ptr[12] == 3) {
    src_pixfmt = QOIR_PIXEL_FORMAT__RGB;
  } else if (src_ptr[12] == 4) {
    src_pixfmt = QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  } else {
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }

  qoir_pixel_format dst_pixfmt = (options && options->pixfmt)
                                     ? options->pixfmt
                                     : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  size_t dst_bpp = qoir_pixel_format__bytes_per_pixel(dst_pixfmt);
  size_t pixbuf_len = width * height * dst_bpp;  // TODO: handle overflow.
  uint8_t* pixbuf_data = malloc(pixbuf_len);
  if (!pixbuf_data) {
    result.status_message = qoir_status_message__error_out_of_memory;
    return result;
  }
  const char* status_message = qoir_private_decode_tile(
      dst_pixfmt, width, height, pixbuf_data, dst_bpp * (size_t)width,
      src_pixfmt, src_ptr + 14, src_len - 14);
  if (status_message) {
    result.status_message = status_message;
    free(pixbuf_data);
    return result;
  }

  result.owned_memory = pixbuf_data;
  result.dst_pixbuf.pixcfg.pixfmt = dst_pixfmt;
  result.dst_pixbuf.pixcfg.width_in_pixels = width;
  result.dst_pixbuf.pixcfg.height_in_pixels = height;
  result.dst_pixbuf.data = pixbuf_data;
  result.dst_pixbuf.stride_in_bytes = dst_bpp * (size_t)width;
  return result;
}

QOIR_MAYBE_STATIC qoir_encode_result  //
qoir_encode(                          //
    qoir_pixel_buffer* src_pixbuf,    //
    qoir_encode_options* options) {
  qoir_encode_result result = {0};
  if (!src_pixbuf) {
    result.status_message = qoir_status_message__error_invalid_argument;
    return result;
  }
  qoi_desc desc;
  desc.width = src_pixbuf->pixcfg.width_in_pixels;
  desc.height = src_pixbuf->pixcfg.height_in_pixels;
  desc.channels = 0;
  desc.colorspace = QOI_SRGB;

  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      desc.channels = 3;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      desc.channels = 4;
      break;
    default:
      result.status_message = qoir_status_message__error_unsupported_pixfmt;
      return result;
  }

  if (src_pixbuf->stride_in_bytes !=
      ((size_t)desc.channels * src_pixbuf->pixcfg.width_in_pixels)) {
    result.status_message = qoir_status_message__error_unsupported_pixbuf;
    return result;
  }
  int encoded_qoi_len = 0;
  void* encoded_qoi_ptr = qoi_encode(src_pixbuf->data, &desc, &encoded_qoi_len);

  result.owned_memory = encoded_qoi_ptr;
  result.dst_ptr = encoded_qoi_ptr;
  result.dst_len = encoded_qoi_len;
  return result;
}

// ================================ -Private Implementation

#endif  // QOIR_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // QOIR_INCLUDE_GUARD
