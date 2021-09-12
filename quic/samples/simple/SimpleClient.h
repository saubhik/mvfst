#pragma once

#include <iostream>

#include <folly/fibers/Baton.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include <quic/api/QuicSocket.h>
#include <quic/client/QuicClientTransport.h>
#include <quic/common/test/TestClientUtils.h>
#include <quic/fizz/client/handshake/FizzClientQuicHandshakeContext.h>

namespace quic {
namespace samples {
class SimpleClient : public quic::QuicSocket::ConnectionCallback,
                     public quic::QuicSocket::ReadCallback,
                     public quic::QuicSocket::WriteCallback {
 public:
  SimpleClient(const std::string& host, uint16_t port)
      : host_(host), port_(port) {}

  void readAvailable(quic::StreamId streamId) noexcept override {
    auto readData = quicClient_->read(streamId, 0);
    if (readData.hasError()) {
      LOG(ERROR) << "SimpleClient failed read from stream=" << streamId
                 << ", error=" << (uint32_t)readData.error();
    }

    auto copy = readData->first->clone();
    if (recvOffsets_.find(streamId) == recvOffsets_.end()) {
      recvOffsets_[streamId] = copy->length();
    } else {
      recvOffsets_[streamId] += copy->length();
    }
    LOG(INFO) << "Client received data=" << copy->moveToFbString().toStdString()
              << " on stream=" << streamId;
  }

  void readError(
      quic::StreamId streamId,
      std::pair<quic::QuicErrorCode, folly::Optional<folly::StringPiece>>
          error) noexcept override {
    LOG(ERROR) << "SimpleClient failed read from stream=" << streamId
               << ", error=" << toString(error);
  }

  void onNewBidirectionalStream(quic::StreamId id) noexcept override {
    LOG(INFO) << "SimpleClient: new bidirectional stream=" << id;
    quicClient_->setReadCallback(id, this);
  }

  void onNewUnidirectionalStream(quic::StreamId id) noexcept override {
    LOG(INFO) << "SimpleClient: new unidirectional stream=" << id;
    quicClient_->setReadCallback(id, this);
  }

  void onStopSending(
      quic::StreamId id,
      quic::ApplicationErrorCode /*error*/) noexcept override {
    VLOG(10) << "SimpleClient got StopSending stream id=" << id;
  }

  void onConnectionEnd() noexcept override {
    LOG(INFO) << "SimpleClient connection end";
  }

  void onConnectionError(
      std::pair<quic::QuicErrorCode, std::string> error) noexcept override {
    LOG(ERROR) << "SimpleClient error: " << toString(error.first)
               << "; errStr=" << error.second;
    startDone_.post();
  }

  void onTransportReady() noexcept override {
    startDone_.post();
  }

  void onStreamWriteReady(
      quic::StreamId id,
      uint64_t maxToSend) noexcept override {
    LOG(INFO) << "SimpleClient socket is write ready with maxToSend="
              << maxToSend;
    sendMessage(id, pendingOutput_[id]);
  }

  void start() {
    folly::ScopedEventBaseThread networkThread("SimpleClientThread");
    auto evb = networkThread.getEventBase();
    folly::SocketAddress addr(host_.c_str(), port_);

    evb->runInEventBaseThreadAndWait([&] {
      auto sock = std::make_unique<folly::AsyncUDPSocket>(evb);
      auto fizzClientContext =
          FizzClientQuicHandshakeContext::Builder()
              .setCertificateVerifier(test::createTestCertificateVerifier())
              .build();
      quicClient_ = std::make_shared<quic::QuicClientTransport>(
          evb, std::move(sock), std::move(fizzClientContext));
      quicClient_->setHostname("simple.com");
      quicClient_->addNewPeerAddress(addr);

      LOG(INFO) << "SimpleClient connecting to " << addr.describe();
      quicClient_->start(this);
    });

    startDone_.wait();

    std::string message;
    auto client = quicClient_;
    while (std::getline(std::cin, message)) {
      if (message.empty()) {
        continue;
      }
      evb->runInEventBaseThreadAndWait([=] {
        auto streamId = client->createBidirectionalStream().value();
        client->setReadCallback(streamId, this);
        pendingOutput_[streamId].append(folly::IOBuf::copyBuffer(message));
        sendMessage(streamId, pendingOutput_[streamId]);
      });
    }

    LOG(INFO) << "SimpleClient stopping client";
  }

  ~SimpleClient() override = default;

 private:
  void sendMessage(quic::StreamId id, BufQueue& data) {
    auto message = data.move();
    auto res = quicClient_->writeChain(id, message->clone(), true);
    if (res.hasError()) {
      LOG(ERROR) << "SimpleClient writeChain error=" << uint32_t(res.error());
    } else {
      auto str = message->moveToFbString().toStdString();
      LOG(INFO) << "SimpleClient wrote \"" << str << "\", len=" << str.size()
                << " on stream=" << id;
      pendingOutput_.erase(id);
    }
  }

  std::string host_;
  uint16_t port_;
  std::shared_ptr<quic::QuicClientTransport> quicClient_;
  std::map<quic::StreamId, BufQueue> pendingOutput_;
  std::map<quic::StreamId, uint64_t> recvOffsets_;
  folly::fibers::Baton startDone_;
};
} // namespace samples
} // namespace quic
