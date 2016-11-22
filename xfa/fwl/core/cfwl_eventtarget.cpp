// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_eventtarget.h"

#include "xfa/fwl/core/ifwl_widget.h"
#include "xfa/fwl/core/ifwl_widgetdelegate.h"

CFWL_EventTarget::CFWL_EventTarget(IFWL_Widget* pListener)
    : m_pListener(pListener), m_bInvalid(false) {}

CFWL_EventTarget::~CFWL_EventTarget() {}

void CFWL_EventTarget::SetEventSource(IFWL_Widget* pSource) {
  if (pSource)
    m_widgets.insert(pSource);
}

bool CFWL_EventTarget::ProcessEvent(CFWL_Event* pEvent) {
  IFWL_WidgetDelegate* pDelegate = m_pListener->GetDelegate();
  if (!pDelegate)
    return false;
  if (!m_widgets.empty() && m_widgets.count(pEvent->m_pSrcTarget) == 0)
    return false;

  pDelegate->OnProcessEvent(pEvent);
  return true;
}