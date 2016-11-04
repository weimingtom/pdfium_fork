// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_SCROLLBAR_H_
#define XFA_FWL_CORE_IFWL_SCROLLBAR_H_

#include "core/fxcrt/fx_system.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/fwl_error.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_timer.h"
#include "xfa/fwl/core/ifwl_widget.h"

class IFWL_Widget;

#define FWL_STYLEEXT_SCB_Horz (0L << 0)
#define FWL_STYLEEXT_SCB_Vert (1L << 0)

enum FWL_SCBCODE {
  FWL_SCBCODE_None = 1,
  FWL_SCBCODE_Min,
  FWL_SCBCODE_Max,
  FWL_SCBCODE_PageBackward,
  FWL_SCBCODE_PageForward,
  FWL_SCBCODE_StepBackward,
  FWL_SCBCODE_StepForward,
  FWL_SCBCODE_Pos,
  FWL_SCBCODE_TrackPos,
  FWL_SCBCODE_EndScroll,
};

class IFWL_ScrollBarDP : public IFWL_DataProvider {};

class IFWL_ScrollBar : public IFWL_Widget {
 public:
  IFWL_ScrollBar(const IFWL_App* app,
                 std::unique_ptr<CFWL_WidgetProperties> properties,
                 IFWL_Widget* pOuter);
  ~IFWL_ScrollBar() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  FWL_Error GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  FWL_Error Update() override;
  FWL_Error DrawWidget(CFX_Graphics* pGraphics,
                       const CFX_Matrix* pMatrix = nullptr) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  bool IsVertical();
  FWL_Error GetRange(FX_FLOAT& fMin, FX_FLOAT& fMax);
  FWL_Error SetRange(FX_FLOAT fMin, FX_FLOAT fMax);
  FX_FLOAT GetPageSize();
  FWL_Error SetPageSize(FX_FLOAT fPageSize);
  FX_FLOAT GetStepSize();
  FWL_Error SetStepSize(FX_FLOAT fStepSize);
  FX_FLOAT GetPos();
  FWL_Error SetPos(FX_FLOAT fPos);
  FX_FLOAT GetTrackPos();
  FWL_Error SetTrackPos(FX_FLOAT fTrackPos);
  bool DoScroll(uint32_t dwCode, FX_FLOAT fPos = 0.0f);
  FWL_Error SetOuter(IFWL_Widget* pOuter);

 protected:
  class Timer : public IFWL_Timer {
   public:
    explicit Timer(IFWL_ScrollBar* pToolTip);
    ~Timer() override {}

    void Run(IFWL_TimerInfo* pTimerInfo) override;
  };
  friend class IFWL_ScrollBar::Timer;

  IFWL_ScrollBar();
  void DrawTrack(CFX_Graphics* pGraphics,
                 IFWL_ThemeProvider* pTheme,
                 bool bLower = true,
                 const CFX_Matrix* pMatrix = nullptr);
  void DrawArrowBtn(CFX_Graphics* pGraphics,
                    IFWL_ThemeProvider* pTheme,
                    bool bMinBtn = true,
                    const CFX_Matrix* pMatrix = nullptr);
  void DrawThumb(CFX_Graphics* pGraphics,
                 IFWL_ThemeProvider* pTheme,
                 const CFX_Matrix* pMatrix = nullptr);
  void Layout();
  void CalcButtonLen();
  void CalcMinButtonRect(CFX_RectF& rect);
  void CalcMaxButtonRect(CFX_RectF& rect);
  void CalcThumbButtonRect(CFX_RectF& rect);
  void CalcMinTrackRect(CFX_RectF& rect);
  void CalcMaxTrackRect(CFX_RectF& rect);
  FX_FLOAT GetTrackPointPos(FX_FLOAT fx, FX_FLOAT fy);
  void GetTrackRect(CFX_RectF& rect, bool bLower = true);
  bool SendEvent();
  bool OnScroll(uint32_t dwCode, FX_FLOAT fPos);

  IFWL_TimerInfo* m_pTimerInfo;
  FX_FLOAT m_fRangeMin;
  FX_FLOAT m_fRangeMax;
  FX_FLOAT m_fPageSize;
  FX_FLOAT m_fStepSize;
  FX_FLOAT m_fPos;
  FX_FLOAT m_fTrackPos;
  int32_t m_iMinButtonState;
  int32_t m_iMaxButtonState;
  int32_t m_iThumbButtonState;
  int32_t m_iMinTrackState;
  int32_t m_iMaxTrackState;
  FX_FLOAT m_fLastTrackPos;
  FX_FLOAT m_cpTrackPointX;
  FX_FLOAT m_cpTrackPointY;
  int32_t m_iMouseWheel;
  bool m_bTrackMouseLeave;
  bool m_bMouseHover;
  bool m_bMouseDown;
  bool m_bRepaintThumb;
  FX_FLOAT m_fButtonLen;
  bool m_bMinSize;
  CFX_RectF m_rtClient;
  CFX_RectF m_rtThumb;
  CFX_RectF m_rtMinBtn;
  CFX_RectF m_rtMaxBtn;
  CFX_RectF m_rtMinTrack;
  CFX_RectF m_rtMaxTrack;
  bool m_bCustomLayout;
  FX_FLOAT m_fMinThumb;
  IFWL_ScrollBar::Timer m_Timer;

 private:
  void OnLButtonDown(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy);
  void OnLButtonUp(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy);
  void OnMouseMove(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy);
  void OnMouseLeave();
  void OnMouseWheel(FX_FLOAT fx,
                    FX_FLOAT fy,
                    uint32_t dwFlags,
                    FX_FLOAT fDeltaX,
                    FX_FLOAT fDeltaY);
  void DoMouseDown(int32_t iItem,
                   const CFX_RectF& rtItem,
                   int32_t& iState,
                   FX_FLOAT fx,
                   FX_FLOAT fy);
  void DoMouseUp(int32_t iItem,
                 const CFX_RectF& rtItem,
                 int32_t& iState,
                 FX_FLOAT fx,
                 FX_FLOAT fy);
  void DoMouseMove(int32_t iItem,
                   const CFX_RectF& rtItem,
                   int32_t& iState,
                   FX_FLOAT fx,
                   FX_FLOAT fy);
  void DoMouseLeave(int32_t iItem, const CFX_RectF& rtItem, int32_t& iState);
  void DoMouseHover(int32_t iItem, const CFX_RectF& rtItem, int32_t& iState);
};

#endif  // XFA_FWL_CORE_IFWL_SCROLLBAR_H_