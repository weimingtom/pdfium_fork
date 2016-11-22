// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_MSGSETFOCUS_H_
#define XFA_FWL_CORE_CFWL_MSGSETFOCUS_H_

#include <memory>

#include "xfa/fwl/core/cfwl_message.h"

class CFWL_MsgSetFocus : public CFWL_Message {
 public:
  CFWL_MsgSetFocus();
  ~CFWL_MsgSetFocus() override;

  // CFWL_Message
  std::unique_ptr<CFWL_Message> Clone() override;
  CFWL_MessageType GetClassID() const override;

  IFWL_Widget* m_pKillFocus;
};

#endif  // XFA_FWL_CORE_CFWL_MSGSETFOCUS_H_