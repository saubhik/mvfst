/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/fizz/server/handshake/FizzServerHandshake.h>

#include <quic/fizz/handshake/FizzBridge.h>
#include <quic/fizz/server/handshake/AppToken.h>
#include <quic/fizz/server/handshake/FizzServerQuicHandshakeContext.h>

#include <fizz/protocol/Protocol.h>
#include <fizz/server/State.h>

// This is necessary for the conversion between QuicServerConnectionState and
// QuicConnectionStateBase and can be removed once ServerHandshake accepts
// QuicServerConnectionState.
#include <quic/server/state/ServerStateMachine.h>

#include <iostream>

#include "net.h"

namespace fizz {
namespace server {
struct ResumptionState;
} // namespace server
} // namespace fizz

namespace {
class FailingAppTokenValidator : public fizz::server::AppTokenValidator {
  bool validate(const fizz::server::ResumptionState&) const override {
    return false;
  }
};
} // namespace

namespace quic {

FizzServerHandshake::FizzServerHandshake(
    QuicServerConnectionState* conn,
    std::shared_ptr<FizzServerQuicHandshakeContext> fizzContext)
    : ServerHandshake(conn), fizzContext_(std::move(fizzContext)) {}

void FizzServerHandshake::initializeImpl(
    HandshakeCallback* callback,
    std::unique_ptr<fizz::server::AppTokenValidator> validator) {
  auto context = std::make_shared<fizz::server::FizzServerContext>(
      *fizzContext_->getContext());
  context->setFactory(cryptoFactory_.getFizzFactory());
  context->setSupportedCiphers({{fizz::CipherSuite::TLS_AES_128_GCM_SHA256}});
  context->setVersionFallbackEnabled(false);
  // Since Draft-17, client won't sent EOED
  context->setOmitEarlyRecordLayer(true);
  state_.context() = std::move(context);
  callback_ = callback;

  if (validator) {
    state_.appTokenValidator() = std::move(validator);
  } else {
    state_.appTokenValidator() = std::make_unique<FailingAppTokenValidator>();
  }
}

const CryptoFactory& FizzServerHandshake::getCryptoFactory() const {
  return cryptoFactory_;
}

const fizz::server::FizzServerContext* FizzServerHandshake::getContext() const {
  return state_.context();
}

EncryptionLevel FizzServerHandshake::getReadRecordLayerEncryptionLevel() {
  return getEncryptionLevelFromFizz(
      state_.readRecordLayer()->getEncryptionLevel());
}

void FizzServerHandshake::processSocketData(folly::IOBufQueue& queue) {
  startActions(machine_.processSocketData(state_, queue));
}

#if 0
// Converts the hex encoded string to an IOBuf.
std::unique_ptr<folly::IOBuf>
toIOBuf(std::string hexData, size_t headroom = 0, size_t tailroom = 0) {
  std::string out;
  CHECK(folly::unhexlify(hexData, out));
  return folly::IOBuf::copyBuffer(out, headroom, tailroom);
}

template <typename Array>
Array hexToBytes(const folly::StringPiece hex) {
  auto bytesString = folly::unhexlify(hex);
  Array bytes;
  memcpy(bytes.data(), bytesString.data(), bytes.size());
  return bytes;
}

using SampleBytes = std::array<uint8_t, 16>;
using InitialByte = std::array<uint8_t, 1>;
using PacketNumberBytes = std::array<uint8_t, 4>;

struct CipherBytes {
  SampleBytes sample;
  InitialByte initial;
  PacketNumberBytes packetNumber;

  explicit CipherBytes(
      const folly::StringPiece sampleHex,
      const folly::StringPiece initialHex,
      const folly::StringPiece packetNumberHex)
      : sample(hexToBytes<SampleBytes>(sampleHex)),
        initial(hexToBytes<InitialByte>(initialHex)),
        packetNumber(hexToBytes<PacketNumberBytes>(packetNumberHex)) {}
};

struct HeaderParams {
  fizz::CipherSuite cipher;
  folly::StringPiece key;
  folly::StringPiece sample;
  folly::StringPiece packetNumberBytes;
  folly::StringPiece initialByte;
  folly::StringPiece decryptedPacketNumberBytes;
  folly::StringPiece decryptedInitialByte;
};

HeaderParams headerParams{
    fizz::CipherSuite::TLS_AES_128_GCM_SHA256,
    folly::StringPiece{"0edd982a6ac527f2eddcbb7348dea5d7"},
    folly::StringPiece{"0000f3a694c75775b4e546172ce9e047"},
    folly::StringPiece{"0dbc195a"},
    folly::StringPiece{"c1"},
    folly::StringPiece{"00000002"},
    folly::StringPiece{"c3"}};

CipherBytes cipherBytes(
    headerParams.sample,
    headerParams.decryptedInitialByte,
    headerParams.decryptedPacketNumberBytes);
#endif

std::pair<std::unique_ptr<Aead>, std::unique_ptr<PacketNumberCipher>>
FizzServerHandshake::buildCiphers(folly::ByteRange secret) {
  auto aead = FizzAead::wrap(fizz::Protocol::deriveRecordAeadWithLabel(
      *state_.context()->getFactory(),
      *state_.keyScheduler(),
      *state_.cipher(),
      secret,
      kQuicKeyLabel,
      kQuicIVLabel));
  auto headerCipher = cryptoFactory_.makePacketNumberCipher(secret);

#if 0
  for (uint8_t i : secret)
    std::cout << fmt::format("{:x}", std::byte(i)) << ",";
  std::cout << std::endl;
#endif

  aead->setHashIndex();
  headerCipher->setHashIndex();

  const ssize_t bufLen = 16 + (ssize_t)secret.size();
  uint8_t buf[bufLen];
  uint64_t aeadHashIndex = aead->getHashIndex();
  uint64_t headerCipherIndex = headerCipher->getHashIndex();
  memcpy(buf, &(aeadHashIndex), 8);
  memcpy(buf + 8, &(headerCipherIndex), 8);
  uint8_t* bufPtr = buf + 16;
  for (const unsigned char& s : secret) {
    memcpy(bufPtr, &s, 1);
    bufPtr += 1;
  }

  rt::SendToIOKernel(buf, bufLen);

#if 0
  auto out = aead->getFizzAead()->encrypt(
      toIOBuf(folly::hexlify("plaintext")),
      toIOBuf("").get(),
      0);

  std::cout << R"(aead->encrypt(hexlify("plaintext"),"",0) = )"
            << folly::hexlify(out->moveToFbString().toStdString())
            << std::endl;

  auto key = folly::unhexlify(headerParams.key);
  headerCipher->setKey(folly::range(key));
  headerCipher->encryptLongHeader(
      cipherBytes.sample,
      folly::range(cipherBytes.initial),
      folly::range(cipherBytes.packetNumber));

  std::cout << "InitialByte: "
            << headerParams.decryptedInitialByte
            << " ----encryptLongHeader----> "
            << folly::hexlify(cipherBytes.initial)
            << std::endl;
  std::cout << "PacketNumberBytes: "
            << headerParams.decryptedPacketNumberBytes
            << " ----encryptLongHeader----> "
            << folly::hexlify(cipherBytes.packetNumber)
            << std::endl;

  headerCipher->decryptLongHeader(
      cipherBytes.sample,
      folly::range(cipherBytes.initial),
      folly::range(cipherBytes.packetNumber));

  std::cout << "InitialByte: "
            << headerParams.initialByte
            << " ----decryptLongHeader----> "
            << folly::hexlify(cipherBytes.initial)
            << std::endl;
  std::cout << "PacketNumberBytes: "
            << headerParams.packetNumberBytes
            << " ----decryptLongHeader----> "
            << folly::hexlify(cipherBytes.packetNumber)
            << std::endl;
#endif

  return {std::move(aead), std::move(headerCipher)};
}

void FizzServerHandshake::processAccept() {
  addProcessingActions(machine_.processAccept(
      state_, executor_, state_.context(), transportParams_));
}

bool FizzServerHandshake::processPendingCryptoEvent() {
  if (pendingEvents_.empty()) {
    return false;
  }

  auto write = std::move(pendingEvents_.front());
  pendingEvents_.pop_front();
  startActions(machine_.processWriteNewSessionTicket(state_, std::move(write)));
  return true;
}

void FizzServerHandshake::writeNewSessionTicketToCrypto(
    const AppToken& appToken) {
  fizz::WriteNewSessionTicket writeNST;
  writeNST.appToken = encodeAppToken(appToken);
  pendingEvents_.push_back(std::move(writeNST));
}

} // namespace quic
