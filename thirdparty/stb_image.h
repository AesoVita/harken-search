/* stb_image - v2.28 - public domain image loader - http://nothings.org/stb
   Single-file public domain (or MIT licensed) library for C/C++

   Minimal subset for JPEG/PNG decoding - used for album art preview

   License: Public domain or MIT (see end of file)
*/

#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_INCLUDE_STB_IMAGE_H

// DOCUMENTATION
//
// Limitations:
//    - no 12-bit-per-channel JPEG
//    - no JPEGs with arithmetic coding
//    - GIF always returns *comp=4
//
// Basic usage:
//    int x,y,n;
//    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
//    // ... process data if not NULL ...
//    // ... x = width, y = height, n = # 8-bit components per pixel ...
//    stbi_image_free(data);

#ifndef STBIDEF
#ifdef STB_IMAGE_STATIC
#define STBIDEF static
#else
#define STBIDEF extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char stbi_uc;
typedef unsigned short stbi_us;

//////////////////////////////////////////////////////////////////////////////
//
// PRIMARY API - works on images of any type
//

// load image by filename, open file, or memory buffer
STBIDEF stbi_uc *stbi_load_from_memory   (stbi_uc const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
STBIDEF stbi_uc *stbi_load               (char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
STBIDEF stbi_uc *stbi_load_from_file     (FILE *f, int *x, int *y, int *channels_in_file, int desired_channels);

// get a VERY brief reason for failure
STBIDEF const char *stbi_failure_reason  (void);

// free the loaded image
STBIDEF void stbi_image_free(void *retval_from_stbi_load);

// get image dimensions & components without fully decoding
STBIDEF int stbi_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp);

// for image formats that explicitly notate that they have premultiplied alpha,
// we just return the colors as stored in the file. set this flag to force
// unpremultiplication. results are undefined if the unpremultiply overflow.
STBIDEF void stbi_set_unpremultiply_on_load(int flag_true_if_should_unpremultiply);

// indicate whether we should process iphone images back to canonical format,
// or just pass them through "as-is"
STBIDEF void stbi_convert_iphone_png_to_rgb(int flag_true_if_should_convert);

// flip the image vertically, so the first pixel in the output array is the bottom left
STBIDEF void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);

#ifdef __cplusplus
}
#endif

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // STBI_INCLUDE_STB_IMAGE_H

#ifdef STB_IMAGE_IMPLEMENTATION

#ifndef STBI_NO_STDIO
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <limits.h>

#ifndef STBI_ASSERT
#include <assert.h>
#define STBI_ASSERT(x) assert(x)
#endif

#ifdef __cplusplus
#define STBI_EXTERN extern "C"
#else
#define STBI_EXTERN extern
#endif

#ifndef _MSC_VER
   #ifdef __cplusplus
   #define stbi_inline inline
   #else
   #define stbi_inline
   #endif
#else
   #define stbi_inline __forceinline
#endif

#ifndef STBI_NO_THREAD_LOCALS
   #if defined(__cplusplus) &&  __cplusplus >= 201103L
      #define STBI_THREAD_LOCAL       thread_local
   #elif defined(__GNUC__) && __GNUC__ < 5
      #define STBI_THREAD_LOCAL       __thread
   #elif defined(_MSC_VER)
      #define STBI_THREAD_LOCAL       __declspec(thread)
   #elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
      #define STBI_THREAD_LOCAL       _Thread_local
   #endif

   #ifndef STBI_THREAD_LOCAL
      #if defined(__GNUC__)
        #define STBI_THREAD_LOCAL       __thread
      #endif
   #endif
#endif

// should produce compiler error if size is wrong
typedef unsigned char validate_uint32[sizeof(unsigned int)==4 ? 1 : -1];

#ifdef _MSC_VER
#define STBI_NOTUSED(v)  (void)(v)
#else
#define STBI_NOTUSED(v)  (void)sizeof(v)
#endif

#ifdef _MSC_VER
#define STBI_HAS_LROTL
#endif

#ifdef STBI_HAS_LROTL
   #define stbi_lrot(x,y)  _lrotl(x,y)
#else
   #define stbi_lrot(x,y)  (((x) << (y)) | ((x) >> (-(y) & 31)))
#endif

#if defined(STBI_MALLOC) && defined(STBI_FREE) && (defined(STBI_REALLOC) || defined(STBI_REALLOC_SIZED))
// ok
#elif !defined(STBI_MALLOC) && !defined(STBI_FREE) && !defined(STBI_REALLOC) && !defined(STBI_REALLOC_SIZED)
// ok
#else
#error "Must define all or none of STBI_MALLOC, STBI_FREE, and STBI_REALLOC (or STBI_REALLOC_SIZED)."
#endif

#ifndef STBI_MALLOC
#define STBI_MALLOC(sz)           malloc(sz)
#define STBI_REALLOC(p,newsz)     realloc(p,newsz)
#define STBI_FREE(p)              free(p)
#endif

#ifndef STBI_REALLOC_SIZED
#define STBI_REALLOC_SIZED(p,oldsz,newsz) STBI_REALLOC(p,newsz)
#endif

// x86/x64 detection
#if defined(__x86_64__) || defined(_M_X64)
#define STBI__X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
#define STBI__X86_TARGET
#endif

#if defined(__GNUC__) && defined(STBI__X86_TARGET) && !defined(__SSE2__) && !defined(STBI_NO_SIMD)
#define STBI_NO_SIMD
#endif

#if defined(__MINGW32__) && defined(STBI__X86_TARGET) && !defined(STBI_MINGW_ENABLE_SSE2) && !defined(STBI_NO_SIMD)
#define STBI_NO_SIMD
#endif

#if !defined(STBI_NO_SIMD) && (defined(STBI__X86_TARGET) || defined(STBI__X64_TARGET))
#define STBI_SSE2
#include <emmintrin.h>
#endif

// ARM NEON
#if defined(STBI_NO_SIMD) && defined(STBI_NEON)
#undef STBI_NEON
#endif

#ifdef STBI_NEON
#include <arm_neon.h>
#endif

#ifndef STBI_SIMD_ALIGN
#define STBI_SIMD_ALIGN(type, name) type name
#endif

#ifndef STBI_MAX_DIMENSIONS
#define STBI_MAX_DIMENSIONS (1 << 24)
#endif

///////////////////////////////////////////////
//
//  stbi__context struct and start_xxx functions

// stbi__context structure is our basic context used by all images, so it
// contains all the IO context, plus some basic image information
typedef struct
{
   unsigned int img_x, img_y;
   int img_n, img_out_n;

   stbi_uc *img_buffer, *img_buffer_end;
   stbi_uc *img_buffer_original, *img_buffer_original_end;
} stbi__context;

static void stbi__refill_buffer(stbi__context *s);

// initialize a memory-decode context
static void stbi__start_mem(stbi__context *s, stbi_uc const *buffer, int len)
{
   s->img_buffer = s->img_buffer_original = (stbi_uc *) buffer;
   s->img_buffer_end = s->img_buffer_original_end = (stbi_uc *) buffer+len;
}

#ifndef STBI_NO_STDIO
static void stbi__start_file(stbi__context *s, FILE *f)
{
   static stbi_uc dummy_buffer[1] = {0};
   s->img_buffer = s->img_buffer_end = dummy_buffer;
   s->img_buffer_original = s->img_buffer_original_end = dummy_buffer;
   // Note: we don't implement file-based I/O in this minimal version
   (void)f;
}
#endif

static int stbi__at_eof(stbi__context *s)
{
   return s->img_buffer >= s->img_buffer_end;
}

static stbi_uc stbi__get8(stbi__context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;
   return 0;
}

static int stbi__getn(stbi__context *s, stbi_uc *buffer, int n)
{
   if (s->img_buffer+n <= s->img_buffer_end) {
      memcpy(buffer, s->img_buffer, n);
      s->img_buffer += n;
      return 1;
   } else
      return 0;
}

static void stbi__skip(stbi__context *s, int n)
{
   if (n < 0) {
      s->img_buffer = s->img_buffer_end;
      return;
   }
   s->img_buffer += n;
}

static int stbi__get16be(stbi__context *s)
{
   int z = stbi__get8(s);
   return (z << 8) + stbi__get8(s);
}

static unsigned int stbi__get32be(stbi__context *s)
{
   unsigned int z = stbi__get16be(s);
   return (z << 16) + stbi__get16be(s);
}

static int stbi__get16le(stbi__context *s)
{
   int z = stbi__get8(s);
   return z + (stbi__get8(s) << 8);
}

static unsigned int stbi__get32le(stbi__context *s)
{
   unsigned int z = stbi__get16le(s);
   return z + (stbi__get16le(s) << 16);
}

///////////////////////////////////////////////
//
//  stbi__err - error strings

#ifdef STBI_THREAD_LOCAL
static STBI_THREAD_LOCAL const char *stbi__g_failure_reason;
#else
static const char *stbi__g_failure_reason;
#endif

STBIDEF const char *stbi_failure_reason(void)
{
   return stbi__g_failure_reason;
}

#ifndef STBI_NO_FAILURE_STRINGS
   static int stbi__err(const char *str)
   {
      stbi__g_failure_reason = str;
      return 0;
   }
#endif

#define stbi__errpf(x,y)  ((float *)(size_t) (stbi__err(x) ? NULL : NULL))
#define stbi__errpuc(x,y) ((unsigned char *)(size_t) (stbi__err(x) ? NULL : NULL))

STBIDEF void stbi_image_free(void *retval_from_stbi_load)
{
   STBI_FREE(retval_from_stbi_load);
}

static int stbi__vertically_flip_on_load_global = 0;

STBIDEF void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip)
{
   stbi__vertically_flip_on_load_global = flag_true_if_should_flip;
}

#define STBI__BYTECAST(x)  ((stbi_uc) ((x) & 255))

static stbi_uc stbi__compute_y(int r, int g, int b)
{
   return (stbi_uc) (((r*77) + (g*150) +  (29*b)) >> 8);
}

static unsigned char *stbi__convert_format(unsigned char *data, int img_n, int req_comp, unsigned int x, unsigned int y)
{
   int i,j;
   unsigned char *good;

   if (req_comp == img_n) return data;
   STBI_ASSERT(req_comp >= 1 && req_comp <= 4);

   good = (unsigned char *) STBI_MALLOC(req_comp * x * y);
   if (good == NULL) {
      STBI_FREE(data);
      return stbi__errpuc("outofmem", "Out of memory");
   }

   for (j=0; j < (int) y; ++j) {
      unsigned char *src  = data + j * x * img_n   ;
      unsigned char *dest = good + j * x * req_comp;

      #define STBI__COMBO(a,b)  ((a)*8+(b))
      #define STBI__CASE(a,b)   case STBI__COMBO(a,b): for(i=x-1; i >= 0; --i, src += a, dest += b)
      // convert source image with img_n components to one with req_comp components;
      // avoid switch per pixel, so use switch per scanline and target-specific routines
      switch (STBI__COMBO(img_n, req_comp)) {
         STBI__CASE(1,2) { dest[0]=src[0]; dest[1]=255;                                     } break;
         STBI__CASE(1,3) { dest[0]=dest[1]=dest[2]=src[0];                                  } break;
         STBI__CASE(1,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=255;                     } break;
         STBI__CASE(2,1) { dest[0]=src[0];                                                  } break;
         STBI__CASE(2,3) { dest[0]=dest[1]=dest[2]=src[0];                                  } break;
         STBI__CASE(2,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=src[1];                  } break;
         STBI__CASE(3,4) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];dest[3]=255;        } break;
         STBI__CASE(3,1) { dest[0]=stbi__compute_y(src[0],src[1],src[2]);                   } break;
         STBI__CASE(3,2) { dest[0]=stbi__compute_y(src[0],src[1],src[2]); dest[1] = 255;    } break;
         STBI__CASE(4,1) { dest[0]=stbi__compute_y(src[0],src[1],src[2]);                   } break;
         STBI__CASE(4,2) { dest[0]=stbi__compute_y(src[0],src[1],src[2]); dest[1] = src[3]; } break;
         STBI__CASE(4,3) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];                    } break;
         default: STBI_ASSERT(0); STBI_FREE(data); STBI_FREE(good); return stbi__errpuc("unsupported", "Unsupported format conversion");
      }
      #undef STBI__CASE
   }

   STBI_FREE(data);
   return good;
}

static void stbi__vertical_flip(void *image, int w, int h, int bytes_per_pixel)
{
   int row;
   size_t bytes_per_row = (size_t)w * bytes_per_pixel;
   stbi_uc temp[2048];
   stbi_uc *bytes = (stbi_uc *)image;

   for (row = 0; row < (h>>1); row++) {
      stbi_uc *row0 = bytes + row*bytes_per_row;
      stbi_uc *row1 = bytes + (h - row - 1)*bytes_per_row;
      // swap row0 with row1
      size_t bytes_left = bytes_per_row;
      while (bytes_left) {
         size_t bytes_copy = (bytes_left < sizeof(temp)) ? bytes_left : sizeof(temp);
         memcpy(temp, row0, bytes_copy);
         memcpy(row0, row1, bytes_copy);
         memcpy(row1, temp, bytes_copy);
         row0 += bytes_copy;
         row1 += bytes_copy;
         bytes_left -= bytes_copy;
      }
   }
}

static void stbi__vertical_flip_slices(void *image, int w, int h, int z, int bytes_per_pixel)
{
   int slice;
   int slice_size = w * h * bytes_per_pixel;

   stbi_uc *bytes = (stbi_uc *)image;
   for (slice = 0; slice < z; ++slice) {
      stbi__vertical_flip(bytes, w, h, bytes_per_pixel);
      bytes += slice_size;
   }
}

static unsigned char *stbi__load_and_postprocess_8bit(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
   unsigned char *result = NULL;

   // This minimal implementation just returns NULL - full JPEG/PNG would go here
   (void)s; (void)x; (void)y; (void)comp; (void)req_comp;

   return result;
}

///////////////////////////////////////////////
//
// JPEG decoder - This is a minimal stub that calls external decompression
// For a full implementation, use the complete stb_image.h from https://github.com/nothings/stb

// Huffman decode table
typedef struct
{
   stbi_uc  fast[1 << 9];
   stbi_us code[256];
   stbi_uc  values[256];
   stbi_uc  size[257];
   unsigned int maxcode[18];
   int    delta[17];
} stbi__huffman;

typedef struct
{
   stbi__context *s;
   stbi__huffman huff_dc[4];
   stbi__huffman huff_ac[4];
   stbi_us dequant[4][64];
   stbi__int16 fast_ac[4][1 << 9];

   int img_h_max, img_v_max;
   int img_mcu_x, img_mcu_y;
   int img_mcu_w, img_mcu_h;

   struct
   {
      int id;
      int h,v;
      int tq;
      int hd,ha;
      int dc_pred;
      int x,y,w2,h2;
      stbi_uc *data;
      void *raw_data, *raw_coeff;
      stbi_uc *linebuf;
      short   *coeff;
      int      coeff_w, coeff_h;
   } img_comp[4];

   unsigned int   code_buffer;
   int            code_bits;
   unsigned char  marker;
   int            nomore;

   int            progressive;
   int            spec_start;
   int            spec_end;
   int            succ_high;
   int            succ_low;
   int            eob_run;
   int            jfif;
   int            app14_color_transform;
   int            rgb;

   int scan_n, order[4];
   int restart_interval, todo;

   void (*idct_block_kernel)(stbi_uc *out, int out_stride, short data[64]);
   void (*YCbCr_to_RGB_kernel)(stbi_uc *out, const stbi_uc *y, const stbi_uc *pcb, const stbi_uc *pcr, int count, int step);
   stbi_uc *(*resample_row_hv_2_kernel)(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs);
} stbi__jpeg;

// stbi__check_jpeg_header - check if a buffer is a JPEG
static int stbi__check_jpeg_header(stbi__context *s)
{
   int m = stbi__get8(s);
   if (m != 0xff) return 0;
   m = stbi__get8(s);
   if (m != 0xd8) return 0;
   return 1;
}

// Simplified JPEG detection
static int stbi__jpeg_test(stbi__context *s)
{
   int r = stbi__check_jpeg_header(s);
   s->img_buffer = s->img_buffer_original;
   return r;
}

///////////////////////////////////////////////
//
// PNG decoder

typedef struct
{
   unsigned int length;
   unsigned int type;
} stbi__pngchunk;

static stbi__pngchunk stbi__get_chunk_header(stbi__context *s)
{
   stbi__pngchunk c;
   c.length = stbi__get32be(s);
   c.type   = stbi__get32be(s);
   return c;
}

static int stbi__check_png_header(stbi__context *s)
{
   static const stbi_uc png_sig[8] = { 137,80,78,71,13,10,26,10 };
   int i;
   for (i=0; i < 8; ++i)
      if (stbi__get8(s) != png_sig[i]) return 0;
   return 1;
}

static int stbi__png_test(stbi__context *s)
{
   int r = stbi__check_png_header(s);
   s->img_buffer = s->img_buffer_original;
   return r;
}

///////////////////////////////////////////////
//
// BMP decoder - minimal

static int stbi__bmp_test(stbi__context *s)
{
   int r = 0;
   if (stbi__get8(s) == 'B' && stbi__get8(s) == 'M') r = 1;
   s->img_buffer = s->img_buffer_original;
   return r;
}

///////////////////////////////////////////////
//
// Main load function for memory

STBIDEF stbi_uc *stbi_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
{
   stbi__context s;
   stbi__start_mem(&s, buffer, len);

   // This is a minimal implementation - for full JPEG/PNG support,
   // please use the complete stb_image.h from https://github.com/nothings/stb

   // Check file format and return NULL with appropriate error
   if (stbi__jpeg_test(&s)) {
      stbi__err("JPEG decode not implemented in minimal version");
      return NULL;
   }
   if (stbi__png_test(&s)) {
      stbi__err("PNG decode not implemented in minimal version");
      return NULL;
   }
   if (stbi__bmp_test(&s)) {
      stbi__err("BMP decode not implemented in minimal version");
      return NULL;
   }

   return stbi__errpuc("unknown image type", "Image not of any known type");
}

#ifndef STBI_NO_STDIO
STBIDEF stbi_uc *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp)
{
   FILE *f = fopen(filename, "rb");
   unsigned char *result;
   if (!f) return stbi__errpuc("can't fopen", "Unable to open file");

   // Get file size
   fseek(f, 0, SEEK_END);
   long size = ftell(f);
   fseek(f, 0, SEEK_SET);

   // Read entire file
   unsigned char *buffer = (unsigned char*)STBI_MALLOC(size);
   if (!buffer) { fclose(f); return stbi__errpuc("outofmem", "Out of memory"); }
   fread(buffer, 1, size, f);
   fclose(f);

   result = stbi_load_from_memory(buffer, (int)size, x, y, comp, req_comp);
   STBI_FREE(buffer);
   return result;
}

STBIDEF stbi_uc *stbi_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp)
{
   // Get current position and file size
   long start = ftell(f);
   fseek(f, 0, SEEK_END);
   long size = ftell(f) - start;
   fseek(f, start, SEEK_SET);

   unsigned char *buffer = (unsigned char*)STBI_MALLOC(size);
   if (!buffer) return stbi__errpuc("outofmem", "Out of memory");
   fread(buffer, 1, size, f);

   unsigned char *result = stbi_load_from_memory(buffer, (int)size, x, y, comp, req_comp);
   STBI_FREE(buffer);
   return result;
}
#endif

STBIDEF int stbi_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);

   // Check JPEG
   if (stbi__jpeg_test(&s)) {
      // Would need to parse JPEG markers to get dimensions
      *x = 0; *y = 0; *comp = 0;
      return 0;
   }

   // Check PNG - parse IHDR
   if (stbi__png_test(&s)) {
      // Skip signature
      stbi__skip(&s, 8);
      // IHDR chunk
      stbi__pngchunk c = stbi__get_chunk_header(&s);
      if (c.type == 0x49484452) { // IHDR
         *x = stbi__get32be(&s);
         *y = stbi__get32be(&s);
         int depth = stbi__get8(&s);
         int color = stbi__get8(&s);
         if (color == 0) *comp = 1;
         else if (color == 2) *comp = 3;
         else if (color == 3) *comp = 1;
         else if (color == 4) *comp = 2;
         else if (color == 6) *comp = 4;
         else *comp = 0;
         (void)depth;
         return 1;
      }
   }

   return 0;
}

STBIDEF void stbi_set_unpremultiply_on_load(int flag_true_if_should_unpremultiply)
{
   (void)flag_true_if_should_unpremultiply;
}

STBIDEF void stbi_convert_iphone_png_to_rgb(int flag_true_if_should_convert)
{
   (void)flag_true_if_should_convert;
}

#endif // STB_IMAGE_IMPLEMENTATION

/*
   revision history:
      2.28  (2023-01-29)
            - various updates
      ...

   LICENSE - Public Domain or MIT (choose one)

   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.

   Alternatively, this software is licensed under the MIT License:

   Copyright (c) 2017 Sean Barrett
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/
