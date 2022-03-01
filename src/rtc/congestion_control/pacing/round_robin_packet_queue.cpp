#include "rtc/congestion_control/pacing/round_robin_packet_queue.hpp"

namespace naivertc {

// QueuedPacket
RoundRobinPacketQueue::QueuedPacket::QueuedPacket(int priority,
                                                  Timestamp enqueue_time,
                                                  uint64_t enqueue_order,
                                                  std::multiset<Timestamp>::iterator enqueue_time_it,
                                                  std::shared_ptr<RtpPacketToSend> packet)
        : priority_(priority),
          enqueue_time_(enqueue_time),
          enqueue_order_(enqueue_order),
          enqueue_time_it_(enqueue_time_it),
          owned_packet_(packet) {}

RoundRobinPacketQueue::QueuedPacket::QueuedPacket(const QueuedPacket& rhs) = default;
RoundRobinPacketQueue::QueuedPacket::~QueuedPacket() = default;

bool RoundRobinPacketQueue::QueuedPacket::operator<(const RoundRobinPacketQueue::QueuedPacket& other) const {
    // Compare priority first, and the smaller value has higher priority
    // A lower number to denote a higher priority.
    if (priority_ != other.priority_)
        return priority_ > other.priority_;
    // Retansmisson packet should be sent later
    if (is_retransmission() != other.is_retransmission())
        return other.is_retransmission();

    return enqueue_order_ > other.enqueue_order_;
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

// Stream
RoundRobinPacketQueue::Stream::Stream() : ssrc(0), size(0) {}
RoundRobinPacketQueue::Stream::Stream(const Stream& stream) = default;
RoundRobinPacketQueue::Stream::~Stream() = default;

// RoundRobinPacketQueue
RoundRobinPacketQueue::RoundRobinPacketQueue(Timestamp start_time) 
    : last_updated_time_(start_time),
      packet_count_(0) {

}
   
RoundRobinPacketQueue::~RoundRobinPacketQueue() {

}

void RoundRobinPacketQueue::Push(int priority,
                                 Timestamp enqueue_time,
                                 uint64_t enqueue_order,
                                 std::shared_ptr<RtpPacketToSend> packet) {
    if (packet_count_ == 0) {

    } else {

    }
}

std::shared_ptr<RtpPacketToSend> RoundRobinPacketQueue::Pop() {
    return nullptr;
}

// Private methods
void RoundRobinPacketQueue::Push(std::shared_ptr<QueuedPacket> packet) {

}
    
} // namespace naivertc
