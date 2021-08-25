#ifndef _RTC_RTP_RTCP_PACING_ROUND_ROBIN_PACKET_QUEUE_H_
#define _RTC_RTP_RTCP_PACING_ROUND_ROBIN_PACKET_QUEUE_H_

#include "base/defines.hpp"
#include "rtc/base/timestamp.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <set>
#include <map>
#include <queue>
#include <optional>

namespace naivertc {

using DataSize = uint64_t;
    
class RTC_CPP_EXPORT RoundRobinPacketQueue {
public:
    RoundRobinPacketQueue(Timestamp start_time);
    ~RoundRobinPacketQueue();

    void Push(int priority,
              Timestamp enqueue_time,
              uint64_t enqueue_order,
              std::shared_ptr<RtpPacketToSend> packet);

    std::shared_ptr<RtpPacketToSend> Pop();

private:
    // QueuedPacket
    struct QueuedPacket {
    public:
        QueuedPacket(int priority,
                     Timestamp enqueue_time,
                     uint64_t enqueue_order,
                     std::multiset<Timestamp>::iterator enqueue_time_it,
                     std::shared_ptr<RtpPacketToSend> packet);
        QueuedPacket(const QueuedPacket& rhs);
        ~QueuedPacket();

        int priority() const { return priority_; }
        RtpPacketType type() const { return owned_packet_->packet_type(); }
        uint32_t ssrc() const { return owned_packet_->ssrc(); }
        Timestamp enqueue_time() const { return enqueue_time_; }
        bool is_retransmission() const { return type() == RtpPacketType::RETRANSMISSION; }
        int64_t enqueue_order() const { return enqueue_order_; }
        std::shared_ptr<RtpPacketToSend> owned_packet() const { return owned_packet_; }

        std::multiset<Timestamp>::iterator enqueue_time_iterator() const { return enqueue_time_it_; }
        void set_enqueue_time_iterator(std::multiset<Timestamp>::iterator it) { enqueue_time_it_ = it; }

        bool operator<(const QueuedPacket& other) const;

    private:
        int priority_;
        Timestamp enqueue_time_;
        uint64_t enqueue_order_;
        std::multiset<Timestamp>::iterator enqueue_time_it_;
        std::shared_ptr<RtpPacketToSend> owned_packet_;
    };

    // PriorityPacketQueue
    class PriorityPacketQueue : public std::priority_queue<std::shared_ptr<QueuedPacket>> {
    public:
        using const_iterator = container_type::const_iterator;
        const_iterator begin() const;
        const_iterator end() const;
    };

    // StreamPriority 
    struct StreamPriority {
    public:
        StreamPriority(int priority, DataSize size) 
            : priority(priority), size(size) {}

        bool operator<(const StreamPriority& other) const {
            // FIXMED: 为什么此处和QueuedPacket中priority的比较方式不同？
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return size < other.size;
        }
        
        const int priority;
        const DataSize size;
    };

    // Stream
    struct Stream {
        Stream();
        Stream(const Stream&);
        ~Stream();

        uint32_t ssrc;
        DataSize size;
        PriorityPacketQueue packet_queue;
        // Whenever a packet is inserted for this stream we check if |priority_it|
        // points to an element in |stream_priorities_|, and if it does it means
        // this stream has already been scheduled, and if the scheduled priority is
        // lower than the priority of the incoming packet we reschedule this stream
        // with the higher priority.
        std::multimap<StreamPriority, uint32_t>::iterator priority_it;
    };

private:
    void Push(std::shared_ptr<QueuedPacket> packet);

private:
    Timestamp last_updated_time_;
    size_t packet_count_;

    std::optional<QueuedPacket> single_packet_queue_;
};

} // namespace naivertc

#endif