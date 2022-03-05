#ifndef _RTC_CONGESTION_CONTROL_PACING_ROUND_ROBIN_PACKET_QUEUE_H_
#define _RTC_CONGESTION_CONTROL_PACING_ROUND_ROBIN_PACKET_QUEUE_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"

#include <set>
#include <map>
#include <queue>
#include <optional>
#include <unordered_map>

namespace naivertc {

class RoundRobinPacketQueue {
public:
    RoundRobinPacketQueue(Timestamp start_time);
    ~RoundRobinPacketQueue();

    bool include_overhead() const;
    void set_include_overhead();

    size_t transport_overhead() const;
    void set_transport_overhead(size_t overhead_per_packet);

    size_t num_packets() const { return num_packets_; }
    size_t packet_size() const { return total_packet_size_; }

    bool Empty() const;

    void Push(int priority,
              Timestamp enqueue_time,
              uint64_t enqueue_order,
              RtpPacketToSend packet);

    std::optional<RtpPacketToSend> Pop();

    Timestamp OldestEnqueueTime() const;
    void UpdateEnqueueTime(Timestamp now);

    TimeDelta AverageQueueTime() const;

    std::optional<Timestamp> LeadingAudioPacketEnqueueTime() const;

private:
    struct QueuedPacket;
    struct Stream;

    void Push(QueuedPacket packet);

    size_t PacketSize(const QueuedPacket& packet) const;

    void MaybePromoteSinglePacketToNormalQueue();

    Stream* GetHighestPriorityStream();
    bool IsEmptyStream(const Stream& stream) const;

private:
    // QueuedPacket
    struct QueuedPacket {

        int priority;
        Timestamp enqueue_time;
        uint64_t enqueue_order;
        std::multiset<Timestamp>::iterator enqueue_time_it;
        RtpPacketToSend owned_packet;

        QueuedPacket(int priority,
                     Timestamp enqueue_time,
                     uint64_t enqueue_order,
                     std::multiset<Timestamp>::iterator enqueue_time_it,
                     RtpPacketToSend packet);
        QueuedPacket(const QueuedPacket& rhs);
        ~QueuedPacket();

        RtpPacketType type() const { return owned_packet.packet_type(); }
        uint32_t ssrc() const { return owned_packet.ssrc(); }
        bool is_retransmission() const { return type() == RtpPacketType::RETRANSMISSION; }
      
        void SubtractPauseTime(TimeDelta pause_time_sum);
        // std::priority_queue的特点是让优先级高的排在队列前面，优先出队。
        // Return true if the other has higher priority.
        bool operator<(const QueuedPacket& other) const;

    };

    // PriorityPacketQueue
    class PriorityPacketQueue : public std::priority_queue<QueuedPacket> {
    public:
        using const_iterator = container_type::const_iterator;
        const_iterator begin() const;
        const_iterator end() const;
    };

    // StreamPrioKey 
    struct StreamPrioKey {
        const int priority;
        const size_t sent_size;

        StreamPrioKey(int priority, size_t sent_size);
        bool operator<(const StreamPrioKey& other) const;
    };

    // Stream
    struct Stream {
        Stream();
        Stream(const Stream&);
        ~Stream();

        uint32_t ssrc;
        size_t sent_size;
        PriorityPacketQueue packet_queue;
        // Whenever a packet is inserted for this stream we check if |priority_it|
        // points to an element in |stream_priorities_|, and if it does it means
        // this stream has already been scheduled, and if the scheduled priority is
        // lower than the priority of the incoming packet we reschedule this stream
        // with the higher priority.
        std::multimap<StreamPrioKey, uint32_t>::iterator priority_it;
    };

private:
    Timestamp time_last_update_;
    size_t max_stream_sent_size_;
    bool paused_ = false;
    size_t num_packets_ = 0;
    // The total size of all packets in streams.
    size_t total_packet_size_ = 0;
    TimeDelta queue_time_sum_ = TimeDelta::Zero();
    TimeDelta pause_time_sum_ = TimeDelta::Zero();

    bool include_overhead_ = false;
    size_t transport_overhead_ = 0;

    // A map of streams used to prioritize from which stream to send next. We use
    // a multimap instead of a priority_queue since the priority of a stream can
    // change as a new packet is inserted, and a multimap allows us to remove and
    // then reinsert a StreamPrioKey if the priority has increased.
    std::multimap<StreamPrioKey, uint32_t> stream_priorities_;

    // A map of SSRCs to Streams.
    std::unordered_map<uint32_t, Stream> streams_;

    // The enqueue time of every packet currently in the queue. Used to figure out
    // the age of the oldest packet in the queue.
    std::multiset<Timestamp> enqueue_times_;

    std::optional<QueuedPacket> single_packet_queue_;
};

} // namespace naivertc

#endif