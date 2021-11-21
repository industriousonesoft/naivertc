#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_receiver_ulp.hpp"
#include "rtc/base/time/clock_simulated.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_encoder.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_test_helper.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

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
class T(UlpFecReceiverTest) : public ::testing::Test {
protected:
    T(UlpFecReceiverTest)() 
        : clock_(std::make_shared<SimulatedClock>(0x100)),
          recovered_packet_receiver_(std::make_shared<MockRecoveredPacketReceiver>()),
          fec_receiver_(std::make_unique<UlpFecReceiver>(kMediaSsrc, clock_, recovered_packet_receiver_)),
          fec_encoder_(FecEncoder::CreateUlpFecEncoder()),
          generated_fec_packets_(fec_encoder_->MaxFecPackets()),
          packet_generator_(kMediaSsrc, kVp8PayloadType, kFecPayloadType, kRedPayloadType) {}

    size_t EncoderFec(const FecEncoder::PacketList& media_packets, size_t num_fec_packet);

    void PacketizeFrame(size_t num_media_packets, FecEncoder::PacketList& media_packets);

    void BuildAndAddRedMediaPacket(const RtpPacket& rtp_packet, bool is_recovered = false);

    void BuildAndAddRedFecPacket(const CopyOnWriteBuffer& fec_packets);

    void VerifyRecoveredMediaPacket(const RtpPacket& packet, size_t call_times);

    template<typename T>
    void InjectGarbageData(size_t offset, T data);

protected:
    std::shared_ptr<SimulatedClock> clock_;
    std::shared_ptr<MockRecoveredPacketReceiver> recovered_packet_receiver_;
    std::unique_ptr<UlpFecReceiver> fec_receiver_;
    std::unique_ptr<FecEncoder> fec_encoder_;
    FecEncoder::FecPacketList generated_fec_packets_;
    UlpFecPacketGenerator packet_generator_;
};

// Implements
size_t T(UlpFecReceiverTest)::EncoderFec(const FecEncoder::PacketList& media_packets, 
                                      size_t num_fec_packets) {
    const uint8_t protection_factor = num_fec_packets * 255 / media_packets.size();
    // Unequal protection is turned off, and the number of important
    // packets is thus irrelevant.
    constexpr int kNumImportantPackets = 0;
    constexpr bool kUseUnequalProtection = false;
    constexpr FecMaskType kFecMaskType = FecMaskType::BURSTY;
    auto [num_generated_fec_packets, success] = fec_encoder_->Encode(media_packets,
                                                                     protection_factor,
                                                                     kNumImportantPackets,
                                                                     kUseUnequalProtection,
                                                                     kFecMaskType,
                                                                     generated_fec_packets_);
    EXPECT_TRUE(success);
    return num_generated_fec_packets;
}

void T(UlpFecReceiverTest)::PacketizeFrame(size_t num_media_packets, FecEncoder::PacketList& media_packets) {
    packet_generator_.NewFrame(num_media_packets);
    for (size_t i = 0; i < num_media_packets; ++i) {
        RtpPacket rtp_packet = packet_generator_.NextRtpPacket(10 /* payload_size */ , 0/* padding_size */);
        media_packets.push_back(std::make_shared<RtpPacket>(std::move(rtp_packet)));
    }
}

void T(UlpFecReceiverTest)::BuildAndAddRedMediaPacket(const RtpPacket& rtp_packet, bool is_recovered) {
    RtpPacketReceived red_packet = packet_generator_.BuildMediaRedPacket(rtp_packet, is_recovered);
    EXPECT_TRUE(fec_receiver_->OnRedPacket(red_packet, kFecPayloadType));
} 

void T(UlpFecReceiverTest)::BuildAndAddRedFecPacket(const CopyOnWriteBuffer& fec_packets) {
    RtpPacketReceived red_packet = packet_generator_.BuildUlpFecRedPacket(fec_packets);
    EXPECT_TRUE(fec_receiver_->OnRedPacket(red_packet, kFecPayloadType));
}

void T(UlpFecReceiverTest)::VerifyRecoveredMediaPacket(const RtpPacket& packet, size_t call_times) {
    // Verify that the content of the reconstructed packet is equal to the
    // content of |packet|, and that the same content is received |times| number
    // of times in a row.
    // NOTE: `EXPECT_CALL` MUST be called before the `OnRecoveredPacket` which is the real call. 
    EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(packet)).Times(call_times);
}

template<typename U>
void T(UlpFecReceiverTest)::InjectGarbageData(size_t offset, U data) {
    const size_t kNumMediaPackets = 2u;
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(kNumMediaPackets, media_packets);
    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPackets, EncoderFec(media_packets, kNumFecPackets));

    // Insert garbage bytes
    auto fec_it = generated_fec_packets_.begin();
    ByteWriter<U>::WriteBigEndian(fec_it->data() + offset, data);

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
MY_TEST_F(UlpFecReceiverTest, TwoMediaOneFec) {
    const size_t kNumMediaPackets = 2u;
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(kNumMediaPackets, media_packets);
    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPackets, EncoderFec(media_packets, kNumFecPackets));

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

MY_TEST_F(UlpFecReceiverTest, TwoMediaOneFecNotUsesRecoveredPackets) {
    const size_t kNumMediaPackets = 2u;
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(kNumMediaPackets, media_packets);
    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPackets, EncoderFec(media_packets, kNumFecPackets));

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

MY_TEST_F(UlpFecReceiverTest, InjectGarbageFecHeaderLengthRecovery) {
    // Byte offset 8 is the 'length recovery' field of the FEC header.
    InjectGarbageData(8, 0x4711);
}

MY_TEST_F(UlpFecReceiverTest, InjectGarbageFecLevelHeaderProtectionLength) {
    // Byte offset 10 is the 'protection length' field in the first FEC level
    // header.
    InjectGarbageData(10, 0x4711);
}

MY_TEST_F(UlpFecReceiverTest, TwoMediaTwoFec) {
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
    EXPECT_EQ(kNumFecPackets, EncoderFec(media_packets, kNumFecPackets));

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

MY_TEST_F(UlpFecReceiverTest, TwoFramesOneFec) {
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(1, media_packets);
    PacketizeFrame(1, media_packets);
    EXPECT_EQ(2u, media_packets.size());
    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPackets, EncoderFec(media_packets, kNumFecPackets));

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

MY_TEST_F(UlpFecReceiverTest, TwoFramesThreePacketOneFec) {
    const size_t kNumFecPackets = 1u;
    // Create media packets.
    FecEncoder::PacketList media_packets;
    PacketizeFrame(1, media_packets);
    PacketizeFrame(2, media_packets);
    EXPECT_EQ(3u, media_packets.size());
    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPackets, EncoderFec(media_packets, kNumFecPackets));

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

MY_TEST_F(UlpFecReceiverTest, MaxFramesOneFec) {
    const size_t kNumFecPackets = 1;
    const size_t kNumMediaPackets = 48; // L bit set, mask size = 2 + 4
    // Generate media packets.
    FecEncoder::PacketList media_packets;
    for (size_t i = 0; i < kNumMediaPackets; ++i) {
        PacketizeFrame(1, media_packets);
    }
    EXPECT_EQ(kNumMediaPackets, media_packets.size());

    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPackets, EncoderFec(media_packets, kNumFecPackets));

    // Try to recover.
    auto media_it = media_packets.begin();
    auto& dropped_media_packet = **media_it;
    // Drop first packet.
    ++media_it;
    for (; media_it != media_packets.end(); ++media_it) {
        VerifyRecoveredMediaPacket(**media_it, 1);
        BuildAndAddRedMediaPacket(**media_it);
    }

    // Add FEC packet to recover the dropped media packet.
    VerifyRecoveredMediaPacket(dropped_media_packet, 1);
    BuildAndAddRedFecPacket(*generated_fec_packets_.begin());
}

MY_TEST_F(UlpFecReceiverTest, TooManyFrames) {
    const size_t kNumFecPackets = 1;
    // The max number of media packets can be protected by FEC is 48.
    const size_t kNumMediaPackets = 49;
    // Generate media packets.
    FecEncoder::PacketList media_packets;
    for (size_t i = 0; i < kNumMediaPackets; ++i) {
        PacketizeFrame(1, media_packets);
    }
    EXPECT_EQ(kNumMediaPackets, media_packets.size());

    const uint8_t protection_factor = kNumFecPackets * 255 / media_packets.size();
    // Unequal protection is turned off, and the number of important
    // packets is thus irrelevant.
    constexpr int kNumImportantPackets = 0;
    constexpr bool kUseUnequalProtection = false;
    constexpr FecMaskType kFecMaskType = FecMaskType::BURSTY;
    auto [num_fec_packets, success]  = fec_encoder_->Encode(media_packets,
                                                            protection_factor,
                                                            kNumImportantPackets,
                                                            kUseUnequalProtection,
                                                            kFecMaskType,
                                                            generated_fec_packets_);
    EXPECT_EQ(0, num_fec_packets);
    EXPECT_FALSE(success);
}

MY_TEST_F(UlpFecReceiverTest, PacketNotDroppedTooEarly) {
    // 1 frame with 2 media packets and one FEC packet. One media packet missing.
    // Delay the FEC packet.
    const size_t kNumFecPacketsBatch1 = 1;
    const size_t kNumMediaPacketsBatch1 = 2;
    // Generate media packets.
    FecEncoder::PacketList media_packets_batch1;
    PacketizeFrame(kNumMediaPacketsBatch1, media_packets_batch1);

    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPacketsBatch1, EncoderFec(media_packets_batch1, kNumFecPacketsBatch1));
    EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(1);
    BuildAndAddRedMediaPacket(**media_packets_batch1.begin());

    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(1u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);

    auto& delayed_fec_packet = *generated_fec_packets_.begin();

    // Fill the FEC decoder. No packets should be dropped.
    const size_t kNumMediaPacketsBatch2 = 191;
    FecEncoder::PacketList media_packets_batch2;
    for (size_t i = 0; i < kNumMediaPacketsBatch2; ++i) {
        PacketizeFrame(1, media_packets_batch2);
    }
    EXPECT_EQ(kNumMediaPacketsBatch2, media_packets_batch2.size());

    // Add media packet to FEC receiver.
    for (auto it = media_packets_batch2.begin(); it != media_packets_batch2.end(); ++it) {
        EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(1);
        BuildAndAddRedMediaPacket(**it);
    }

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(192u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);

    // Add the delayed FEC packet to recover the missing media packet.
    EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(1);
    BuildAndAddRedFecPacket(delayed_fec_packet);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(193u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_recovered_packets);

}

MY_TEST_F(UlpFecReceiverTest, PacketDroppedWhenTooOld) {
    // 1 frame with 2 media packets and one FEC packet. One media packet missing.
    // Delay the FEC packet.
    const size_t kNumFecPacketsBatch1 = 1;
    const size_t kNumMediaPacketsBatch1 = 2;
    // Generate media packets.
    FecEncoder::PacketList media_packets_batch1;
    PacketizeFrame(kNumMediaPacketsBatch1, media_packets_batch1);

    // Encode to FEC packets.
    EXPECT_EQ(kNumFecPacketsBatch1, EncoderFec(media_packets_batch1, kNumFecPacketsBatch1));
    EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(1);
    BuildAndAddRedMediaPacket(**media_packets_batch1.begin());

    auto fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(1u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);

    auto& delayed_fec_packet = *generated_fec_packets_.begin();

    // Fill the FEC decoder. No packets should be dropped.
    const size_t kNumMediaPacketsBatch2 = kMaxTrackedMediaPackets; // 192
    FecEncoder::PacketList media_packets_batch2;
    for (size_t i = 0; i < kNumMediaPacketsBatch2; ++i) {
        PacketizeFrame(1, media_packets_batch2);
    }
    EXPECT_EQ(kNumMediaPacketsBatch2, media_packets_batch2.size());

    // Add media packet to FEC receiver.
    for (auto it = media_packets_batch2.begin(); it != media_packets_batch2.end(); ++it) {
        EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(1);
        BuildAndAddRedMediaPacket(**it);
    }

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(193u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);

    // Add the delayed FEC packet. No packet should be reconstructed since the
    // first media packet of that frame has been dropped due to being too old.
    EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(0);
    BuildAndAddRedFecPacket(delayed_fec_packet);

    fec_packet_counter = fec_receiver_->packet_counter();
    EXPECT_EQ(194u, fec_packet_counter.num_received_packets);
    EXPECT_EQ(1u, fec_packet_counter.num_received_fec_packets);
    EXPECT_EQ(0u, fec_packet_counter.num_recovered_packets);
}

MY_TEST_F(UlpFecReceiverTest, OldFecPacketDropped) {
    // 49 frames with 2 media packets and one FEC packet. 
    // All media packets missing.
    const size_t kNumMediaPackets = (kUlpFecMaxMediaPackets /* 48 */ + 1) * 2;
    FecEncoder::PacketList media_packets;
    for (size_t i = 0; i < kNumMediaPackets / 2; ++i) {
        FecEncoder::PacketList frame_media_packets;
        // Generate media packets.
        PacketizeFrame(2, frame_media_packets);
        // Encode one FEC packet.
        EncoderFec(frame_media_packets, 1);
        for (auto it = generated_fec_packets_.begin(); it != generated_fec_packets_.end(); ++it) {
            // Only FEC packtes inserted, no media packets recoverable at this time.
            EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(0);
            BuildAndAddRedFecPacket(*it);
        }
        media_packets.insert(media_packets.end(), frame_media_packets.begin(), frame_media_packets.end());
    }

    // Insert the oldest media packet. The corresponding FEC packet is too old
    // and should have been dropped. Only the media packet we inserted will be
    // returned.
    EXPECT_CALL(*recovered_packet_receiver_, OnRecoveredPacket(_)).Times(1);
    BuildAndAddRedMediaPacket(**media_packets.begin());
    
}

MY_TEST_F(UlpFecReceiverTest, MediaWithPadding) {
    const size_t kNumFecPacket = 1;
    FecEncoder::PacketList media_packets;
    PacketizeFrame(2, media_packets);

    // Append 4 bytes of padding to the first media packet.
    auto first_media_packet = **media_packets.begin();
    first_media_packet.SetPadding(4);

    // Generate one FEC packet.
    EncoderFec(media_packets, kNumFecPacket);

    // Received the first media packet.
    auto media_it = media_packets.begin();
    VerifyRecoveredMediaPacket(**media_it, 1);
    BuildAndAddRedMediaPacket(**media_it);

    // Missing the second media packet.
    ++media_it;
    // Received FEC packet to recover the missing media packet.
    VerifyRecoveredMediaPacket(**media_it, 1);
    BuildAndAddRedFecPacket(*generated_fec_packets_.begin());
}

} // namespace test
} // namespace naivertc