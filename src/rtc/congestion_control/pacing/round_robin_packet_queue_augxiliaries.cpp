#include "rtc/congestion_control/pacing/round_robin_packet_queue.hpp"

namespace naivertc {

// QueuedPacket
RoundRobinPacketQueue::QueuedPacket::QueuedPacket(int priority,
                                                  Timestamp enqueue_time,
                                                  uint64_t enqueue_order,
                                                  std::multiset<Timestamp>::iterator enqueue_time_it,
                                                  RtpPacketToSend packet)
    : priority(priority),
      enqueue_time(enqueue_time),
      enqueue_order(enqueue_order),
      enqueue_time_it(enqueue_time_it),
      owned_packet(std::move(packet)) {}

RoundRobinPacketQueue::QueuedPacket::QueuedPacket(const QueuedPacket& rhs) = default;
RoundRobinPacketQueue::QueuedPacket::~QueuedPacket() = default;

void RoundRobinPacketQueue::QueuedPacket::SubtractPauseTime(TimeDelta pause_time_sum) {
    enqueue_time -= pause_time_sum;
}

bool RoundRobinPacketQueue::QueuedPacket::operator<(const RoundRobinPacketQueue::QueuedPacket& other) const {
    // Lower number takes priority over higher.
    if (priority != other.priority)
        return other.priority < priority;
    // Send retransimission before new media.
    if (is_retransmission() != other.is_retransmission())
        return other.is_retransmission();

    // Send the early packe first.
    return other.enqueue_order < enqueue_order;
}

// RoundRobinPacketQueue
RoundRobinPacketQueue::PriorityPacketQueue::const_iterator
RoundRobinPacketQueue::PriorityPacketQueue::begin() const {
    return c.begin();
}

RoundRobinPacketQueue::PriorityPacketQueue::const_iterator
RoundRobinPacketQueue::PriorityPacketQueue::end() const {
    return c.end();
}

// StreamPrioKey
RoundRobinPacketQueue::StreamPrioKey::StreamPrioKey(int priority, size_t sent_size) 
    : priority(priority), sent_size(sent_size) {}

bool RoundRobinPacketQueue::StreamPrioKey::operator<(const StreamPrioKey& other) const {
    // Lower number takes priority over higher.
    if (priority != other.priority) {
        return priority < other.priority;
    }
    // Smaller size takes priority over larger.
    return sent_size < other.sent_size;
}
        
// Stream
RoundRobinPacketQueue::Stream::Stream() : ssrc(0), sent_size(0) {}
RoundRobinPacketQueue::Stream::Stream(const Stream& stream) = default;
RoundRobinPacketQueue::Stream::~Stream() = default;
    
} // namespace naivertc
