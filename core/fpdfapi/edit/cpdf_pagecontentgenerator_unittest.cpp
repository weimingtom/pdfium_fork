// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/edit/cpdf_pagecontentgenerator.h"

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pathobject.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"
#include "third_party/base/ptr_util.h"

class CPDF_PageContentGeneratorTest : public testing::Test {
 protected:
  void SetUp() override { CPDF_ModuleMgr::Get()->Init(); }

  void TearDown() override {
    CPDF_ModuleMgr::Destroy();
  }

  void TestProcessPath(CPDF_PageContentGenerator* pGen,
                       std::ostringstream* buf,
                       CPDF_PathObject* pPathObj) {
    pGen->ProcessPath(buf, pPathObj);
  }

  CPDF_Dictionary* TestGetResource(CPDF_PageContentGenerator* pGen,
                                   const CFX_ByteString& type,
                                   const CFX_ByteString& name) {
    return pGen->m_pPage->m_pResources->GetDictFor(type)->GetDictFor(name);
  }

  void TestProcessText(CPDF_PageContentGenerator* pGen,
                       std::ostringstream* buf,
                       CPDF_TextObject* pTextObj) {
    pGen->ProcessText(buf, pTextObj);
  }
};

TEST_F(CPDF_PageContentGeneratorTest, ProcessRect) {
  auto pPathObj = pdfium::MakeUnique<CPDF_PathObject>();
  pPathObj->m_Path.AppendRect(10, 5, 13, 30);
  pPathObj->m_FillType = FXFILL_ALTERNATE;
  pPathObj->m_bStroke = true;

  auto pTestPage = pdfium::MakeUnique<CPDF_Page>(nullptr, nullptr, false);
  CPDF_PageContentGenerator generator(pTestPage.get());
  std::ostringstream buf;
  TestProcessPath(&generator, &buf, pPathObj.get());
  EXPECT_EQ("q 10 5 3 25 re B* Q\n", CFX_ByteString(buf));

  pPathObj = pdfium::MakeUnique<CPDF_PathObject>();
  pPathObj->m_Path.AppendPoint(CFX_PointF(0, 0), FXPT_TYPE::MoveTo, false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(5.2f, 0), FXPT_TYPE::LineTo, false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(5.2f, 3.78f), FXPT_TYPE::LineTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(0, 3.78f), FXPT_TYPE::LineTo, true);
  pPathObj->m_FillType = 0;
  pPathObj->m_bStroke = false;
  buf.str("");

  TestProcessPath(&generator, &buf, pPathObj.get());
  EXPECT_EQ("q 0 0 5.2 3.78 re n Q\n", CFX_ByteString(buf));
}

TEST_F(CPDF_PageContentGeneratorTest, ProcessPath) {
  auto pPathObj = pdfium::MakeUnique<CPDF_PathObject>();
  pPathObj->m_Path.AppendPoint(CFX_PointF(3.102f, 4.67f), FXPT_TYPE::MoveTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(5.45f, 0.29f), FXPT_TYPE::LineTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(4.24f, 3.15f), FXPT_TYPE::BezierTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(4.65f, 2.98f), FXPT_TYPE::BezierTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(3.456f, 0.24f), FXPT_TYPE::BezierTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(10.6f, 11.15f), FXPT_TYPE::LineTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(11, 12.5f), FXPT_TYPE::LineTo, false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(11.46f, 12.67f), FXPT_TYPE::BezierTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(11.84f, 12.96f), FXPT_TYPE::BezierTo,
                               false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(12, 13.64f), FXPT_TYPE::BezierTo,
                               true);
  pPathObj->m_FillType = FXFILL_WINDING;
  pPathObj->m_bStroke = false;

  auto pTestPage = pdfium::MakeUnique<CPDF_Page>(nullptr, nullptr, false);
  CPDF_PageContentGenerator generator(pTestPage.get());
  std::ostringstream buf;
  TestProcessPath(&generator, &buf, pPathObj.get());
  EXPECT_EQ(
      "q 3.102 4.67 m 5.45 0.29 l 4.24 3.15 4.65 2.98 3.456 0.24 c 10.6 11.15 "
      "l 11 12.5 l 11.46 12.67 11.84 12.96 12 13.64 c h f Q\n",
      CFX_ByteString(buf));
}

TEST_F(CPDF_PageContentGeneratorTest, ProcessGraphics) {
  auto pPathObj = pdfium::MakeUnique<CPDF_PathObject>();
  pPathObj->m_Path.AppendPoint(CFX_PointF(1, 2), FXPT_TYPE::MoveTo, false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(3, 4), FXPT_TYPE::LineTo, false);
  pPathObj->m_Path.AppendPoint(CFX_PointF(5, 6), FXPT_TYPE::LineTo, true);
  pPathObj->m_FillType = FXFILL_WINDING;
  pPathObj->m_bStroke = true;

  float rgb[3] = {0.5f, 0.7f, 0.35f};
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB);
  pPathObj->m_ColorState.SetFillColor(pCS, rgb, 3);

  float rgb2[3] = {1, 0.9f, 0};
  pPathObj->m_ColorState.SetStrokeColor(pCS, rgb2, 3);
  pPathObj->m_GeneralState.SetFillAlpha(0.5f);
  pPathObj->m_GeneralState.SetStrokeAlpha(0.8f);

  auto pDoc = pdfium::MakeUnique<CPDF_Document>(nullptr);
  pDoc->CreateNewDoc();
  CPDF_Dictionary* pPageDict = pDoc->CreateNewPage(0);
  auto pTestPage = pdfium::MakeUnique<CPDF_Page>(pDoc.get(), pPageDict, false);
  CPDF_PageContentGenerator generator(pTestPage.get());
  std::ostringstream buf;
  TestProcessPath(&generator, &buf, pPathObj.get());
  CFX_ByteString pathString(buf);

  // Color RGB values used are integers divided by 255.
  EXPECT_EQ("q 0.501961 0.701961 0.34902 rg 1 0.901961 0 RG /",
            pathString.Left(48));
  EXPECT_EQ(" gs 1 2 m 3 4 l 5 6 l h B Q\n", pathString.Right(28));
  ASSERT_TRUE(pathString.GetLength() > 76);
  CPDF_Dictionary* externalGS = TestGetResource(
      &generator, "ExtGState", pathString.Mid(48, pathString.GetLength() - 76));
  ASSERT_TRUE(externalGS);
  EXPECT_EQ(0.5f, externalGS->GetNumberFor("ca"));
  EXPECT_EQ(0.8f, externalGS->GetNumberFor("CA"));

  // Same path, now with a stroke.
  pPathObj->m_GraphState.SetLineWidth(10.5f);
  buf.str("");
  TestProcessPath(&generator, &buf, pPathObj.get());
  CFX_ByteString pathString2(buf);
  EXPECT_EQ("q 0.501961 0.701961 0.34902 rg 1 0.901961 0 RG 10.5 w /",
            pathString2.Left(55));
  EXPECT_EQ(" gs 1 2 m 3 4 l 5 6 l h B Q\n", pathString2.Right(28));

  // Compare with the previous (should use same dictionary for gs)
  EXPECT_EQ(pathString.GetLength() + 7, pathString2.GetLength());
  EXPECT_EQ(pathString.Mid(48, pathString.GetLength() - 76),
            pathString2.Mid(55, pathString2.GetLength() - 83));
}

TEST_F(CPDF_PageContentGeneratorTest, ProcessStandardText) {
  // Checking font whose font dictionary is not yet indirect object.
  auto pDoc = pdfium::MakeUnique<CPDF_Document>(nullptr);
  pDoc->CreateNewDoc();
  CPDF_Dictionary* pPageDict = pDoc->CreateNewPage(0);
  auto pTestPage = pdfium::MakeUnique<CPDF_Page>(pDoc.get(), pPageDict, false);
  CPDF_PageContentGenerator generator(pTestPage.get());
  auto pTextObj = pdfium::MakeUnique<CPDF_TextObject>();
  CPDF_Font* pFont = CPDF_Font::GetStockFont(pDoc.get(), "Times-Roman");
  pTextObj->m_TextState.SetFont(pFont);
  pTextObj->m_TextState.SetFontSize(10.0f);
  float rgb[3] = {0.5f, 0.7f, 0.35f};
  CPDF_ColorSpace* pCS = CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB);
  pTextObj->m_ColorState.SetFillColor(pCS, rgb, 3);

  float rgb2[3] = {1, 0.9f, 0};
  pTextObj->m_ColorState.SetStrokeColor(pCS, rgb2, 3);
  pTextObj->m_GeneralState.SetFillAlpha(0.5f);
  pTextObj->m_GeneralState.SetStrokeAlpha(0.8f);
  pTextObj->Transform(CFX_Matrix(1, 0, 0, 1, 100, 100));
  pTextObj->SetText("Hello World");
  std::ostringstream buf;
  TestProcessText(&generator, &buf, pTextObj.get());
  CFX_ByteString textString(buf);
  int firstResourceAt = textString.Find('/') + 1;
  int secondResourceAt = textString.ReverseFind('/') + 1;
  CFX_ByteString firstString = textString.Left(firstResourceAt);
  CFX_ByteString midString =
      textString.Mid(firstResourceAt, secondResourceAt - firstResourceAt);
  CFX_ByteString lastString =
      textString.Right(textString.GetLength() - secondResourceAt);
  CFX_ByteString compareString1 = "BT 1 0 0 1 100 100 Tm /";
  // Color RGB values used are integers divided by 255.
  CFX_ByteString compareString2 =
      " 10 Tf q 0.501961 0.701961 0.34902 rg 1 0.901961 0 RG /";
  CFX_ByteString compareString3 = " gs <48656C6C6F20576F726C64> Tj ET Q\n";
  EXPECT_LT(compareString1.GetLength() + compareString2.GetLength() +
                compareString3.GetLength(),
            textString.GetLength());
  EXPECT_EQ(compareString1, firstString.Left(compareString1.GetLength()));
  EXPECT_EQ(compareString2, midString.Right(compareString2.GetLength()));
  EXPECT_EQ(compareString3, lastString.Right(compareString3.GetLength()));
  CPDF_Dictionary* externalGS = TestGetResource(
      &generator, "ExtGState",
      lastString.Left(lastString.GetLength() - compareString3.GetLength()));
  ASSERT_TRUE(externalGS);
  EXPECT_EQ(0.5f, externalGS->GetNumberFor("ca"));
  EXPECT_EQ(0.8f, externalGS->GetNumberFor("CA"));
  CPDF_Dictionary* fontDict = TestGetResource(
      &generator, "Font",
      midString.Left(midString.GetLength() - compareString2.GetLength()));
  ASSERT_TRUE(fontDict);
  EXPECT_EQ("Font", fontDict->GetStringFor("Type"));
  EXPECT_EQ("Type1", fontDict->GetStringFor("Subtype"));
  EXPECT_EQ("Times-Roman", fontDict->GetStringFor("BaseFont"));
}

TEST_F(CPDF_PageContentGeneratorTest, ProcessText) {
  // Checking font whose font dictionary is already an indirect object.
  auto pDoc = pdfium::MakeUnique<CPDF_Document>(nullptr);
  pDoc->CreateNewDoc();
  CPDF_Dictionary* pPageDict = pDoc->CreateNewPage(0);
  auto pTestPage = pdfium::MakeUnique<CPDF_Page>(pDoc.get(), pPageDict, false);
  CPDF_PageContentGenerator generator(pTestPage.get());

  std::ostringstream buf;
  {
    // Set the text object font and text
    auto pTextObj = pdfium::MakeUnique<CPDF_TextObject>();
    CPDF_Dictionary* pDict = pDoc->NewIndirect<CPDF_Dictionary>();
    pDict->SetNewFor<CPDF_Name>("Type", "Font");
    pDict->SetNewFor<CPDF_Name>("Subtype", "TrueType");
    CPDF_Font* pFont = CPDF_Font::GetStockFont(pDoc.get(), "Arial");
    pDict->SetNewFor<CPDF_Name>("BaseFont", pFont->GetBaseFont());

    CPDF_Dictionary* pDesc = pDoc->NewIndirect<CPDF_Dictionary>();
    pDesc->SetNewFor<CPDF_Name>("Type", "FontDescriptor");
    pDesc->SetNewFor<CPDF_Name>("FontName", pFont->GetBaseFont());
    pDict->SetNewFor<CPDF_Reference>("FontDescriptor", pDoc.get(),
                                     pDesc->GetObjNum());

    CPDF_Font* loadedFont = pDoc->LoadFont(pDict);
    pTextObj->m_TextState.SetFont(loadedFont);
    pTextObj->m_TextState.SetFontSize(15.5f);
    pTextObj->SetText("I am indirect");

    TestProcessText(&generator, &buf, pTextObj.get());
  }

  CFX_ByteString textString(buf);
  int firstResourceAt = textString.Find('/') + 1;
  CFX_ByteString firstString = textString.Left(firstResourceAt);
  CFX_ByteString lastString =
      textString.Right(textString.GetLength() - firstResourceAt);
  CFX_ByteString compareString1 = "BT 1 0 0 1 0 0 Tm /";
  CFX_ByteString compareString2 =
      " 15.5 Tf q <4920616D20696E646972656374> Tj ET Q\n";
  EXPECT_LT(compareString1.GetLength() + compareString2.GetLength(),
            textString.GetLength());
  EXPECT_EQ(compareString1, textString.Left(compareString1.GetLength()));
  EXPECT_EQ(compareString2, textString.Right(compareString2.GetLength()));
  CPDF_Dictionary* fontDict = TestGetResource(
      &generator, "Font",
      textString.Mid(compareString1.GetLength(),
                     textString.GetLength() - compareString1.GetLength() -
                         compareString2.GetLength()));
  ASSERT_TRUE(fontDict);
  EXPECT_TRUE(fontDict->GetObjNum());
  EXPECT_EQ("Font", fontDict->GetStringFor("Type"));
  EXPECT_EQ("TrueType", fontDict->GetStringFor("Subtype"));
  EXPECT_EQ("Helvetica", fontDict->GetStringFor("BaseFont"));
  CPDF_Dictionary* fontDesc = fontDict->GetDictFor("FontDescriptor");
  ASSERT_TRUE(fontDesc);
  EXPECT_TRUE(fontDesc->GetObjNum());
  EXPECT_EQ("FontDescriptor", fontDesc->GetStringFor("Type"));
  EXPECT_EQ("Helvetica", fontDesc->GetStringFor("FontName"));
}
