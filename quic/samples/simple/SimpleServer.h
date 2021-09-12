#pragma once

#include <quic/common/test/TestUtils.h>
#include <quic/samples/simple/SimpleHandler.h>
#include <quic/server/QuicServer.h>

namespace quic {
namespace samples {

class SimpleServerTransportFactory : public quic::QuicServerTransportFactory {
 public:
  ~SimpleServerTransportFactory() override {
    while (!simpleHandlers_.empty()) {
      auto& handler = simpleHandlers_.back();
      handler->getEventBase()->runImmediatelyOrRunInEventBaseThreadAndWait(
          [this] { simpleHandlers_.pop_back(); });
    }
  }

  SimpleServerTransportFactory() = default;

  quic::QuicServerTransport::Ptr make(
      folly::EventBase* evb,
      std::unique_ptr<folly::AsyncUDPSocket> sock,
      const folly::SocketAddress&,
      QuicVersion,
      std::shared_ptr<const fizz::server::FizzServerContext>
          ctx) noexcept override {
    CHECK_EQ(evb, sock->getEventBase());
    auto simpleHandler = std::make_unique<SimpleHandler>(evb);
    auto transport = quic::QuicServerTransport::make(
        evb, std::move(sock), *simpleHandler, ctx);
    simpleHandler->setQuicSocket(transport);
    simpleHandlers_.push_back(std::move(simpleHandler));
    return transport;
  }

  std::vector<std::unique_ptr<SimpleHandler>> simpleHandlers_;

 private:
};

class SimpleServer {
 public:
  explicit SimpleServer(const std::string& host = "::1", uint16_t port = 6666)
      : host_(host), port_(port), server_(QuicServer::createQuicServer()) {
    server_->setQuicServerTransportFactory(
        std::make_unique<SimpleServerTransportFactory>());
    auto serverCtx = quic::test::createServerCtx();
    serverCtx->setClock(std::make_shared<fizz::SystemClock>());
    server_->setFizzContext(serverCtx);
  }

  void start() {
    folly::SocketAddress addr1(host_.c_str(), port_);
    addr1.setFromHostPort(host_, port_);
    server_->start(addr1, 0);
    LOG(INFO) << "Simple server started at: " << addr1.describe();
    eventbase_.loopForever();
  }

 private:
  std::string host_;
  uint16_t port_;
  folly::EventBase eventbase_;
  std::shared_ptr<quic::QuicServer> server_;
};

} // namespace samples
} // namespace quic
