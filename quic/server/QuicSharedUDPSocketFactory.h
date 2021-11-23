/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#pragma once

#include <quic/server/QuicUDPSocketFactory.h>

namespace quic {

class QuicSharedUDPSocketFactory : public QuicUDPSocketFactory {
 public:
  ~QuicSharedUDPSocketFactory() override {}
  QuicSharedUDPSocketFactory() {}

  std::unique_ptr<folly::AsyncUDPSocket> make(folly::EventBase* evb, rt::UdpConn* fd)
      override {
    auto sock = std::make_unique<folly::AsyncUDPSocket>(evb);
    if (fd != nullptr) {
      sock->setFD(
          folly::ShNetworkSocket::fromFd(fd),
          folly::AsyncUDPSocket::FDOwnership::SHARED);
      sock->setDFAndTurnOffPMTU();
    }
    return sock;
  }
};
} // namespace quic
