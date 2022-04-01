/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/common/FunctionLooper.h>

#include <folly/ScopeGuard.h>

#if PROFILING_ENABLED
namespace {
std::unordered_map<std::string, uint64_t> totElapsed;
}
#endif
namespace quic {
using namespace std::chrono_literals;

FunctionLooper::FunctionLooper(
    folly::EventBase* evb,
    folly::Function<void(bool)>&& func,
    LooperType type)
    : evb_(evb), func_(std::move(func)), type_(type) {}

void FunctionLooper::setPacingTimer(
    TimerHighRes::SharedPtr pacingTimer) noexcept {
  pacingTimer_ = std::move(pacingTimer);
}

bool FunctionLooper::hasPacingTimer() const noexcept {
  return pacingTimer_ != nullptr;
}

void FunctionLooper::setPacingFunction(
    folly::Function<std::chrono::microseconds()>&& pacingFunc) {
  pacingFunc_ = std::move(pacingFunc);
}

void FunctionLooper::commonLoopBody(bool fromTimer) noexcept {
#if PROFILING_ENABLED
  uint64_t st = microtime();
#endif
  inLoopBody_ = true;
  SCOPE_EXIT {
    inLoopBody_ = false;
  };
  auto hasBeenRunning = running_;
#if PROFILING_ENABLED
  totElapsed["commonLoopBody-1"] += microtime() - st;
  VLOG_EVERY_N(1, 100000) << "quic::FunctionLooper::commonLoopBody() PART 1"
                          << " tot = "
                          << totElapsed["commonLoopBody-1"]
                          << " micros"
                          << (totElapsed["commonLoopBody-1"] = 0);
#endif
  func_(fromTimer);
#if PROFILING_ENABLED
  st = microtime();
#endif
  // callback could cause us to stop ourselves.
  // Someone could have also called run() in the callback.
  VLOG(10) << __func__ << ": " << type_ << " fromTimer=" << fromTimer
           << " hasBeenRunning=" << hasBeenRunning << " running_=" << running_;
  if (!running_) {
    return;
  }
  if (!schedulePacingTimeout(fromTimer)) {
    evb_->runInLoop(this);
  }
#if PROFILING_ENABLED
  totElapsed["commonLoopBody-2"] += microtime() - st;
  VLOG_EVERY_N(1, 100000) << "quic::FunctionLooper::commonLoopBody() PART 2"
                          << " tot = "
                          << totElapsed["commonLoopBody-2"]
                          << " micros"
                          << (totElapsed["commonLoopBody-2"] = 0);
#endif
}

bool FunctionLooper::schedulePacingTimeout(bool /* fromTimer */) noexcept {
  if (pacingFunc_ && pacingTimer_ && !isScheduled()) {
    auto nextPacingTime = (*pacingFunc_)();
    if (nextPacingTime != 0us) {
      pacingTimer_->scheduleTimeout(this, nextPacingTime);
      return true;
    }
  }
  return false;
}

void FunctionLooper::runLoopCallback() noexcept {
#if PROFILING_ENABLED
  uint64_t st = microtime();
#endif
  folly::DelayedDestruction::DestructorGuard dg(this);
#if PROFILING_ENABLED
  totElapsed["runLoopCallback"] += microtime() - st;
  VLOG_EVERY_N(1, 100000) << "quic::FunctionLooper::runLoopCallback()"
                          << " tot = "
                          << totElapsed["runLoopCallback"]
                          << " micros"
                          << (totElapsed["runLoopCallback"] = 0);
#endif
  commonLoopBody(false);
}

void FunctionLooper::run(bool thisIteration) noexcept {
  VLOG(10) << __func__ << ": " << type_;
  running_ = true;
  // Caller can call run() in func_. But if we are in pacing mode, we should
  // prevent such loop.
  if (pacingTimer_ && inLoopBody_) {
    VLOG(4) << __func__ << ": " << type_
            << " in loop body and using pacing - not rescheduling";
    return;
  }
  if (isLoopCallbackScheduled() || isScheduled()) {
    VLOG(10) << __func__ << ": " << type_ << " already scheduled";
    return;
  }
  evb_->runInLoop(this, thisIteration);
}

void FunctionLooper::stop() noexcept {
  VLOG(10) << __func__ << ": " << type_;
  running_ = false;
  cancelLoopCallback();
  cancelTimeout();
}

bool FunctionLooper::isRunning() const {
  return running_;
}

void FunctionLooper::attachEventBase(folly::EventBase* evb) {
  VLOG(10) << __func__ << ": " << type_;
  DCHECK(!evb_);
  DCHECK(evb && evb->isInEventBaseThread());
  evb_ = evb;
}

void FunctionLooper::detachEventBase() {
  VLOG(10) << __func__ << ": " << type_;
  DCHECK(evb_ && evb_->isInEventBaseThread());
  stop();
  cancelTimeout();
  evb_ = nullptr;
}

void FunctionLooper::timeoutExpired() noexcept {
  folly::DelayedDestruction::DestructorGuard dg(this);
  commonLoopBody(true);
}

void FunctionLooper::callbackCanceled() noexcept {
  return;
}

folly::Optional<std::chrono::microseconds>
FunctionLooper::getTimerTickInterval() noexcept {
  if (pacingTimer_) {
    return pacingTimer_->getTickInterval();
  }
  return folly::none;
}

std::ostream& operator<<(std::ostream& out, const LooperType& rhs) {
  switch (rhs) {
    case LooperType::ReadLooper:
      out << "ReadLooper";
      break;
    case LooperType::PeekLooper:
      out << "PeekLooper";
      break;
    case LooperType::WriteLooper:
      out << "WriteLooper";
      break;
    default:
      out << "unknown";
      break;
  }
  return out;
}
} // namespace quic
