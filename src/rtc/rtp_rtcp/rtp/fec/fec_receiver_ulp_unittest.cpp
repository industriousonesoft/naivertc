#include "rtc/rtp_rtcp/rtp/fec/fec_receiver_ulp.hpp"
#include "rtc/base/time/clock_simulated.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_encoder.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_test_helper.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace naivertc {
namespace test {
namespace {

using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

constexpr uint8_t kFecPayloadType = 96;
constexpr uint8_t kRedPayloadType = 97;
constexpr uint8_t kVp8PayloadType = 120;

constexpr uint32_t kMediaSsrc = 835424;
    
} // namespace

// MockRecoveredPacketReceiver
class MockRecoveredPacketReceiver : public RecoveredPacketReceiver {
public:
    MOCK_METHOD(void, 
                OnRecoveredPacket, 
                (CopyOnWriteBuffer recovered_packet), 
                (override));
};

// UlpFecReceiverTest
class UlpFecReceiverTest : public ::testing::Test {
protected:
    UlpFecReceiverTest() 
        : clock_(std::make_shared<SimulatedClock>(0x100)),
          fec_receiver_(std::make_unique<UlpFecReceiver>(kMediaSsrc, clock_, &recovered_packet_receiver_)),
          fec_encoder_(FecEncoder::CreateUlpFecEncoder()),
          generated_fec_packets_(fec_encoder_->MaxFecPackets()),
          num_generated_fec_packets_(0),
          packet_generator_(kMediaSsrc, kVp8PayloadType, kFecPayloadType, kRedPayloadType) {}

    void EncoderFec(const FecEncoder::PacketList& media_packets, size_t num_fec_packet);

    void PacketizeFrame(size_t num_media_packets, FecEncoder::PacketList& media_packets);

    void BuildAndAddRedMediaPacket(const RtpPacket& rtp_packet, bool is_recovered = false);

    void BuildAndAddRedFecPacket(const CopyOnWriteBuffer& fec_packets);

    void VerifyRecoveredMediaPacket(const RtpPacket& packet, size_t call_times);

    template<typename T>
    void InjectGarbageData(size_t offset, T data);

protected:
    std::shared_ptr<SimulatedClock> clock_;
    MockRecoveredPacketReceiver recovered_packet_receiver_;
    std::unique_ptr<UlpFecReceiver> fec_receiver_;
    std::unique_ptr<FecEncoder> fec_encoder_;
    FecEncoder::FecPacketList generated_fec_packets_;
    size_t num_generated_fec_packets_;
    UlpFecPacketGenerator packet_generator_;
};

// Implements
void UlpFecReceiverTest::EncoderFec(const FecEncoder::PacketList& media_packets, 
                                    size_t num_fec_packets) {
    const uint8_t protection_factor = num_fec_packets * 255 / media_packets.size();
    // Unequal protection is turned off, and the number of important
    // packets is thus irrelevant.
    constexpr int kNumImportantPackets = 0;
    constexpr bool kUseUnequalProtection = false;
    constexpr FecMaskType kFecMaskType = FecMaskType::BURSTY;
    num_generated_fec_packets_ = fec_encoder_->Encode(media_packets,
                                                      protection_factor,
                                                      kNumImportantPackets,
                                                      kUseUnequalProtection,
                                                      kFecMaskType,
                                                      generated_fec_packets_);
    ASSERT_EQ(num_generated_fec_packets_, num_fec_packets);
}

void UlpFecReceiverTest::PacketizeFrame(size_t num_media_packets, FecEncoder::PacketList& media_packets) {
    packet_generator_.NewFrame(num_media_packets);
    for (size_t i = 0; i < num_media_packets; ++i) {
        RtpPacket rtp_packet = packet_generator_.NextRtpPacket(10 /* payload_size */ , 0/* padding_size */);
        media_packets.push_back(std::make_shared<RtpPacket>(std::move(rtp_packet)));
    }
}

void UlpFecReceiverTest::BuildAndAddRedMediaPacket(const RtpPacket& rtp_packet, bool is_recovered) {
    RtpPacketReceived red_packet = packet_generator_.BuildMediaRedPacket(rtp_packet, is_recovered);
    EXPECT_TRUE(fec_receiver_->AddReceivedRedPacket(red_packet, kFecPayloadType));
} 

void UlpFecReceiverTest::BuildAndAddRedFecPacket(const CopyOnWriteBuffer& fec_packets) {
    RtpPacketReceived red_packet = packet_generator_.BuildUlpFecRedPacket(fec_packets);
    EXPECT_TRUE(fec_receiver_->AddReceivedRedPacket(red_packet, kFecPayloadType));
}

void UlpFecReceiverTest::VerifyRecoveredMediaPacket(const RtpPacket& packet, size_t call_times) {
    // Verify that the content of the reconstructed packet is equal to the
    // content of |packet|, and that the same content is received |times| number
    // of times in a row.
    // NOTE: `EXPECT_CALL` MUST be called before the `OnRecoveredPacket` which is the real call. 
    EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(packet)).Times(call_times);
}

template<typename T>
void UlpFecReceiverTest::InjectGarbageData(size_t offset, T data) {
    const size_t kNumMediaPackets = 2u;
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(kNumMediaPackets, media_packets);
    // Encode to FEC packets.
    EncoderFec(media_packets, kNumFecPackets);
    EXPECT_EQ(kNumFecPackets, num_generated_fec_packets_);

    // Insert garbage bytes
    auto fec_it = generated_fec_packets_.begin();
    ByteWriter<T>::WriteBigEndian(fec_it->data() + offset, data);

    // Try to recovery
    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(0u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(-1, fec_packet_counter.first_packet_arrival_time_ms);

    auto media_it = media_packets.begin();
    // the received meida packet will be sent to VCM.
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    // `VerifyRecoveredMediaPacket` MUST be called before `BuildAndAddRedMediaPacket`.
    BuildAndAddRedMediaPacket(**media_it);

    // Drop one media packet
    ++media_it;
    // Failed to recover media packet from a invalid FEC packet.
    VerifyRecoveredMediaPacket(**media_it, 0 /* call_times */);
    BuildAndAddRedFecPacket(*fec_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(2u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);
}

// Test
TEST_F(UlpFecReceiverTest, TwoMediaOneFec) {
    const size_t kNumMediaPackets = 2u;
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(kNumMediaPackets, media_packets);
    // Encode to FEC packets.
    EncoderFec(media_packets, kNumFecPackets);
    EXPECT_EQ(kNumFecPackets, num_generated_fec_packets_);

    // Try to recovery.
    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(0u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(-1, fec_packet_counter.first_packet_arrival_time_ms);

    auto media_it = media_packets.begin();
    // the received meida packet will be sent to VCM.
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    // `VerifyRecoveredMediaPacket` MUST be called before `BuildAndAddRedMediaPacket`.
    BuildAndAddRedMediaPacket(**media_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(1u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);
    const int64_t first_packet_arrival_time_ms = fec_packet_counter.first_packet_arrival_time_ms;
    EXPECT_NE(-1, first_packet_arrival_time_ms);

    // Drop one media packet
    ++media_it;
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);

    auto fec_it = generated_fec_packets_.begin();
    BuildAndAddRedFecPacket(*fec_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(2u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_recovered_packets);
    EXPECT_EQ(first_packet_arrival_time_ms, fec_packet_counter.first_packet_arrival_time_ms);
}

TEST_F(UlpFecReceiverTest, TwoMediaOneFecNotUsesRecoveredPackets) {
    const size_t kNumMediaPackets = 2u;
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(kNumMediaPackets, media_packets);
    // Encode to FEC packets.
    EncoderFec(media_packets, kNumFecPackets);
    EXPECT_EQ(kNumFecPackets, num_generated_fec_packets_);

    // Try to recovery
    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(0u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(-1, fec_packet_counter.first_packet_arrival_time_ms);

    auto media_it = media_packets.begin();
    // the received meida packet will be sent to VCM.
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    // `VerifyRecoveredMediaPacket` MUST be called before `BuildAndAddRedMediaPacket`.
    BuildAndAddRedMediaPacket(**media_it, true /* is_recovered */);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(1u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);
    const int64_t first_packet_arrival_time_ms = fec_packet_counter.first_packet_arrival_time_ms;
    EXPECT_NE(-1, first_packet_arrival_time_ms);

    // Drop one media packet
    ++media_it;
    VerifyRecoveredMediaPacket(**media_it, 0 /* call_times */);

    auto fec_it = generated_fec_packets_.begin();
    BuildAndAddRedFecPacket(*fec_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(2u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);
    EXPECT_EQ(first_packet_arrival_time_ms, fec_packet_counter.first_packet_arrival_time_ms);
}

TEST_F(UlpFecReceiverTest, InjectGarbageFecHeaderLengthRecovery) {
    // Byte offset 8 is the 'length recovery' field of the FEC header.
    InjectGarbageData(8, 0x4711);
}

TEST_F(UlpFecReceiverTest, InjectGarbageFecLevelHeaderProtectionLength) {
    // Byte offset 10 is the 'protection length' field in the first FEC level
    // header.
    InjectGarbageData(10, 0x4711);
}

TEST_F(UlpFecReceiverTest, TwoMediaTwoFec) {
    const size_t kNumMediaPackets = 2u;
    const size_t kNumFecPackets = 2u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(kNumMediaPackets, media_packets);
    // Encode to FEC packets: unequal and bursty
    // Used a fixed packet mask:
    // #define kMaskBursty2_2 \
        0x80, 0x00, \
        0xc0, 0x00
    // The first FEC packet only protect the first media packet,
    // and the second FEC packet protect both two media packet.
    EncoderFec(media_packets, kNumFecPackets);
    EXPECT_EQ(kNumFecPackets, num_generated_fec_packets_);

    // Try to recovery both media packets.
    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(0u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(-1, fec_packet_counter.first_packet_arrival_time_ms);

    auto fec_it = generated_fec_packets_.begin();
    auto media_it = media_packets.begin();

    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    BuildAndAddRedFecPacket(*fec_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(1u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_recovered_packets);
    const int64_t first_packet_arrival_time_ms = fec_packet_counter.first_packet_arrival_time_ms;
    EXPECT_NE(-1, first_packet_arrival_time_ms);

    ++fec_it;
    ++media_it;
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    BuildAndAddRedFecPacket(*fec_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(2u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(2u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(2u, fec_packet_counter.num_recovered_packets);
    EXPECT_EQ(first_packet_arrival_time_ms, fec_packet_counter.first_packet_arrival_time_ms);
}

TEST_F(UlpFecReceiverTest, TwoFramesOneFec) {
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(1, media_packets);
    PacketizeFrame(1, media_packets);
    EXPECT_EQ(2u, media_packets.size());
    // Encode to FEC packets.
    EncoderFec(media_packets, kNumFecPackets);
    EXPECT_EQ(kNumFecPackets, num_generated_fec_packets_);

    // Try to recovery
    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(0u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(-1, fec_packet_counter.first_packet_arrival_time_ms);

    auto media_it = media_packets.begin();
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    // Add media packet
    BuildAndAddRedMediaPacket(**media_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(1u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);
    const int64_t first_packet_arrival_time_ms = fec_packet_counter.first_packet_arrival_time_ms;
    EXPECT_NE(-1, first_packet_arrival_time_ms);

    // Drop one media packet
    ++media_it;
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    // Recovery
    auto fec_it = generated_fec_packets_.begin();
    // Add FEC packet
    BuildAndAddRedFecPacket(*fec_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(2u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_recovered_packets);
    EXPECT_EQ(first_packet_arrival_time_ms, fec_packet_counter.first_packet_arrival_time_ms);
}

TEST_F(UlpFecReceiverTest, TwoFramesThreePacketOneFec) {
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(1, media_packets);
    PacketizeFrame(2, media_packets);
    EXPECT_EQ(3u, media_packets.size());
    // Encode to FEC packets.
    EncoderFec(media_packets, kNumFecPackets);
    EXPECT_EQ(kNumFecPackets, num_generated_fec_packets_);

    // Try to recovery
    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(0u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(-1, fec_packet_counter.first_packet_arrival_time_ms);

    // Add the first frame: one packet.
    auto media_it = media_packets.begin();
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    // Add media packet
    BuildAndAddRedMediaPacket(**media_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(1u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);

    ++media_it;
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);
    // Add the first packet of second frame.
    BuildAndAddRedMediaPacket(**media_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(2u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);

    // Drop the second pakcet of second frame.
    ++media_it;
    VerifyRecoveredMediaPacket(**media_it, 1 /* call_times */);

    // Recovery
    auto fec_it = generated_fec_packets_.begin();
    // Add FEC packet
    BuildAndAddRedFecPacket(*fec_it);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(3u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_recovered_packets);

}

} // namespace test
} // namespace naivertc