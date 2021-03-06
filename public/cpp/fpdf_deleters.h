// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PUBLIC_CPP_FPDF_DELETERS_H_
#define PUBLIC_CPP_FPDF_DELETERS_H_

#include "public/fpdf_dataavail.h"
#include "public/fpdf_edit.h"
#include "public/fpdf_formfill.h"
#include "public/fpdf_structtree.h"
#include "public/fpdf_text.h"
#include "public/fpdfview.h"

// Custom deleters for using FPDF_* types with std::unique_ptr<>.

struct FPDFAvailDeleter {
  inline void operator()(FPDF_AVAIL avail) { FPDFAvail_Destroy(avail); }
};

struct FPDFBitmapDeleter {
  inline void operator()(FPDF_BITMAP bitmap) { FPDFBitmap_Destroy(bitmap); }
};

struct FPDFDocumentDeleter {
  inline void operator()(FPDF_DOCUMENT doc) { FPDF_CloseDocument(doc); }
};

struct FPDFFormHandleDeleter {
  inline void operator()(FPDF_FORMHANDLE form) {
    FPDFDOC_ExitFormFillEnvironment(form);
  }
};

struct FPDFTextPageDeleter {
  inline void operator()(FPDF_TEXTPAGE text) { FPDFText_ClosePage(text); }
};

struct FPDFPageDeleter {
  inline void operator()(FPDF_PAGE page) { FPDF_ClosePage(page); }
};

struct FPDFStructTreeDeleter {
  inline void operator()(FPDF_STRUCTTREE tree) { FPDF_StructTree_Close(tree); }
};

struct FPDFFontDeleter {
  inline void operator()(FPDF_FONT font) { FPDFFont_Close(font); }
};

#endif  // PUBLIC_CPP_FPDF_DELETERS_H_
