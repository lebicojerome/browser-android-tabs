// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/frames/quic_stop_sending_frame.h"

namespace net {

QuicStopSendingFrame::QuicStopSendingFrame()
    : stream_id(0), application_error_code(0) {}

QuicStopSendingFrame::QuicStopSendingFrame(
    QuicControlFrameId control_frame_id,
    QuicStreamId stream_id,
    QuicApplicationErrorCode application_error_code)
    : QuicControlFrame(control_frame_id),
      stream_id(stream_id),
      application_error_code(application_error_code) {}

std::ostream& operator<<(std::ostream& os, const QuicStopSendingFrame& frame) {
  os << "{ control_frame_id: " << frame.control_frame_id
     << ", stream_id: " << frame.stream_id
     << ", application_error_code: " << frame.application_error_code << " }\n";
  return os;
}

}  // namespace net
