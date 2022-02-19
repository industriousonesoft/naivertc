#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_TRANSPORT_FEEDBACK_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_TRANSPORT_FEEDBACK_H_

#include "base/defines.hpp"
#include "rtc/base/units/time_delta.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/rtpfb.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"

namespace naivertc {
namespace rtcp {

class TransportFeedback : public Rtpfb {
public:
    // ReceivedPacket
    class ReceivedPacket {
    public:
        ReceivedPacket(uint16_t sequence_number, int16_t delta_ticks)
            : sequence_number_(sequence_number),
            delta_ticks_(delta_ticks),
            received_(true) {}
        explicit ReceivedPacket(uint16_t sequence_number)
            : sequence_number_(sequence_number), received_(false) {}
        ReceivedPacket(const ReceivedPacket&) = default;
        ReceivedPacket& operator=(const ReceivedPacket&) = default;

        uint16_t sequence_number() const { return sequence_number_; }
        int16_t delta_ticks() const { return delta_ticks_; }
        int32_t delta_us() const { return delta_ticks_ * kDeltaScaleFactor; }
        TimeDelta delta() const { return TimeDelta::Micros(delta_us()); }
        bool received() const { return received_; }

    private:
        uint16_t sequence_number_;
        int16_t delta_ticks_;
        bool received_;
    };

    static constexpr uint8_t kFeedbackMessageType = 15;
    // Convert to multiples of 0.25ms.
    static constexpr int kDeltaScaleFactor = 250 /*250 us*/;
    // Maximum number of packets (including missing) TransportFeedback can report.
    static constexpr size_t kMaxReportedPackets = 0xffff;

public:
    TransportFeedback();

    // If |include_timestamps| is set to false, the created packet will not
    // contain the receive delta block.
    explicit TransportFeedback(bool include_timestamps,
                               bool include_lost = false);
    TransportFeedback(const TransportFeedback&);
    TransportFeedback(TransportFeedback&&);

    ~TransportFeedback() override;

    void SetBase(uint16_t base_sequence,     // Seq# of first packet in this msg.
               int64_t ref_timestamp_us);  // Reference timestamp for this msg.
    void SetFeedbackSequenceNumber(uint8_t feedback_sequence);
    // NOTE: This method requires increasing sequence numbers (excepting wraps).
    bool AddReceivedPacket(uint16_t sequence_number, int64_t timestamp_us);
    const std::vector<ReceivedPacket>& GetReceivedPackets() const;
    const std::vector<ReceivedPacket>& GetAllPackets() const;

    uint16_t GetBaseSequence() const;

    // Returns number of packets (including missing) this feedback describes.
    size_t GetPacketStatusCount() const { return status_count_; }

    // Get the reference time in microseconds, including any precision loss.
    int64_t GetBaseTimeUs() const;
    TimeDelta GetBaseTime() const;

    // Get the unwrapped delta between current base time and |prev_timestamp_us|.
    int64_t GetBaseDeltaUs(int64_t prev_timestamp_us) const;
    TimeDelta GetBaseDelta(TimeDelta prev_timestamp) const;

    // Does the feedback packet contain timestamp information?
    bool IncludeTimestamps() const { return include_timestamps_; }

    bool Parse(const CommonHeader& packet);
    static std::unique_ptr<TransportFeedback> ParseFrom(const uint8_t* buffer,
                                                        size_t size);
    // Pre and postcondition for all public methods. Should always return true.
    // This function is for tests.
    bool IsConsistent() const;

    size_t PacketSize() const override;
    size_t PaddingSize() const;

    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;
private:
  // Size in bytes of a delta time in rtcp packet.
  // Valid values are 0 (packet wasn't received), 1 or 2.
  using DeltaSize = uint8_t;
  // Keeps DeltaSizes that can be encoded into single chunk if it is last chunk.
  class LastChunk {
    public:
        using DeltaSize = TransportFeedback::DeltaSize;

        LastChunk();

        bool Empty() const;
        void Clear();
        // Return if delta sizes still can be encoded into single chunk with added
        // |delta_size|.
        bool CanAdd(DeltaSize delta_size) const;
        // Add |delta_size|, assumes |CanAdd(delta_size)|,
        void Add(DeltaSize delta_size);

        // Encode chunk as large as possible removing encoded delta sizes.
        // Assume CanAdd() == false for some valid delta_size.
        uint16_t Emit();
        // Encode all stored delta_sizes into single chunk, pad with 0s if needed.
        uint16_t EncodeLast() const;

        // Decode up to |max_size| delta sizes from |chunk|.
        void Decode(uint16_t chunk, size_t max_size);
        // Appends content of the Lastchunk to |deltas|.
        void AppendTo(std::vector<DeltaSize>* deltas) const;

    private:
        static constexpr size_t kMaxRunLengthCapacity = 0x1fff;
        static constexpr size_t kMaxOneBitCapacity = 14;
        static constexpr size_t kMaxTwoBitCapacity = 7;
        static constexpr size_t kMaxVectorCapacity = kMaxOneBitCapacity;
        static constexpr DeltaSize kLarge = 2;

        uint16_t EncodeOneBit() const;
        void DecodeOneBit(uint16_t chunk, size_t max_size);

        uint16_t EncodeTwoBit(size_t size) const;
        void DecodeTwoBit(uint16_t chunk, size_t max_size);

        uint16_t EncodeRunLength() const;
        void DecodeRunLength(uint16_t chunk, size_t max_size);

        DeltaSize delta_sizes_[kMaxVectorCapacity];
        size_t size_;
        bool all_same_;
        bool has_large_delta_;
    };

    // Reset packet to consistent empty state.
    void Clear();
    bool AddDeltaSize(DeltaSize delta_size);

private:
    const bool include_lost_;
    uint16_t base_seq_num_;
    uint16_t status_count_;
    int32_t refernce_time_;
    uint8_t feedback_seq_;
    bool include_timestamps_;

    int64_t last_timestamp_us_;
    std::vector<ReceivedPacket> received_packets_;
    std::vector<ReceivedPacket> all_packets_;
    // All but last encoded packet chunks.
    std::vector<uint16_t> encoded_chunks_;
    LastChunk last_chunk_;
    size_t size_bytes_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif
