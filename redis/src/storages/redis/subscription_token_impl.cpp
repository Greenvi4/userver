#include "subscription_token_impl.hpp"

#include <stdexcept>

#include <userver/engine/task/task_with_result.hpp>
#include <userver/tracing/span.hpp>
#include <userver/utils/async.hpp>

namespace storages::redis {
namespace {

const std::string kSubscribeToChannelPrefix = "redis-channel-subscriber-";
const std::string kSubscribeToPatternPrefix = "redis-pattern-subscriber-";
const std::string kProcessRedisSubscriptionMessage =
    "process redis subscription message";

}  // namespace

SubscriptionTokenImpl::SubscriptionTokenImpl(
    ::redis::SubscribeSentinel& subscribe_sentinel, std::string channel,
    OnMessageCb on_message_cb, const ::redis::CommandControl& command_control)
    : channel_(std::move(channel)),
      queue_(std::make_unique<SubscriptionQueue<ChannelSubscriptionQueueItem>>(
          subscribe_sentinel, channel_, command_control)),
      on_message_cb_(std::move(on_message_cb)),
      subscriber_task_(utils::Async(kSubscribeToChannelPrefix + channel_,
                                    [this] { ProcessMessages(); })) {}

SubscriptionTokenImpl::~SubscriptionTokenImpl() { Unsubscribe(); }

void SubscriptionTokenImpl::SetMaxQueueLength(size_t length) {
  queue_->SetMaxLength(length);
}

void SubscriptionTokenImpl::Unsubscribe() {
  queue_->Unsubscribe();
  subscriber_task_.SyncCancel();
}

void SubscriptionTokenImpl::ProcessMessages() {
  std::unique_ptr<ChannelSubscriptionQueueItem> msg;
  while (queue_->PopMessage(msg)) {
    tracing::Span span(kProcessRedisSubscriptionMessage);
    if (on_message_cb_) on_message_cb_(channel_, msg->message);
  }
}

PsubscriptionTokenImpl::PsubscriptionTokenImpl(
    ::redis::SubscribeSentinel& subscribe_sentinel, std::string pattern,
    OnPmessageCb on_pmessage_cb, const ::redis::CommandControl& command_control)
    : pattern_(std::move(pattern)),
      queue_(std::make_unique<SubscriptionQueue<PatternSubscriptionQueueItem>>(
          subscribe_sentinel, pattern_, command_control)),
      on_pmessage_cb_(std::move(on_pmessage_cb)),
      subscriber_task_(utils::Async(kSubscribeToPatternPrefix + pattern_,
                                    [this] { ProcessMessages(); })) {}

PsubscriptionTokenImpl::~PsubscriptionTokenImpl() { Unsubscribe(); }

void PsubscriptionTokenImpl::SetMaxQueueLength(size_t length) {
  queue_->SetMaxLength(length);
}

void PsubscriptionTokenImpl::Unsubscribe() {
  queue_->Unsubscribe();
  subscriber_task_.SyncCancel();
}

void PsubscriptionTokenImpl::ProcessMessages() {
  std::unique_ptr<PatternSubscriptionQueueItem> msg;
  while (queue_->PopMessage(msg)) {
    tracing::Span span(kProcessRedisSubscriptionMessage);
    if (on_pmessage_cb_) on_pmessage_cb_(pattern_, msg->channel, msg->message);
  }
}

}  // namespace storages::redis
