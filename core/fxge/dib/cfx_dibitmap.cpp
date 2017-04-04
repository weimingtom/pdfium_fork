// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/dib/cfx_dibitmap.h"

#include <limits>
#include <memory>
#include <utility>

#include "core/fxcodec/fx_codec.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/dib/cfx_scanlinecompositor.h"
#include "core/fxge/dib/dib_int.h"
#include "core/fxge/ge/cfx_cliprgn.h"
#include "third_party/base/ptr_util.h"

#define MAX_OOM_LIMIT 12000000

CFX_DIBitmap::CFX_DIBitmap() {
  m_bExtBuf = false;
  m_pBuffer = nullptr;
  m_pPalette = nullptr;
#ifdef _SKIA_SUPPORT_PATHS_
  m_nFormat = Format::kCleared;
#endif
}

bool CFX_DIBitmap::Create(int width,
                          int height,
                          FXDIB_Format format,
                          uint8_t* pBuffer,
                          int pitch) {
  m_pBuffer = nullptr;
  m_bpp = static_cast<uint8_t>(format);
  m_AlphaFlag = static_cast<uint8_t>(format >> 8);
  m_Width = m_Height = m_Pitch = 0;
  if (width <= 0 || height <= 0 || pitch < 0)
    return false;

  if ((INT_MAX - 31) / width < (format & 0xff))
    return false;

  if (!pitch)
    pitch = (width * (format & 0xff) + 31) / 32 * 4;

  if ((1 << 30) / pitch < height)
    return false;

  if (pBuffer) {
    m_pBuffer = pBuffer;
    m_bExtBuf = true;
  } else {
    int size = pitch * height + 4;
    int oomlimit = MAX_OOM_LIMIT;
    if (oomlimit >= 0 && size >= oomlimit) {
      m_pBuffer = FX_TryAlloc(uint8_t, size);
      if (!m_pBuffer)
        return false;
    } else {
      m_pBuffer = FX_Alloc(uint8_t, size);
    }
  }
  m_Width = width;
  m_Height = height;
  m_Pitch = pitch;
  if (!HasAlpha() || format == FXDIB_Argb)
    return true;

  if (BuildAlphaMask())
    return true;

  if (m_bExtBuf)
    return true;

  FX_Free(m_pBuffer);
  m_pBuffer = nullptr;
  m_Width = m_Height = m_Pitch = 0;
  return false;
}

bool CFX_DIBitmap::Copy(const CFX_RetainPtr<CFX_DIBSource>& pSrc) {
  if (m_pBuffer)
    return false;

  if (!Create(pSrc->GetWidth(), pSrc->GetHeight(), pSrc->GetFormat()))
    return false;

  SetPalette(pSrc->GetPalette());
  SetAlphaMask(pSrc->m_pAlphaMask);
  for (int row = 0; row < pSrc->GetHeight(); row++)
    memcpy(m_pBuffer + row * m_Pitch, pSrc->GetScanline(row), m_Pitch);
  return true;
}

CFX_DIBitmap::~CFX_DIBitmap() {
  if (!m_bExtBuf)
    FX_Free(m_pBuffer);
}

uint8_t* CFX_DIBitmap::GetBuffer() const {
  return m_pBuffer;
}

const uint8_t* CFX_DIBitmap::GetScanline(int line) const {
  return m_pBuffer ? m_pBuffer + line * m_Pitch : nullptr;
}

void CFX_DIBitmap::TakeOver(CFX_RetainPtr<CFX_DIBitmap>&& pSrcBitmap) {
  if (!m_bExtBuf)
    FX_Free(m_pBuffer);

  m_pBuffer = pSrcBitmap->m_pBuffer;
  m_pPalette = std::move(pSrcBitmap->m_pPalette);
  m_pAlphaMask = pSrcBitmap->m_pAlphaMask;
  pSrcBitmap->m_pBuffer = nullptr;
  pSrcBitmap->m_pAlphaMask = nullptr;
  m_bpp = pSrcBitmap->m_bpp;
  m_bExtBuf = pSrcBitmap->m_bExtBuf;
  m_AlphaFlag = pSrcBitmap->m_AlphaFlag;
  m_Width = pSrcBitmap->m_Width;
  m_Height = pSrcBitmap->m_Height;
  m_Pitch = pSrcBitmap->m_Pitch;
}

void CFX_DIBitmap::Clear(uint32_t color) {
  if (!m_pBuffer) {
    return;
  }
  switch (GetFormat()) {
    case FXDIB_1bppMask:
      memset(m_pBuffer, (color & 0xff000000) ? 0xff : 0, m_Pitch * m_Height);
      break;
    case FXDIB_1bppRgb: {
      int index = FindPalette(color);
      memset(m_pBuffer, index ? 0xff : 0, m_Pitch * m_Height);
      break;
    }
    case FXDIB_8bppMask:
      memset(m_pBuffer, color >> 24, m_Pitch * m_Height);
      break;
    case FXDIB_8bppRgb: {
      int index = FindPalette(color);
      memset(m_pBuffer, index, m_Pitch * m_Height);
      break;
    }
    case FXDIB_Rgb:
    case FXDIB_Rgba: {
      int a, r, g, b;
      ArgbDecode(color, a, r, g, b);
      if (r == g && g == b) {
        memset(m_pBuffer, r, m_Pitch * m_Height);
      } else {
        int byte_pos = 0;
        for (int col = 0; col < m_Width; col++) {
          m_pBuffer[byte_pos++] = b;
          m_pBuffer[byte_pos++] = g;
          m_pBuffer[byte_pos++] = r;
        }
        for (int row = 1; row < m_Height; row++) {
          memcpy(m_pBuffer + row * m_Pitch, m_pBuffer, m_Pitch);
        }
      }
      break;
    }
    case FXDIB_Rgb32:
    case FXDIB_Argb: {
      color = IsCmykImage() ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
#ifdef _SKIA_SUPPORT_
      if (FXDIB_Rgb32 == GetFormat() && !IsCmykImage()) {
        color |= 0xFF000000;
      }
#endif
      for (int i = 0; i < m_Width; i++) {
        ((uint32_t*)m_pBuffer)[i] = color;
      }
      for (int row = 1; row < m_Height; row++) {
        memcpy(m_pBuffer + row * m_Pitch, m_pBuffer, m_Pitch);
      }
      break;
    }
    default:
      break;
  }
}

bool CFX_DIBitmap::TransferBitmap(
    int dest_left,
    int dest_top,
    int width,
    int height,
    const CFX_RetainPtr<CFX_DIBSource>& pSrcBitmap,
    int src_left,
    int src_top) {
  if (!m_pBuffer)
    return false;

  GetOverlapRect(dest_left, dest_top, width, height, pSrcBitmap->GetWidth(),
                 pSrcBitmap->GetHeight(), src_left, src_top, nullptr);
  if (width == 0 || height == 0)
    return true;

  FXDIB_Format dest_format = GetFormat();
  FXDIB_Format src_format = pSrcBitmap->GetFormat();
  if (dest_format == src_format) {
    if (GetBPP() == 1) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = m_pBuffer + (dest_top + row) * m_Pitch;
        const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row);
        for (int col = 0; col < width; col++) {
          if (src_scan[(src_left + col) / 8] &
              (1 << (7 - (src_left + col) % 8))) {
            dest_scan[(dest_left + col) / 8] |= 1
                                                << (7 - (dest_left + col) % 8);
          } else {
            dest_scan[(dest_left + col) / 8] &=
                ~(1 << (7 - (dest_left + col) % 8));
          }
        }
      }
    } else {
      int Bpp = GetBPP() / 8;
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan =
            m_pBuffer + (dest_top + row) * m_Pitch + dest_left * Bpp;
        const uint8_t* src_scan =
            pSrcBitmap->GetScanline(src_top + row) + src_left * Bpp;
        memcpy(dest_scan, src_scan, width * Bpp);
      }
    }
  } else {
    if (m_pPalette)
      return false;

    if (m_bpp == 8)
      dest_format = FXDIB_8bppMask;

    uint8_t* dest_buf =
        m_pBuffer + dest_top * m_Pitch + dest_left * GetBPP() / 8;
    std::unique_ptr<uint32_t, FxFreeDeleter> d_plt;
    if (!ConvertBuffer(dest_format, dest_buf, m_Pitch, width, height,
                       pSrcBitmap, src_left, src_top, &d_plt)) {
      return false;
    }
  }
  return true;
}

bool CFX_DIBitmap::TransferMask(int dest_left,
                                int dest_top,
                                int width,
                                int height,
                                const CFX_RetainPtr<CFX_DIBSource>& pMask,
                                uint32_t color,
                                int src_left,
                                int src_top,
                                int alpha_flag,
                                void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  ASSERT(HasAlpha() && (m_bpp >= 24));
  ASSERT(pMask->IsAlphaMask());
  if (!HasAlpha() || !pMask->IsAlphaMask() || m_bpp < 24) {
    return false;
  }
  GetOverlapRect(dest_left, dest_top, width, height, pMask->GetWidth(),
                 pMask->GetHeight(), src_left, src_top, nullptr);
  if (width == 0 || height == 0) {
    return true;
  }
  int src_bpp = pMask->GetBPP();
  int alpha;
  uint32_t dst_color;
  if (alpha_flag >> 8) {
    alpha = alpha_flag & 0xff;
    dst_color = FXCMYK_TODIB(color);
  } else {
    alpha = FXARGB_A(color);
    dst_color = FXARGB_TODIB(color);
  }
  uint8_t* color_p = (uint8_t*)&dst_color;
  if (pIccTransform && CFX_GEModule::Get()->GetCodecModule() &&
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule()) {
    CCodec_IccModule* pIccModule =
        CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
    pIccModule->TranslateScanline(pIccTransform, color_p, color_p, 1);
  } else {
    if (alpha_flag >> 8 && !IsCmykImage()) {
      AdobeCMYK_to_sRGB1(FXSYS_GetCValue(color), FXSYS_GetMValue(color),
                         FXSYS_GetYValue(color), FXSYS_GetKValue(color),
                         color_p[2], color_p[1], color_p[0]);
    } else if (!(alpha_flag >> 8) && IsCmykImage()) {
      return false;
    }
  }
  if (!IsCmykImage()) {
    color_p[3] = (uint8_t)alpha;
  }
  if (GetFormat() == FXDIB_Argb) {
    for (int row = 0; row < height; row++) {
      uint32_t* dest_pos =
          (uint32_t*)(m_pBuffer + (dest_top + row) * m_Pitch + dest_left * 4);
      const uint8_t* src_scan = pMask->GetScanline(src_top + row);
      if (src_bpp == 1) {
        for (int col = 0; col < width; col++) {
          int src_bitpos = src_left + col;
          if (src_scan[src_bitpos / 8] & (1 << (7 - src_bitpos % 8))) {
            *dest_pos = dst_color;
          } else {
            *dest_pos = 0;
          }
          dest_pos++;
        }
      } else {
        src_scan += src_left;
        dst_color = FXARGB_TODIB(dst_color);
        dst_color &= 0xffffff;
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_pos++,
                        dst_color | ((alpha * (*src_scan++) / 255) << 24));
        }
      }
    }
  } else {
    int comps = m_bpp / 8;
    for (int row = 0; row < height; row++) {
      uint8_t* dest_color_pos =
          m_pBuffer + (dest_top + row) * m_Pitch + dest_left * comps;
      uint8_t* dest_alpha_pos =
          (uint8_t*)m_pAlphaMask->GetScanline(dest_top + row) + dest_left;
      const uint8_t* src_scan = pMask->GetScanline(src_top + row);
      if (src_bpp == 1) {
        for (int col = 0; col < width; col++) {
          int src_bitpos = src_left + col;
          if (src_scan[src_bitpos / 8] & (1 << (7 - src_bitpos % 8))) {
            memcpy(dest_color_pos, color_p, comps);
            *dest_alpha_pos = 0xff;
          } else {
            memset(dest_color_pos, 0, comps);
            *dest_alpha_pos = 0;
          }
          dest_color_pos += comps;
          dest_alpha_pos++;
        }
      } else {
        src_scan += src_left;
        for (int col = 0; col < width; col++) {
          memcpy(dest_color_pos, color_p, comps);
          dest_color_pos += comps;
          *dest_alpha_pos++ = (alpha * (*src_scan++) / 255);
        }
      }
    }
  }
  return true;
}

const int g_ChannelOffset[] = {0, 2, 1, 0, 0, 1, 2, 3, 3};
bool CFX_DIBitmap::LoadChannel(FXDIB_Channel destChannel,
                               const CFX_RetainPtr<CFX_DIBSource>& pSrcBitmap,
                               FXDIB_Channel srcChannel) {
  if (!m_pBuffer)
    return false;

  CFX_RetainPtr<CFX_DIBSource> pSrcClone = pSrcBitmap;
  int srcOffset;
  if (srcChannel == FXDIB_Alpha) {
    if (!pSrcBitmap->HasAlpha() && !pSrcBitmap->IsAlphaMask())
      return false;

    if (pSrcBitmap->GetBPP() == 1) {
      pSrcClone = pSrcBitmap->CloneConvert(FXDIB_8bppMask);
      if (!pSrcClone)
        return false;
    }
    srcOffset = pSrcBitmap->GetFormat() == FXDIB_Argb ? 3 : 0;
  } else {
    if (pSrcBitmap->IsAlphaMask())
      return false;

    if (pSrcBitmap->GetBPP() < 24) {
      if (pSrcBitmap->IsCmykImage()) {
        pSrcClone = pSrcBitmap->CloneConvert(static_cast<FXDIB_Format>(
            (pSrcBitmap->GetFormat() & 0xff00) | 0x20));
      } else {
        pSrcClone = pSrcBitmap->CloneConvert(static_cast<FXDIB_Format>(
            (pSrcBitmap->GetFormat() & 0xff00) | 0x18));
      }
      if (!pSrcClone)
        return false;
    }
    srcOffset = g_ChannelOffset[srcChannel];
  }
  int destOffset = 0;
  if (destChannel == FXDIB_Alpha) {
    if (IsAlphaMask()) {
      if (!ConvertFormat(FXDIB_8bppMask))
        return false;
    } else {
      if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb))
        return false;

      if (GetFormat() == FXDIB_Argb)
        destOffset = 3;
    }
  } else {
    if (IsAlphaMask())
      return false;

    if (GetBPP() < 24) {
      if (HasAlpha()) {
        if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb))
          return false;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb32)) {
#else
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb)) {
#endif
        return false;
      }
    }
    destOffset = g_ChannelOffset[destChannel];
  }
  if (srcChannel == FXDIB_Alpha && pSrcClone->m_pAlphaMask) {
    CFX_RetainPtr<CFX_DIBSource> pAlphaMask = pSrcClone->m_pAlphaMask;
    if (pSrcClone->GetWidth() != m_Width ||
        pSrcClone->GetHeight() != m_Height) {
      if (pAlphaMask) {
        pAlphaMask = pAlphaMask->StretchTo(m_Width, m_Height);
        if (!pAlphaMask)
          return false;
      }
    }
    pSrcClone = std::move(pAlphaMask);
    srcOffset = 0;
  } else if (pSrcClone->GetWidth() != m_Width ||
             pSrcClone->GetHeight() != m_Height) {
    CFX_RetainPtr<CFX_DIBitmap> pSrcMatched =
        pSrcClone->StretchTo(m_Width, m_Height);
    if (!pSrcMatched)
      return false;

    pSrcClone = std::move(pSrcMatched);
  }
  CFX_RetainPtr<CFX_DIBitmap> pDst(this);
  if (destChannel == FXDIB_Alpha && m_pAlphaMask) {
    pDst = m_pAlphaMask;
    destOffset = 0;
  }
  int srcBytes = pSrcClone->GetBPP() / 8;
  int destBytes = pDst->GetBPP() / 8;
  for (int row = 0; row < m_Height; row++) {
    uint8_t* dest_pos = (uint8_t*)pDst->GetScanline(row) + destOffset;
    const uint8_t* src_pos = pSrcClone->GetScanline(row) + srcOffset;
    for (int col = 0; col < m_Width; col++) {
      *dest_pos = *src_pos;
      dest_pos += destBytes;
      src_pos += srcBytes;
    }
  }
  return true;
}

bool CFX_DIBitmap::LoadChannel(FXDIB_Channel destChannel, int value) {
  if (!m_pBuffer) {
    return false;
  }
  int destOffset;
  if (destChannel == FXDIB_Alpha) {
    if (IsAlphaMask()) {
      if (!ConvertFormat(FXDIB_8bppMask)) {
        return false;
      }
      destOffset = 0;
    } else {
      destOffset = 0;
      if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb)) {
        return false;
      }
      if (GetFormat() == FXDIB_Argb) {
        destOffset = 3;
      }
    }
  } else {
    if (IsAlphaMask()) {
      return false;
    }
    if (GetBPP() < 24) {
      if (HasAlpha()) {
        if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb)) {
          return false;
        }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb)) {
#else
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb32)) {
#endif
        return false;
      }
    }
    destOffset = g_ChannelOffset[destChannel];
  }
  int Bpp = GetBPP() / 8;
  if (Bpp == 1) {
    memset(m_pBuffer, value, m_Height * m_Pitch);
    return true;
  }
  if (destChannel == FXDIB_Alpha && m_pAlphaMask) {
    memset(m_pAlphaMask->GetBuffer(), value,
           m_pAlphaMask->GetHeight() * m_pAlphaMask->GetPitch());
    return true;
  }
  for (int row = 0; row < m_Height; row++) {
    uint8_t* scan_line = m_pBuffer + row * m_Pitch + destOffset;
    for (int col = 0; col < m_Width; col++) {
      *scan_line = value;
      scan_line += Bpp;
    }
  }
  return true;
}

bool CFX_DIBitmap::MultiplyAlpha(
    const CFX_RetainPtr<CFX_DIBSource>& pSrcBitmap) {
  if (!m_pBuffer)
    return false;

  ASSERT(pSrcBitmap->IsAlphaMask());
  if (!pSrcBitmap->IsAlphaMask())
    return false;

  if (!IsAlphaMask() && !HasAlpha())
    return LoadChannel(FXDIB_Alpha, pSrcBitmap, FXDIB_Alpha);

  CFX_RetainPtr<CFX_DIBitmap> pSrcClone = pSrcBitmap.As<CFX_DIBitmap>();
  if (pSrcBitmap->GetWidth() != m_Width ||
      pSrcBitmap->GetHeight() != m_Height) {
    pSrcClone = pSrcBitmap->StretchTo(m_Width, m_Height);
    if (!pSrcClone)
      return false;
  }
  if (IsAlphaMask()) {
    if (!ConvertFormat(FXDIB_8bppMask))
      return false;

    for (int row = 0; row < m_Height; row++) {
      uint8_t* dest_scan = m_pBuffer + m_Pitch * row;
      uint8_t* src_scan = pSrcClone->m_pBuffer + pSrcClone->m_Pitch * row;
      if (pSrcClone->GetBPP() == 1) {
        for (int col = 0; col < m_Width; col++) {
          if (!((1 << (7 - col % 8)) & src_scan[col / 8]))
            dest_scan[col] = 0;
        }
      } else {
        for (int col = 0; col < m_Width; col++) {
          *dest_scan = (*dest_scan) * src_scan[col] / 255;
          dest_scan++;
        }
      }
    }
  } else {
    if (GetFormat() == FXDIB_Argb) {
      if (pSrcClone->GetBPP() == 1)
        return false;

      for (int row = 0; row < m_Height; row++) {
        uint8_t* dest_scan = m_pBuffer + m_Pitch * row + 3;
        uint8_t* src_scan = pSrcClone->m_pBuffer + pSrcClone->m_Pitch * row;
        for (int col = 0; col < m_Width; col++) {
          *dest_scan = (*dest_scan) * src_scan[col] / 255;
          dest_scan += 4;
        }
      }
    } else {
      m_pAlphaMask->MultiplyAlpha(pSrcClone);
    }
  }
  return true;
}

bool CFX_DIBitmap::GetGrayData(void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  switch (GetFormat()) {
    case FXDIB_1bppRgb: {
      if (!m_pPalette)
        return false;

      uint8_t gray[2];
      for (int i = 0; i < 2; i++) {
        int r = static_cast<uint8_t>(m_pPalette.get()[i] >> 16);
        int g = static_cast<uint8_t>(m_pPalette.get()[i] >> 8);
        int b = static_cast<uint8_t>(m_pPalette.get()[i]);
        gray[i] = static_cast<uint8_t>(FXRGB2GRAY(r, g, b));
      }
      auto pMask = pdfium::MakeRetain<CFX_DIBitmap>();
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask))
        return false;

      memset(pMask->GetBuffer(), gray[0], pMask->GetPitch() * m_Height);
      for (int row = 0; row < m_Height; row++) {
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        uint8_t* dest_pos = (uint8_t*)pMask->GetScanline(row);
        for (int col = 0; col < m_Width; col++) {
          if (src_pos[col / 8] & (1 << (7 - col % 8))) {
            *dest_pos = gray[1];
          }
          dest_pos++;
        }
      }
      TakeOver(std::move(pMask));
      break;
    }
    case FXDIB_8bppRgb: {
      if (!m_pPalette)
        return false;

      uint8_t gray[256];
      for (int i = 0; i < 256; i++) {
        int r = static_cast<uint8_t>(m_pPalette.get()[i] >> 16);
        int g = static_cast<uint8_t>(m_pPalette.get()[i] >> 8);
        int b = static_cast<uint8_t>(m_pPalette.get()[i]);
        gray[i] = static_cast<uint8_t>(FXRGB2GRAY(r, g, b));
      }
      auto pMask = pdfium::MakeRetain<CFX_DIBitmap>();
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask))
        return false;

      for (int row = 0; row < m_Height; row++) {
        uint8_t* dest_pos = pMask->GetBuffer() + row * pMask->GetPitch();
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        for (int col = 0; col < m_Width; col++) {
          *dest_pos++ = gray[*src_pos++];
        }
      }
      TakeOver(std::move(pMask));
      break;
    }
    case FXDIB_Rgb: {
      auto pMask = pdfium::MakeRetain<CFX_DIBitmap>();
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask))
        return false;

      for (int row = 0; row < m_Height; row++) {
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        uint8_t* dest_pos = pMask->GetBuffer() + row * pMask->GetPitch();
        for (int col = 0; col < m_Width; col++) {
          *dest_pos++ = FXRGB2GRAY(src_pos[2], src_pos[1], *src_pos);
          src_pos += 3;
        }
      }
      TakeOver(std::move(pMask));
      break;
    }
    case FXDIB_Rgb32: {
      auto pMask = pdfium::MakeRetain<CFX_DIBitmap>();
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask))
        return false;

      for (int row = 0; row < m_Height; row++) {
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        uint8_t* dest_pos = pMask->GetBuffer() + row * pMask->GetPitch();
        for (int col = 0; col < m_Width; col++) {
          *dest_pos++ = FXRGB2GRAY(src_pos[2], src_pos[1], *src_pos);
          src_pos += 4;
        }
      }
      TakeOver(std::move(pMask));
      break;
    }
    default:
      return false;
  }
  return true;
}

bool CFX_DIBitmap::MultiplyAlpha(int alpha) {
  if (!m_pBuffer) {
    return false;
  }
  switch (GetFormat()) {
    case FXDIB_1bppMask:
      if (!ConvertFormat(FXDIB_8bppMask)) {
        return false;
      }
      MultiplyAlpha(alpha);
      break;
    case FXDIB_8bppMask: {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* scan_line = m_pBuffer + row * m_Pitch;
        for (int col = 0; col < m_Width; col++) {
          scan_line[col] = scan_line[col] * alpha / 255;
        }
      }
      break;
    }
    case FXDIB_Argb: {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* scan_line = m_pBuffer + row * m_Pitch + 3;
        for (int col = 0; col < m_Width; col++) {
          *scan_line = (*scan_line) * alpha / 255;
          scan_line += 4;
        }
      }
      break;
    }
    default:
      if (HasAlpha()) {
        m_pAlphaMask->MultiplyAlpha(alpha);
      } else if (IsCmykImage()) {
        if (!ConvertFormat((FXDIB_Format)(GetFormat() | 0x0200))) {
          return false;
        }
        m_pAlphaMask->MultiplyAlpha(alpha);
      } else {
        if (!ConvertFormat(FXDIB_Argb)) {
          return false;
        }
        MultiplyAlpha(alpha);
      }
      break;
  }
  return true;
}

uint32_t CFX_DIBitmap::GetPixel(int x, int y) const {
  if (!m_pBuffer) {
    return 0;
  }
  uint8_t* pos = m_pBuffer + y * m_Pitch + x * GetBPP() / 8;
  switch (GetFormat()) {
    case FXDIB_1bppMask: {
      if ((*pos) & (1 << (7 - x % 8))) {
        return 0xff000000;
      }
      return 0;
    }
    case FXDIB_1bppRgb: {
      if ((*pos) & (1 << (7 - x % 8))) {
        return m_pPalette ? m_pPalette.get()[1] : 0xffffffff;
      }
      return m_pPalette ? m_pPalette.get()[0] : 0xff000000;
    }
    case FXDIB_8bppMask:
      return (*pos) << 24;
    case FXDIB_8bppRgb:
      return m_pPalette ? m_pPalette.get()[*pos]
                        : (0xff000000 | ((*pos) * 0x10101));
    case FXDIB_Rgb:
    case FXDIB_Rgba:
    case FXDIB_Rgb32:
      return FXARGB_GETDIB(pos) | 0xff000000;
    case FXDIB_Argb:
      return FXARGB_GETDIB(pos);
    default:
      break;
  }
  return 0;
}

void CFX_DIBitmap::SetPixel(int x, int y, uint32_t color) {
  if (!m_pBuffer) {
    return;
  }
  if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) {
    return;
  }
  uint8_t* pos = m_pBuffer + y * m_Pitch + x * GetBPP() / 8;
  switch (GetFormat()) {
    case FXDIB_1bppMask:
      if (color >> 24) {
        *pos |= 1 << (7 - x % 8);
      } else {
        *pos &= ~(1 << (7 - x % 8));
      }
      break;
    case FXDIB_1bppRgb:
      if (m_pPalette) {
        if (color == m_pPalette.get()[1]) {
          *pos |= 1 << (7 - x % 8);
        } else {
          *pos &= ~(1 << (7 - x % 8));
        }
      } else {
        if (color == 0xffffffff) {
          *pos |= 1 << (7 - x % 8);
        } else {
          *pos &= ~(1 << (7 - x % 8));
        }
      }
      break;
    case FXDIB_8bppMask:
      *pos = (uint8_t)(color >> 24);
      break;
    case FXDIB_8bppRgb: {
      if (m_pPalette) {
        for (int i = 0; i < 256; i++) {
          if (m_pPalette.get()[i] == color) {
            *pos = (uint8_t)i;
            return;
          }
        }
        *pos = 0;
      } else {
        *pos = FXRGB2GRAY(FXARGB_R(color), FXARGB_G(color), FXARGB_B(color));
      }
      break;
    }
    case FXDIB_Rgb:
    case FXDIB_Rgb32: {
      int alpha = FXARGB_A(color);
      pos[0] = (FXARGB_B(color) * alpha + pos[0] * (255 - alpha)) / 255;
      pos[1] = (FXARGB_G(color) * alpha + pos[1] * (255 - alpha)) / 255;
      pos[2] = (FXARGB_R(color) * alpha + pos[2] * (255 - alpha)) / 255;
      break;
    }
    case FXDIB_Rgba: {
      pos[0] = FXARGB_B(color);
      pos[1] = FXARGB_G(color);
      pos[2] = FXARGB_R(color);
      break;
    }
    case FXDIB_Argb:
      FXARGB_SETDIB(pos, color);
      break;
    default:
      break;
  }
}

void CFX_DIBitmap::DownSampleScanline(int line,
                                      uint8_t* dest_scan,
                                      int dest_bpp,
                                      int dest_width,
                                      bool bFlipX,
                                      int clip_left,
                                      int clip_width) const {
  if (!m_pBuffer) {
    return;
  }
  int src_Bpp = m_bpp / 8;
  uint8_t* scanline = m_pBuffer + line * m_Pitch;
  if (src_Bpp == 0) {
    for (int i = 0; i < clip_width; i++) {
      uint32_t dest_x = clip_left + i;
      uint32_t src_x = dest_x * m_Width / dest_width;
      if (bFlipX) {
        src_x = m_Width - src_x - 1;
      }
      src_x %= m_Width;
      dest_scan[i] = (scanline[src_x / 8] & (1 << (7 - src_x % 8))) ? 255 : 0;
    }
  } else if (src_Bpp == 1) {
    for (int i = 0; i < clip_width; i++) {
      uint32_t dest_x = clip_left + i;
      uint32_t src_x = dest_x * m_Width / dest_width;
      if (bFlipX) {
        src_x = m_Width - src_x - 1;
      }
      src_x %= m_Width;
      int dest_pos = i;
      if (m_pPalette) {
        if (!IsCmykImage()) {
          dest_pos *= 3;
          FX_ARGB argb = m_pPalette.get()[scanline[src_x]];
          dest_scan[dest_pos] = FXARGB_B(argb);
          dest_scan[dest_pos + 1] = FXARGB_G(argb);
          dest_scan[dest_pos + 2] = FXARGB_R(argb);
        } else {
          dest_pos *= 4;
          FX_CMYK cmyk = m_pPalette.get()[scanline[src_x]];
          dest_scan[dest_pos] = FXSYS_GetCValue(cmyk);
          dest_scan[dest_pos + 1] = FXSYS_GetMValue(cmyk);
          dest_scan[dest_pos + 2] = FXSYS_GetYValue(cmyk);
          dest_scan[dest_pos + 3] = FXSYS_GetKValue(cmyk);
        }
      } else {
        dest_scan[dest_pos] = scanline[src_x];
      }
    }
  } else {
    for (int i = 0; i < clip_width; i++) {
      uint32_t dest_x = clip_left + i;
      uint32_t src_x =
          bFlipX ? (m_Width - dest_x * m_Width / dest_width - 1) * src_Bpp
                 : (dest_x * m_Width / dest_width) * src_Bpp;
      src_x %= m_Width * src_Bpp;
      int dest_pos = i * src_Bpp;
      for (int b = 0; b < src_Bpp; b++) {
        dest_scan[dest_pos + b] = scanline[src_x + b];
      }
    }
  }
}

// TODO(weili): Split this function into two for handling CMYK and RGB
// colors separately.
bool CFX_DIBitmap::ConvertColorScale(uint32_t forecolor, uint32_t backcolor) {
  ASSERT(!IsAlphaMask());
  if (!m_pBuffer || IsAlphaMask()) {
    return false;
  }
  // Values used for CMYK colors.
  int fc = 0;
  int fm = 0;
  int fy = 0;
  int fk = 0;
  int bc = 0;
  int bm = 0;
  int by = 0;
  int bk = 0;
  // Values used for RGB colors.
  int fr = 0;
  int fg = 0;
  int fb = 0;
  int br = 0;
  int bg = 0;
  int bb = 0;
  bool isCmykImage = IsCmykImage();
  if (isCmykImage) {
    fc = FXSYS_GetCValue(forecolor);
    fm = FXSYS_GetMValue(forecolor);
    fy = FXSYS_GetYValue(forecolor);
    fk = FXSYS_GetKValue(forecolor);
    bc = FXSYS_GetCValue(backcolor);
    bm = FXSYS_GetMValue(backcolor);
    by = FXSYS_GetYValue(backcolor);
    bk = FXSYS_GetKValue(backcolor);
  } else {
    fr = FXSYS_GetRValue(forecolor);
    fg = FXSYS_GetGValue(forecolor);
    fb = FXSYS_GetBValue(forecolor);
    br = FXSYS_GetRValue(backcolor);
    bg = FXSYS_GetGValue(backcolor);
    bb = FXSYS_GetBValue(backcolor);
  }
  if (m_bpp <= 8) {
    if (isCmykImage) {
      if (forecolor == 0xff && backcolor == 0 && !m_pPalette) {
        return true;
      }
    } else if (forecolor == 0 && backcolor == 0xffffff && !m_pPalette) {
      return true;
    }
    if (!m_pPalette) {
      BuildPalette();
    }
    int size = 1 << m_bpp;
    if (isCmykImage) {
      for (int i = 0; i < size; i++) {
        uint8_t b, g, r;
        AdobeCMYK_to_sRGB1(FXSYS_GetCValue(m_pPalette.get()[i]),
                           FXSYS_GetMValue(m_pPalette.get()[i]),
                           FXSYS_GetYValue(m_pPalette.get()[i]),
                           FXSYS_GetKValue(m_pPalette.get()[i]), r, g, b);
        int gray = 255 - FXRGB2GRAY(r, g, b);
        m_pPalette.get()[i] = CmykEncode(
            bc + (fc - bc) * gray / 255, bm + (fm - bm) * gray / 255,
            by + (fy - by) * gray / 255, bk + (fk - bk) * gray / 255);
      }
    } else {
      for (int i = 0; i < size; i++) {
        int gray = FXRGB2GRAY(FXARGB_R(m_pPalette.get()[i]),
                              FXARGB_G(m_pPalette.get()[i]),
                              FXARGB_B(m_pPalette.get()[i]));
        m_pPalette.get()[i] = FXARGB_MAKE(0xff, br + (fr - br) * gray / 255,
                                          bg + (fg - bg) * gray / 255,
                                          bb + (fb - bb) * gray / 255);
      }
    }
    return true;
  }
  if (isCmykImage) {
    if (forecolor == 0xff && backcolor == 0x00) {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* scanline = m_pBuffer + row * m_Pitch;
        for (int col = 0; col < m_Width; col++) {
          uint8_t b, g, r;
          AdobeCMYK_to_sRGB1(scanline[0], scanline[1], scanline[2], scanline[3],
                             r, g, b);
          *scanline++ = 0;
          *scanline++ = 0;
          *scanline++ = 0;
          *scanline++ = 255 - FXRGB2GRAY(r, g, b);
        }
      }
      return true;
    }
  } else if (forecolor == 0 && backcolor == 0xffffff) {
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch;
      int gap = m_bpp / 8 - 2;
      for (int col = 0; col < m_Width; col++) {
        int gray = FXRGB2GRAY(scanline[2], scanline[1], scanline[0]);
        *scanline++ = gray;
        *scanline++ = gray;
        *scanline = gray;
        scanline += gap;
      }
    }
    return true;
  }
  if (isCmykImage) {
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch;
      for (int col = 0; col < m_Width; col++) {
        uint8_t b, g, r;
        AdobeCMYK_to_sRGB1(scanline[0], scanline[1], scanline[2], scanline[3],
                           r, g, b);
        int gray = 255 - FXRGB2GRAY(r, g, b);
        *scanline++ = bc + (fc - bc) * gray / 255;
        *scanline++ = bm + (fm - bm) * gray / 255;
        *scanline++ = by + (fy - by) * gray / 255;
        *scanline++ = bk + (fk - bk) * gray / 255;
      }
    }
  } else {
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch;
      int gap = m_bpp / 8 - 2;
      for (int col = 0; col < m_Width; col++) {
        int gray = FXRGB2GRAY(scanline[2], scanline[1], scanline[0]);
        *scanline++ = bb + (fb - bb) * gray / 255;
        *scanline++ = bg + (fg - bg) * gray / 255;
        *scanline = br + (fr - br) * gray / 255;
        scanline += gap;
      }
    }
  }
  return true;
}

bool CFX_DIBitmap::CompositeBitmap(
    int dest_left,
    int dest_top,
    int width,
    int height,
    const CFX_RetainPtr<CFX_DIBSource>& pSrcBitmap,
    int src_left,
    int src_top,
    int blend_type,
    const CFX_ClipRgn* pClipRgn,
    bool bRgbByteOrder,
    void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  ASSERT(!pSrcBitmap->IsAlphaMask());
  ASSERT(m_bpp >= 8);
  if (pSrcBitmap->IsAlphaMask() || m_bpp < 8) {
    return false;
  }
  GetOverlapRect(dest_left, dest_top, width, height, pSrcBitmap->GetWidth(),
                 pSrcBitmap->GetHeight(), src_left, src_top, pClipRgn);
  if (width == 0 || height == 0) {
    return true;
  }
  CFX_RetainPtr<CFX_DIBitmap> pClipMask;
  FX_RECT clip_box;
  if (pClipRgn && pClipRgn->GetType() != CFX_ClipRgn::RectI) {
    ASSERT(pClipRgn->GetType() == CFX_ClipRgn::MaskF);
    pClipMask = pClipRgn->GetMask();
    clip_box = pClipRgn->GetBox();
  }
  CFX_ScanlineCompositor compositor;
  if (!compositor.Init(GetFormat(), pSrcBitmap->GetFormat(), width,
                       pSrcBitmap->GetPalette(), 0, blend_type,
                       pClipMask != nullptr, bRgbByteOrder, 0, pIccTransform)) {
    return false;
  }
  int dest_Bpp = m_bpp / 8;
  int src_Bpp = pSrcBitmap->GetBPP() / 8;
  bool bRgb = src_Bpp > 1 && !pSrcBitmap->IsCmykImage();
  CFX_RetainPtr<CFX_DIBitmap> pSrcAlphaMask = pSrcBitmap->m_pAlphaMask;
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan =
        m_pBuffer + (dest_top + row) * m_Pitch + dest_left * dest_Bpp;
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row) + src_left * src_Bpp;
    const uint8_t* src_scan_extra_alpha =
        pSrcAlphaMask ? pSrcAlphaMask->GetScanline(src_top + row) + src_left
                      : nullptr;
    uint8_t* dst_scan_extra_alpha =
        m_pAlphaMask
            ? (uint8_t*)m_pAlphaMask->GetScanline(dest_top + row) + dest_left
            : nullptr;
    const uint8_t* clip_scan = nullptr;
    if (pClipMask) {
      clip_scan = pClipMask->m_pBuffer +
                  (dest_top + row - clip_box.top) * pClipMask->m_Pitch +
                  (dest_left - clip_box.left);
    }
    if (bRgb) {
      compositor.CompositeRgbBitmapLine(dest_scan, src_scan, width, clip_scan,
                                        src_scan_extra_alpha,
                                        dst_scan_extra_alpha);
    } else {
      compositor.CompositePalBitmapLine(dest_scan, src_scan, src_left, width,
                                        clip_scan, src_scan_extra_alpha,
                                        dst_scan_extra_alpha);
    }
  }
  return true;
}

bool CFX_DIBitmap::CompositeMask(int dest_left,
                                 int dest_top,
                                 int width,
                                 int height,
                                 const CFX_RetainPtr<CFX_DIBSource>& pMask,
                                 uint32_t color,
                                 int src_left,
                                 int src_top,
                                 int blend_type,
                                 const CFX_ClipRgn* pClipRgn,
                                 bool bRgbByteOrder,
                                 int alpha_flag,
                                 void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  ASSERT(pMask->IsAlphaMask());
  ASSERT(m_bpp >= 8);
  if (!pMask->IsAlphaMask() || m_bpp < 8) {
    return false;
  }
  GetOverlapRect(dest_left, dest_top, width, height, pMask->GetWidth(),
                 pMask->GetHeight(), src_left, src_top, pClipRgn);
  if (width == 0 || height == 0) {
    return true;
  }
  int src_alpha =
      (uint8_t)(alpha_flag >> 8) ? (alpha_flag & 0xff) : FXARGB_A(color);
  if (src_alpha == 0) {
    return true;
  }
  CFX_RetainPtr<CFX_DIBitmap> pClipMask;
  FX_RECT clip_box;
  if (pClipRgn && pClipRgn->GetType() != CFX_ClipRgn::RectI) {
    ASSERT(pClipRgn->GetType() == CFX_ClipRgn::MaskF);
    pClipMask = pClipRgn->GetMask();
    clip_box = pClipRgn->GetBox();
  }
  int src_bpp = pMask->GetBPP();
  int Bpp = GetBPP() / 8;
  CFX_ScanlineCompositor compositor;
  if (!compositor.Init(GetFormat(), pMask->GetFormat(), width, nullptr, color,
                       blend_type, pClipMask != nullptr, bRgbByteOrder,
                       alpha_flag, pIccTransform)) {
    return false;
  }
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan =
        m_pBuffer + (dest_top + row) * m_Pitch + dest_left * Bpp;
    const uint8_t* src_scan = pMask->GetScanline(src_top + row);
    uint8_t* dst_scan_extra_alpha =
        m_pAlphaMask
            ? (uint8_t*)m_pAlphaMask->GetScanline(dest_top + row) + dest_left
            : nullptr;
    const uint8_t* clip_scan = nullptr;
    if (pClipMask) {
      clip_scan = pClipMask->m_pBuffer +
                  (dest_top + row - clip_box.top) * pClipMask->m_Pitch +
                  (dest_left - clip_box.left);
    }
    if (src_bpp == 1) {
      compositor.CompositeBitMaskLine(dest_scan, src_scan, src_left, width,
                                      clip_scan, dst_scan_extra_alpha);
    } else {
      compositor.CompositeByteMaskLine(dest_scan, src_scan + src_left, width,
                                       clip_scan, dst_scan_extra_alpha);
    }
  }
  return true;
}

bool CFX_DIBitmap::CompositeRect(int left,
                                 int top,
                                 int width,
                                 int height,
                                 uint32_t color,
                                 int alpha_flag,
                                 void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  int src_alpha = (alpha_flag >> 8) ? (alpha_flag & 0xff) : FXARGB_A(color);
  if (src_alpha == 0) {
    return true;
  }
  FX_RECT rect(left, top, left + width, top + height);
  rect.Intersect(0, 0, m_Width, m_Height);
  if (rect.IsEmpty()) {
    return true;
  }
  width = rect.Width();
  uint32_t dst_color;
  if (alpha_flag >> 8) {
    dst_color = FXCMYK_TODIB(color);
  } else {
    dst_color = FXARGB_TODIB(color);
  }
  uint8_t* color_p = (uint8_t*)&dst_color;
  if (m_bpp == 8) {
    uint8_t gray = 255;
    if (!IsAlphaMask()) {
      if (pIccTransform && CFX_GEModule::Get()->GetCodecModule() &&
          CFX_GEModule::Get()->GetCodecModule()->GetIccModule()) {
        CCodec_IccModule* pIccModule =
            CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
        pIccModule->TranslateScanline(pIccTransform, &gray, color_p, 1);
      } else {
        if (alpha_flag >> 8) {
          uint8_t r, g, b;
          AdobeCMYK_to_sRGB1(color_p[0], color_p[1], color_p[2], color_p[3], r,
                             g, b);
          gray = FXRGB2GRAY(r, g, b);
        } else {
          gray = (uint8_t)FXRGB2GRAY((int)color_p[2], color_p[1], color_p[0]);
        }
      }
      if (IsCmykImage()) {
        gray = ~gray;
      }
    }
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan = m_pBuffer + row * m_Pitch + rect.left;
      if (src_alpha == 255) {
        memset(dest_scan, gray, width);
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
          dest_scan++;
        }
      }
    }
    return true;
  }
  if (m_bpp == 1) {
    ASSERT(!IsCmykImage() && static_cast<uint8_t>(alpha_flag >> 8) == 0);
    int left_shift = rect.left % 8;
    int right_shift = rect.right % 8;
    int new_width = rect.right / 8 - rect.left / 8;
    int index = 0;
    if (m_pPalette) {
      for (int i = 0; i < 2; i++) {
        if (m_pPalette.get()[i] == color)
          index = i;
      }
    } else {
      index = (static_cast<uint8_t>(color) == 0xff) ? 1 : 0;
    }
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan_top =
          const_cast<uint8_t*>(GetScanline(row)) + rect.left / 8;
      uint8_t* dest_scan_top_r =
          const_cast<uint8_t*>(GetScanline(row)) + rect.right / 8;
      uint8_t left_flag = *dest_scan_top & (255 << (8 - left_shift));
      uint8_t right_flag = *dest_scan_top_r & (255 >> right_shift);
      if (new_width) {
        memset(dest_scan_top + 1, index ? 255 : 0, new_width - 1);
        if (!index) {
          *dest_scan_top &= left_flag;
          *dest_scan_top_r &= right_flag;
        } else {
          *dest_scan_top |= ~left_flag;
          *dest_scan_top_r |= ~right_flag;
        }
      } else {
        if (!index) {
          *dest_scan_top &= left_flag | right_flag;
        } else {
          *dest_scan_top |= ~(left_flag | right_flag);
        }
      }
    }
    return true;
  }
  ASSERT(m_bpp >= 24);
  if (m_bpp < 24) {
    return false;
  }
  if (pIccTransform && CFX_GEModule::Get()->GetCodecModule()) {
    CCodec_IccModule* pIccModule =
        CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
    pIccModule->TranslateScanline(pIccTransform, color_p, color_p, 1);
  } else {
    if (alpha_flag >> 8 && !IsCmykImage()) {
      AdobeCMYK_to_sRGB1(FXSYS_GetCValue(color), FXSYS_GetMValue(color),
                         FXSYS_GetYValue(color), FXSYS_GetKValue(color),
                         color_p[2], color_p[1], color_p[0]);
    } else if (!(alpha_flag >> 8) && IsCmykImage()) {
      return false;
    }
  }
  if (!IsCmykImage()) {
    color_p[3] = (uint8_t)src_alpha;
  }
  int Bpp = m_bpp / 8;
  bool bAlpha = HasAlpha();
  bool bArgb = GetFormat() == FXDIB_Argb;
  if (src_alpha == 255) {
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan = m_pBuffer + row * m_Pitch + rect.left * Bpp;
      uint8_t* dest_scan_alpha =
          m_pAlphaMask ? (uint8_t*)m_pAlphaMask->GetScanline(row) + rect.left
                       : nullptr;
      if (dest_scan_alpha) {
        memset(dest_scan_alpha, 0xff, width);
      }
      if (Bpp == 4) {
        uint32_t* scan = (uint32_t*)dest_scan;
        for (int col = 0; col < width; col++) {
          *scan++ = dst_color;
        }
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = color_p[0];
          *dest_scan++ = color_p[1];
          *dest_scan++ = color_p[2];
        }
      }
    }
    return true;
  }
  for (int row = rect.top; row < rect.bottom; row++) {
    uint8_t* dest_scan = m_pBuffer + row * m_Pitch + rect.left * Bpp;
    if (bAlpha) {
      if (bArgb) {
        for (int col = 0; col < width; col++) {
          uint8_t back_alpha = dest_scan[3];
          if (back_alpha == 0) {
            FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_alpha, color_p[2],
                                                 color_p[1], color_p[0]));
            dest_scan += 4;
            continue;
          }
          uint8_t dest_alpha =
              back_alpha + src_alpha - back_alpha * src_alpha / 255;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[0], alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[1], alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[2], alpha_ratio);
          dest_scan++;
          *dest_scan++ = dest_alpha;
        }
      } else {
        uint8_t* dest_scan_alpha =
            (uint8_t*)m_pAlphaMask->GetScanline(row) + rect.left;
        for (int col = 0; col < width; col++) {
          uint8_t back_alpha = *dest_scan_alpha;
          if (back_alpha == 0) {
            *dest_scan_alpha++ = src_alpha;
            memcpy(dest_scan, color_p, Bpp);
            dest_scan += Bpp;
            continue;
          }
          uint8_t dest_alpha =
              back_alpha + src_alpha - back_alpha * src_alpha / 255;
          *dest_scan_alpha++ = dest_alpha;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          for (int comps = 0; comps < Bpp; comps++) {
            *dest_scan =
                FXDIB_ALPHA_MERGE(*dest_scan, color_p[comps], alpha_ratio);
            dest_scan++;
          }
        }
      }
    } else {
      for (int col = 0; col < width; col++) {
        for (int comps = 0; comps < Bpp; comps++) {
          if (comps == 3) {
            *dest_scan++ = 255;
            continue;
          }
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[comps], src_alpha);
          dest_scan++;
        }
      }
    }
  }
  return true;
}

bool CFX_DIBitmap::ConvertFormat(FXDIB_Format dest_format) {
  FXDIB_Format src_format = GetFormat();
  if (dest_format == src_format)
    return true;

  if (dest_format == FXDIB_8bppMask && src_format == FXDIB_8bppRgb &&
      !m_pPalette) {
    m_AlphaFlag = 1;
    return true;
  }
  if (dest_format == FXDIB_Argb && src_format == FXDIB_Rgb32) {
    m_AlphaFlag = 2;
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch + 3;
      for (int col = 0; col < m_Width; col++) {
        *scanline = 0xff;
        scanline += 4;
      }
    }
    return true;
  }
  int dest_bpp = dest_format & 0xff;
  int dest_pitch = (dest_bpp * m_Width + 31) / 32 * 4;
  uint8_t* dest_buf = FX_TryAlloc(uint8_t, dest_pitch * m_Height + 4);
  if (!dest_buf) {
    return false;
  }
  CFX_RetainPtr<CFX_DIBitmap> pAlphaMask;
  if (dest_format == FXDIB_Argb) {
    memset(dest_buf, 0xff, dest_pitch * m_Height + 4);
    if (m_pAlphaMask) {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* pDstScanline = dest_buf + row * dest_pitch + 3;
        const uint8_t* pSrcScanline = m_pAlphaMask->GetScanline(row);
        for (int col = 0; col < m_Width; col++) {
          *pDstScanline = *pSrcScanline++;
          pDstScanline += 4;
        }
      }
    }
  } else if (dest_format & 0x0200) {
    if (src_format == FXDIB_Argb) {
      pAlphaMask = CloneAlphaMask();
      if (!pAlphaMask) {
        FX_Free(dest_buf);
        return false;
      }
    } else {
      if (!m_pAlphaMask) {
        if (!BuildAlphaMask()) {
          FX_Free(dest_buf);
          return false;
        }
        pAlphaMask = std::move(m_pAlphaMask);
      } else {
        pAlphaMask = m_pAlphaMask;
      }
    }
  }
  bool ret = false;
  CFX_RetainPtr<CFX_DIBSource> holder(this);
  std::unique_ptr<uint32_t, FxFreeDeleter> pal_8bpp;
  ret = ConvertBuffer(dest_format, dest_buf, dest_pitch, m_Width, m_Height,
                      holder, 0, 0, &pal_8bpp);
  if (!ret) {
    FX_Free(dest_buf);
    return false;
  }
  m_pAlphaMask = pAlphaMask;
  m_pPalette = std::move(pal_8bpp);
  if (!m_bExtBuf)
    FX_Free(m_pBuffer);
  m_bExtBuf = false;
  m_pBuffer = dest_buf;
  m_bpp = (uint8_t)dest_format;
  m_AlphaFlag = (uint8_t)(dest_format >> 8);
  m_Pitch = dest_pitch;
  return true;
}