// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_LBMP_FX_BMP_H_
#define CORE_FXCODEC_LBMP_FX_BMP_H_

#include <setjmp.h>

#include "core/fxcodec/codec/ccodec_bmpmodule.h"
#include "core/fxcrt/fx_basic.h"

#define BMP_WIDTHBYTES(width, bitCount) ((width * bitCount) + 31) / 32 * 4
#define BMP_PAL_ENCODE(a, r, g, b) \
  (((uint32_t)(a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define BMP_D_STATUS_HEADER 0x01
#define BMP_D_STATUS_PAL 0x02
#define BMP_D_STATUS_DATA_PRE 0x03
#define BMP_D_STATUS_DATA 0x04
#define BMP_D_STATUS_TAIL 0x00
#define BMP_SIGNATURE 0x4D42
#define BMP_PAL_NEW 0
#define BMP_PAL_OLD 1
#define RLE_MARKER 0
#define RLE_EOL 0
#define RLE_EOI 1
#define RLE_DELTA 2
#define BMP_RGB 0L
#define BMP_RLE8 1L
#define BMP_RLE4 2L
#define BMP_BITFIELDS 3L
#define BMP_BIT_555 0
#define BMP_BIT_565 1
#define BMP_MAX_ERROR_SIZE 256
// Limit width to (MAXINT32 - 31) / 32
#define BMP_MAX_WIDTH 67108863
#pragma pack(1)
typedef struct tagBmpFileHeader {
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;
} BmpFileHeader, *BmpFileHeaderPtr;
typedef struct tagBmpCoreHeader {
  uint32_t bcSize;
  uint16_t bcWidth;
  uint16_t bcHeight;
  uint16_t bcPlanes;
  uint16_t bcBitCount;
} BmpCoreHeader, *BmpCoreHeaderPtr;
typedef struct tagBmpInfoHeader {
  uint32_t biSize;
  int32_t biWidth;
  int32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  int32_t biXPelsPerMeter;
  int32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
} BmpInfoHeader, *BmpInfoHeaderPtr;
#pragma pack()

class BMPDecompressor {
 public:
  void Error(const char* err_msg);
  int32_t DecodeImage();
  int32_t ReadHeader();
  void SetInputBuffer(uint8_t* src_buf, uint32_t src_size);
  uint32_t GetAvailInput(uint8_t** avail_buf);

  jmp_buf jmpbuf;
  char* err_ptr;

  void* context_ptr;

  BmpFileHeaderPtr bmp_header_ptr;
  BmpInfoHeaderPtr bmp_infoheader_ptr;
  int32_t width;
  int32_t height;
  uint32_t compress_flag;
  int32_t components;
  int32_t src_row_bytes;
  int32_t out_row_bytes;
  uint8_t* out_row_buffer;
  uint16_t bitCounts;
  uint32_t color_used;
  bool imgTB_flag;
  int32_t pal_num;
  int32_t pal_type;
  uint32_t* pal_ptr;
  uint32_t data_size;
  uint32_t img_data_offset;
  uint32_t img_ifh_size;
  int32_t row_num;
  int32_t col_num;
  int32_t dpi_x;
  int32_t dpi_y;
  uint32_t mask_red;
  uint32_t mask_green;
  uint32_t mask_blue;

  uint8_t* next_in;
  uint32_t avail_in;
  uint32_t skip_size;
  int32_t decode_status;

 private:
  bool GetDataPosition(uint32_t cur_pos);
  void ReadScanline(int32_t row_num, uint8_t* row_buf);
  int32_t DecodeRGB();
  int32_t DecodeRLE8();
  int32_t DecodeRLE4();
  uint8_t* ReadData(uint8_t** des_buf, uint32_t data_size);
  void SaveDecodingStatus(int32_t status);
  bool ValidateColorIndex(uint8_t val);
  bool ValidateFlag() const;
};

BMPDecompressor* bmp_create_decompress();
void bmp_destroy_decompress(BMPDecompressor** bmp_ptr_ptr);

class CBmpContext : public CCodec_BmpModule::Context {
 public:
  CBmpContext(BMPDecompressor* pBmp,
              CCodec_BmpModule* pModule,
              CCodec_BmpModule::Delegate* pDelegate);
  ~CBmpContext() override;

  BMPDecompressor* m_pBmp;
  CFX_UnownedPtr<CCodec_BmpModule> const m_pModule;
  CFX_UnownedPtr<CCodec_BmpModule::Delegate> const m_pDelegate;
  char m_szLastError[256];
};

uint16_t GetWord_LSBFirst(uint8_t* p);

#endif  // CORE_FXCODEC_LBMP_FX_BMP_H_
