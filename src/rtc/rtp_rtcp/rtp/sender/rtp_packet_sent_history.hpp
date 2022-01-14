#ifndef _RTC_RTP_RTCP_RTP_PACKET_HISTORY_H_
#define _RTC_RTP_RTCP_RTP_PACKET_HISTORY_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_configurations.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

#include <optional>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketSentHistory {
public:
    // Maximum number of packets we ever allow in the history.
    static constexpr size_t kMaxCapacity = 9600;
    // Maximum number of entries in prioritized queue of padding packets.
    static constexpr size_t kMaxPaddingtHistory = 63;
    // Don't remove packets within max(1000ms, 3x RTT).
    static constexpr int64_t kMinPacketDurationMs = 1000;
    static constexpr int kMinPacketDurationRtt = 3;
    // With kStoreAndCull, always remove packets after 3x max(1000ms, 3x rtt).
    static constexpr int kPacketCullingDelayFactor = 3;

    enum class StorageMode {
        DISABLE,            // Don't store any packets
        STORE_AND_CULL,     // Store up to "number_to_store" packets, but try to remove
                            // packets as they time out or as signaled as received.
    };

    struct PacketState {
        PacketState();
        PacketState(const PacketState&);
        ~PacketState();

        uint16_t rtp_sequence_number = 0;
        std::optional<int64_t> send_time_ms;
        int64_t capture_time_ms = 0;
        uint32_t ssrc = 0;
        size_t packet_size = 0;
        // Number of times retransmitted, and not including the first transmission.
        size_t times_retransmitted = 0;
        bool pending_transmission = false;
    };

public:
    RtpPacketSentHistory(const RtpConfiguration& config);

    RtpPacketSentHistory() = delete;
    RtpPacketSentHistory(const RtpPacketSentHistory&) = delete;
    RtpPacketSentHistory& operator=(const RtpPacketSentHistory&) = delete;

    ~RtpPacketSentHistory();

    // Set/get storage mode. Note that setting the state will clear the history,
    // even if setting the same state as is currently used.
    void SetStorePacketsStatus(StorageMode mode, size_t number_to_store);
    StorageMode GetStorageMode() const;

    // Set RTT, used to avoid premature retransmission and to prevent over-writing
    // a packet in the history before we are reasonably sure it has been received.
    void SetRttMs(int64_t rtt_ms);

    // If |send_time| is set, packet was sent without using pacer, so state will
    // be set accordingly.
    void PutRtpPacket(std::shared_ptr<RtpPacketToSend> packet, std::optional<int64_t> send_time_ms);

    // Gets stored RTP packet corresponding to the input |sequence number|.
    // Returns nullptr if packet is not found or was (re)sent too recently.
    std::shared_ptr<RtpPacketToSend> GetPacketAndSetSendTime(uint16_t sequence_number);

    // Gets stored RTP packet corresponding to the input |sequence number|.
    // Returns nullptr if packet is not found or was (re)sent too recently.
    // If a packet copy is returned, it will be marked as pending transmission but
    // does not update send time, that must be done by MarkPacketAsSent().
    std::shared_ptr<RtpPacketToSend> GetPacketAndMarkAsPending(uint16_t sequence_number);

    // In addition to getting packet and marking as sent, this method takes an
    // encapsulator function that takes a reference to the packet and outputs a
    // copy that may be wrapped in a container, eg RTX.
    // If the the encapsulator returns nullptr, the retransmit is aborted and the
    // packet will not be marked as pending.
    std::shared_ptr<RtpPacketToSend> GetPacketAndMarkAsPending(uint16_t sequence_number, 
            std::function<std::shared_ptr<RtpPacketToSend>(std::shared_ptr<RtpPacketToSend>)> encapsulate);

    // Updates the send time for the given packet and increments the transmission
    // counter. Marks the packet as no longer being in the pacer queue.
    void MarkPacketAsSent(uint16_t sequence_number);

    // Similar to GetPacketAndSetSendTime(), but only returns a snapshot of the
    // current state for packet, and never updates internal state.
    std::optional<PacketState> GetPacketState(uint16_t sequence_number) const;

    // Get the packet (if any) from the history, that is deemed most likely to
    // the remote side. This is calculated from heuristics such as packet age
    // and times retransmitted. Updated the send time of the packet, so is not
    // a const method.
    std::shared_ptr<RtpPacketToSend> GetPayloadPaddingPacket();

    // Same as GetPayloadPaddingPacket(void), but adds an encapsulation
    // that can be used for instance to encapsulate the packet in an RTX
    // container, or to abort getting the packet if the function returns
    // nullptr.
    std::shared_ptr<RtpPacketToSend> GetPayloadPaddingPacket(std::function<std::shared_ptr<RtpPacketToSend>(std::shared_ptr<RtpPacketToSend>)> encapsulate);

    // Cull packets that have been acknowledged as received by the remote end.
    void CullAcknowledgedPackets(std::vector<const uint16_t> sequence_numbers);

    // Mark packet as queued for transmission. This will prevent premature
    // removal or duplicate retransmissions in the pacer queue.
    // Returns true if status was set, false if packet was not found.
    bool SetPendingTransmission(uint16_t sequence_number);

    // Remove all pending packets from the history, but keep storage mode and
    // capacity.
    void Clear();

private:
    struct StoredPacketCompare;
    class StoredPacket;
    using PacketPrioritySet = std::set<StoredPacket*, StoredPacketCompare>;

    // StoredPacket
    class StoredPacket {
    public:
        StoredPacket(std::shared_ptr<RtpPacketToSend> packet,
                    std::optional<int64_t> send_time_ms,
                    uint64_t insert_order);
        StoredPacket(StoredPacket&&);
        StoredPacket& operator=(StoredPacket&&);
        ~StoredPacket();

        uint64_t insert_order() const { return insert_order_; }
        size_t times_retransmitted() const { return times_retransmitted_; }
        void IncrementTimesRetransmitted(PacketPrioritySet* priority_set);

        // The time of last transmission, including retransmissions.
        std::optional<int64_t> send_time_ms_;

        // The actual packet.
        std::shared_ptr<RtpPacketToSend> packet_;

        // True if the packet is currently in the pacer queue pending transmission.
        bool pending_transmission_;

    private:
        // Unique number per StoredPacket, incremented by one for each added
        // packet. Used to sort on insert order.
        uint64_t insert_order_;

        // Number of times RE-transmitted, ie excluding the first transmission.
        size_t times_retransmitted_;
    };
    struct StoredPacketCompare {
        bool operator()(StoredPacket* lhs, StoredPacket* rhs) const;
    };

private:
    // Helper method used by GetPacketAndSetSendTime() and GetPacketState() to
    // check if packet has too recently been sent.
    bool VerifyRtt(const StoredPacket& packet, int64_t now_ms) const;
    void Reset();
    void CullOldPackets(int64_t now_ms);
    // Removes the packet from the history, and context/mapping that has been
    // stored. Returns the RTP packet instance contained within the StoredPacket.
    std::shared_ptr<RtpPacketToSend> RemovePacket(int packet_index);
    int GetPacketIndex(uint16_t sequence_number) const;
    StoredPacket* GetStoredPacket(uint16_t sequence_number);
    static PacketState StoredPacketToPacketState(const StoredPacket& stored_packet);

private:
    SequenceChecker sequence_checker_;

    Clock* const clock_;
    const bool enable_padding_prio_;
    size_t number_to_store_;
    StorageMode mode_;
    int64_t rtt_ms_;

    // Queue of stored packets, ordered by sequence number, with older packets in
    // the front and new packets being added to the back. Note that there may be
    // wrap-arounds so the back may have a lower sequence number.
    // Packets may also be removed out-of-order, in which case there will be
    // instances of StoredPacket with |packet_| set to nullptr. The first and last
    // entry in the queue will however always be populated.
    std::deque<StoredPacket> packet_history_;

    // Total number of packets with inserted.
    uint64_t packets_inserted_;

    // Objects from |packet_history_| ordered by "most likely to be useful", used
    // in GetPayloadPaddingPacket().
    PacketPrioritySet padding_priority_;
};
    
} // namespace naivertc


#endif