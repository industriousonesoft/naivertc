#include "rtc/congestion_control/pacing/round_robin_packet_queue.hpp"

namespace naivertc {
namespace {

constexpr size_t kMaxLeadingSize = 1400;

} // namespace

// RoundRobinPacketQueue
RoundRobinPacketQueue::RoundRobinPacketQueue(bool include_overhead, Timestamp start_time) 
    : include_overhead_(include_overhead),
      time_last_update_(start_time),
      max_stream_sent_size_(kMaxLeadingSize) {

}
   
RoundRobinPacketQueue::~RoundRobinPacketQueue() {
    while (!Empty()) {
        Pop();
    }
}

bool RoundRobinPacketQueue::Empty() const {
    if (num_packets_ == 0) {
        assert(!single_packet_queue_.has_value() && stream_priorities_.empty());
        return true;
    }
    assert(single_packet_queue_.has_value() || !stream_priorities_.empty());
    return false;
}

void RoundRobinPacketQueue::Push(int priority,
                                 Timestamp enqueue_time,
                                 uint64_t enqueue_order,
                                 RtpPacketToSend packet) {
    if (num_packets_ == 0) {
        // Single packet fast-path
        single_packet_queue_.emplace(QueuedPacket(priority, 
                                                  enqueue_time, 
                                                  enqueue_order, 
                                                  enqueue_times_.end(), 
                                                  std::move(packet)));
        UpdateEnqueueTime(enqueue_time);
        single_packet_queue_->SubtractPauseTime(pause_time_sum_);
        num_packets_ = 1;
        total_packet_size_ += PacketSize(*single_packet_queue_);
    } else {
        MaybePromoteSinglePacketToNormalQueue();
        Push(QueuedPacket(priority, 
                          enqueue_time, 
                          enqueue_order, 
                          enqueue_times_.insert(enqueue_time), 
                          std::move(packet)));
    } 
}

std::optional<RtpPacketToSend> RoundRobinPacketQueue::Pop() {
    if (single_packet_queue_) {
        RtpPacketToSend rtp_packet = std::move(single_packet_queue_->owned_packet);
        single_packet_queue_.reset();
        queue_time_sum_ = TimeDelta::Zero();
        num_packets_ = 0;
        total_packet_size_ = 0;
        return rtp_packet;
    }

    // Get the stream with highest priority.
    Stream* stream = GetHighestPriorityStream();
    // No stream found.
    if (stream == nullptr || IsEmptyStream(*stream)) {
        return std::nullopt;
    }
    
    // Get the packet with highest priority in the stream.
    const QueuedPacket& queued_packet = stream->packet_queue.top();

    // Calculate the total amount of time spent by this packet in the queue
    // while in a non-paused state. Note that the |pause_time_sum_| was
    // subtracted from |packet.enqueue_time| when the packet was pushed, and
    // by subtracting it now we effectively remove the time spent in in the
    // queue while in a paused state.
    TimeDelta delat_in_non_paused_state = time_last_update_ - queued_packet.enqueue_time - pause_time_sum_;
    queue_time_sum_ -= delat_in_non_paused_state;

    assert(queued_packet.enqueue_time_it != enqueue_times_.end());
    enqueue_times_.erase(queued_packet.enqueue_time_it);

    // Update |bytes| of this stream. The general idea is that the stream that
    // has sent the least amount of bytes should have the highest priority.
    // The problem with that is if streams send with different rates, in which
    // case a "budget" will be built up for the stream sending at the lower
    // bitrate. To avoid building a too large budget we limit |bytes| to be within
    // kMaxLeading bytes of the stream that has sent the most amount of bytes.
    size_t packet_size = PacketSize(queued_packet);
    // FIXME: How to understand this?
    stream->sent_size = std::max(stream->sent_size + packet_size, max_stream_sent_size_ - kMaxLeadingSize);
    max_stream_sent_size_ = std::max(max_stream_sent_size_, stream->sent_size);

    total_packet_size_ -= packet_size;
    num_packets_ -= 1;

    RtpPacketToSend rtp_packet = std::move(queued_packet.owned_packet);
    stream->packet_queue.pop();

    // Update priority after popping packet.
    stream_priorities_.erase(stream->priority_it);
    if (stream->packet_queue.empty()) {
        stream->priority_it = stream_priorities_.end();
    } else {
        // The highest priority of packet denotes the priority of stream.
        int priority = stream->packet_queue.top().priority;
        stream->priority_it = stream_priorities_.emplace(StreamPrioKey(priority, stream->sent_size), stream->ssrc);
    }
    return rtp_packet;
}

Timestamp RoundRobinPacketQueue::OldestEnqueueTime() const {
    if (single_packet_queue_) {
        return single_packet_queue_->enqueue_time;
    }
    if (stream_priorities_.empty()) {
        return Timestamp::MinusInfinity();
    }
    return *enqueue_times_.begin();
}

void RoundRobinPacketQueue::UpdateEnqueueTime(Timestamp now) {
    if (now <= time_last_update_) {
        return;
    }
    auto delta = now - time_last_update_;
    if (paused_) {
        pause_time_sum_ += delta;
    } else {
        // FIXME: How to understand this?
        queue_time_sum_ += TimeDelta::Micros(delta.us() * num_packets_);
    }
    time_last_update_ = now;
}

TimeDelta RoundRobinPacketQueue::AverageQueueTime() const {
    if (Empty()) {
        return TimeDelta::Zero();
    }
    return queue_time_sum_ / num_packets_;
}

std::optional<Timestamp> RoundRobinPacketQueue::LeadingAudioPacketEnqueueTime() const {
    if (Empty()) {
        return std::nullopt;
    }

    // Single packet mode
    if (single_packet_queue_) {
        if (single_packet_queue_->type() == RtpPacketType::AUDIO) {
            return single_packet_queue_->enqueue_time;
        } else {
            return std::nullopt;
        }
    }

    // Queue mode
    uint32_t ssrc = stream_priorities_.begin()->second;
    const auto& top_packet = streams_.find(ssrc)->second.packet_queue.top();
    if (top_packet.type() == RtpPacketType::AUDIO) {
        return top_packet.enqueue_time;
    }
    return std::nullopt;
}

// Private methods
void RoundRobinPacketQueue::Push(QueuedPacket packet) {
    auto stream_it = streams_.find(packet.ssrc());
    if (stream_it == streams_.end()) {
        stream_it = streams_.emplace(packet.ssrc(), Stream()).first;
        stream_it->second.priority_it = stream_priorities_.end();
        stream_it->second.ssrc = packet.ssrc();
    }

    Stream& stream = stream_it->second;

    if (stream.priority_it == stream_priorities_.end()) {
        // If the SSRC is not scheduled, add it to |stream_priorities_|.
        stream.priority_it = stream_priorities_.emplace(StreamPrioKey(packet.priority, stream.sent_size), packet.ssrc());
    } else {
        // If the priority of this SSRC increased, remove the outdated StreamPrioKey
        // and insert a new one with the new priority. Note that |priority_| uses
        // lower ordinal for higher priority.
        stream_priorities_.erase(stream.priority_it);
        stream.priority_it = stream_priorities_.emplace(StreamPrioKey(packet.priority, stream.sent_size), packet.ssrc());
    }

    if (packet.enqueue_time_it == enqueue_times_.end()) {
        packet.enqueue_time_it = enqueue_times_.insert(packet.enqueue_time);
    } else {
        // In order to figure out how much time a packet has spent in the queue
        // while not in a paused state, we subtract the total amount of time the
        // queue has been paused so far, and when the packet is popped we subtract
        // the total amount of time the queue has been paused at that moment. This
        // way we subtract the total amount of time the packet has spent in the
        // queue while in a paused state.
        UpdateEnqueueTime(packet.enqueue_time);
        packet.SubtractPauseTime(pause_time_sum_);

        num_packets_ += 1;
        total_packet_size_ += PacketSize(packet);
    }

    stream.packet_queue.push(std::move(packet));
}

size_t RoundRobinPacketQueue::PacketSize(const QueuedPacket& packet) const {
    size_t packet_size = packet.owned_packet.payload_size() + packet.owned_packet.padding_size();
    if (include_overhead_) {
        packet_size += packet.owned_packet.header_size() + transport_overhead_;
    }
    return packet_size;
}

void RoundRobinPacketQueue::MaybePromoteSinglePacketToNormalQueue() {
    if (single_packet_queue_) {
        Push(std::move(single_packet_queue_.value()));
        single_packet_queue_.reset();
    }
}

RoundRobinPacketQueue::Stream* RoundRobinPacketQueue::GetHighestPriorityStream() {
    if (stream_priorities_.empty()) {
        return nullptr;
    }
    uint32_t ssrc = stream_priorities_.begin()->second;

    auto stream_it = streams_.find(ssrc);
    if (stream_it != streams_.end() &&
        stream_it->second.priority_it == stream_priorities_.begin() &&
        !stream_it->second.packet_queue.empty()) {
        return &stream_it->second;
    }
    return nullptr;
}

bool RoundRobinPacketQueue::IsEmptyStream(const Stream& stream) const {
    if (stream.packet_queue.empty()) {
        assert(stream.priority_it == stream_priorities_.end());
        return true;
    }
    assert(stream.priority_it != stream_priorities_.end());
    return false;
}
    
} // namespace naivertc
