// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_ICCPROFILE_H_
#define CORE_FPDFAPI_PAGE_CPDF_ICCPROFILE_H_

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/cfx_unowned_ptr.h"

class CPDF_Stream;

class CPDF_IccProfile : public CFX_Retainable {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  CPDF_Stream* GetStream() const { return m_pStream.Get(); }
  bool IsValid() const { return IsSRGB() || IsSupported(); }
  bool IsSRGB() const { return m_bsRGB; }
  bool IsSupported() const { return !!m_pTransform; }
  void* transform() { return m_pTransform; }
  uint32_t GetComponents() const { return m_nSrcComponents; }

 private:
  CPDF_IccProfile(CPDF_Stream* pStream, const uint8_t* pData, uint32_t dwSize);
  ~CPDF_IccProfile() override;

  const bool m_bsRGB;
  CFX_UnownedPtr<CPDF_Stream> const m_pStream;
  void* m_pTransform = nullptr;
  uint32_t m_nSrcComponents = 0;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_ICCPROFILE_H_
