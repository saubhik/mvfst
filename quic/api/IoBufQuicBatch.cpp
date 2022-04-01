/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/api/IoBufQuicBatch.h>

#include <quic/common/SocketUtil.h>
#include <quic/happyeyeballs/QuicHappyEyeballsFunctions.h>

#if PROFILING_ENABLED
namespace {
std::unordered_map<std::string, uint64_t> totElapsed;
}
#endif

namespace quic {
IOBufQuicBatch::IOBufQuicBatch(
    BatchWriterPtr&& batchWriter,
    bool threadLocal,
    folly::AsyncUDPSocket& sock,
    folly::SocketAddress& peerAddress,
    QuicConnectionStateBase& conn,
    QuicConnectionStateBase::HappyEyeballsState& happyEyeballsState)
    : batchWriter_(std::move(batchWriter)),
      threadLocal_(threadLocal),
      sock_(sock),
      peerAddress_(peerAddress),
      conn_(conn),
      happyEyeballsState_(happyEyeballsState) {}

bool IOBufQuicBatch::write(
    std::unique_ptr<folly::IOBuf>&& buf,
    size_t encodedSize) {
#if PROFILING_ENABLED
  uint64_t st = microtime();
#endif
  pktSent_++;

  // see if we need to flush the prev buffer(s)
  if (batchWriter_->needsFlush(encodedSize)) {
    // continue even if we get an error here
    flush(FlushType::FLUSH_TYPE_ALWAYS);
  }

  // try to append the new buffers
  if (batchWriter_->append(
          std::move(buf),
          encodedSize,
          peerAddress_,
          threadLocal_ ? &sock_ : nullptr)) {
    // return if we get an error here
#if PROFILING_ENABLED
    totElapsed["write"] += microtime() - st;
    VLOG_EVERY_N(1, 10000) << "quic::IOBufQuicBatch::write()"
                           << " tot = " << totElapsed["write"] << " micros"
                           << (totElapsed["write"] = 0);
#endif
    return flush(FlushType::FLUSH_TYPE_ALWAYS);
  }

  return true;
}

bool IOBufQuicBatch::flush(FlushType flushType) {
#if PROFILING_ENABLED
  uint64_t st = microtime();
#endif
  if (threadLocal_ &&
      (flushType == FlushType::FLUSH_TYPE_ALLOW_THREAD_LOCAL_DELAY)) {
    return true;
  }
#if PROFILING_ENABLED
  totElapsed["flush-1"] += microtime() - st;
  VLOG_EVERY_N(1, 10000) << "quic::IOBufQuicBatch::flush() PART 1"
                         << " tot = " << totElapsed["flush-1"] << " micros"
                         << (totElapsed["flush-1"] = 0);
#endif
  bool ret = flushInternal();
#if PROFILING_ENABLED
  st = microtime();
#endif
  reset();
#if PROFILING_ENABLED
  totElapsed["flush-2"] += microtime() - st;
  VLOG_EVERY_N(1, 10000) << "quic::IOBufQuicBatch::flush() PART 2"
                         << " tot = " << totElapsed["flush-2"] << " micros"
                         << (totElapsed["flush-2"] = 0);
#endif
  return ret;
}

void IOBufQuicBatch::reset() {
  batchWriter_->reset();
}

bool IOBufQuicBatch::isRetriableError(int err) {
  return err == EAGAIN || err == EWOULDBLOCK || err == ENOBUFS ||
      err == EMSGSIZE;
}

bool IOBufQuicBatch::flushInternal() {
#if PROFILING_ENABLED
  uint64_t st = microtime();
#endif
  if (batchWriter_->empty()) {
    return true;
  }

  bool written = false;
  if (happyEyeballsState_.shouldWriteToFirstSocket) {
#if PROFILING_ENABLED
    totElapsed["flushInternal-1"] += microtime() - st;
    VLOG_EVERY_N(1, 10000) << "quic::IOBufQuicBatch::flushInternal() PART 1"
                           << " tot = " << totElapsed["flushInternal-1"]
                           << " micros"
                           << (totElapsed["flushInternal-1"] = 0);
#endif
    auto consumed = batchWriter_->write(sock_, peerAddress_);
#if PROFILING_ENABLED
    st = microtime();
#endif
    written = (consumed >= 0);
    happyEyeballsState_.shouldWriteToFirstSocket =
        (consumed >= 0 || isRetriableError(errno));

    if (!happyEyeballsState_.shouldWriteToFirstSocket) {
      sock_.pauseRead();
    }
  }

  // If error occured on first socket, kick off second socket immediately
  if (!written && happyEyeballsState_.connAttemptDelayTimeout &&
      happyEyeballsState_.connAttemptDelayTimeout->isScheduled()) {
    happyEyeballsState_.connAttemptDelayTimeout->cancelTimeout();
    happyEyeballsStartSecondSocket(happyEyeballsState_);
  }

  if (happyEyeballsState_.shouldWriteToSecondSocket) {
#if PROFILING_ENABLED
    totElapsed["flushInternal-1"] += microtime() - st;
    VLOG_EVERY_N(1, 10000) << "quic::IOBufQuicBatch::flushInternal() PART 1"
                           << " tot = " << totElapsed["flushInternal-1"]
                           << " micros"
                           << (totElapsed["flushInternal-1"] = 0);
#endif
    // TODO: if the errno is EMSGSIZE, and we move on with the second socket,
    // we actually miss the chance to fix our UDP packet size with the first
    // socket.
    auto consumed = batchWriter_->write(
        *happyEyeballsState_.secondSocket,
        happyEyeballsState_.secondPeerAddress);
#if PROFILING_ENABLED
    st = microtime();
#endif
    // written is marked true if either socket write succeeds
    written |= (consumed >= 0);
    happyEyeballsState_.shouldWriteToSecondSocket =
        (consumed >= 0 || isRetriableError(errno));
    if (!happyEyeballsState_.shouldWriteToSecondSocket) {
      happyEyeballsState_.secondSocket->pauseRead();
    }
  }

  int errnoCopy = 0;
  if (!written) {
    errnoCopy = errno;
    QUIC_STATS(
        conn_.statsCallback,
        onUDPSocketWriteError,
        QuicTransportStatsCallback::errnoToSocketErrorType(errnoCopy));
  }

  // TODO: handle ENOBUFS and backpressure the socket.
  if (!happyEyeballsState_.shouldWriteToFirstSocket &&
      !happyEyeballsState_.shouldWriteToSecondSocket) {
    // Both sockets becomes fatal, close connection
    std::string errorMsg = folly::to<std::string>(
        folly::errnoStr(errnoCopy),
        (errnoCopy == EMSGSIZE)
            ? folly::to<std::string>(", pktSize=", batchWriter_->size())
            : "");
    VLOG(4) << "Error writing to the socket " << errorMsg << " "
            << peerAddress_;

    // We can get write error for any reason, close the conn only if network
    // is unreachable, for all others, we throw a transport exception
    if (isNetworkUnreachable(errno)) {
      throw QuicInternalException(
          folly::to<std::string>("Error on socket write ", errorMsg),
          LocalErrorCode::CONNECTION_ABANDONED);
    } else {
      throw QuicTransportException(
          folly::to<std::string>("Error on socket write ", errorMsg),
          TransportErrorCode::INTERNAL_ERROR);
    }
  }

#if PROFILING_ENABLED
  totElapsed["flushInternal-2"] += microtime() - st;
  VLOG_EVERY_N(1, 10000) << "quic::IOBufQuicBatch::flushInternal() PART 2"
                         << " tot = " << totElapsed["flushInternal-2"]
                         << " micros"
                         << (totElapsed["flushInternal-2"] = 0);
#endif
  if (!written) {
    // This can happen normally, so ignore for now. Now we treat EAGAIN same
    // as a loss to avoid looping.
    // TODO: Remove once we use write event from libevent.
    return false; // done
  }

  return true; // success, not done yet
}
} // namespace quic
