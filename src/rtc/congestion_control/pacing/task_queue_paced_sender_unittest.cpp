#include "rtc/congestion_control/pacing/task_queue_paced_sender.hpp"
#include "testing/simulated_time_controller.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

using ::testing::_;

namespace naivertc {
namespace test {
namespace {

constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
constexpr uint32_t kPaddingSsrc = 56789;
constexpr size_t kDefaultPacketSize = 1234;
constexpr int kNoPacketHoldback = -1;

} // namespace

class MockPacketSender : public PacingController::PacketSender {
public:
    void SendPacket(RtpPacketToSend packet,
                    const PacedPacketInfo& cluster_info) override {
        SendPacket(packet.packet_type(), packet.ssrc());
    }
    MOCK_METHOD(void,
                SendPacket,
                (RtpPacketType, uint32_t ssrc));
                
    MOCK_METHOD(std::vector<RtpPacketToSend>,
                FetchFecPackets,
                (),
                (override));
    MOCK_METHOD(std::vector<RtpPacketToSend>,
                GeneratePadding,
                (size_t target_size),
                (override));
};

class T(TaskQueuePacedSenderTest) : public ::testing::Test {
public:
    T(TaskQueuePacedSenderTest)() 
        : time_controller_(Timestamp::Millis(1000)),
          task_queue_(time_controller_.CreateTaskQueue()) {}

    void SetUp() override {
        CreatePacer();
    }

    void CreatePacer(TimeDelta max_holdback_window = PacingController::kMaxEarlyProbeProcessing, 
                     int max_hold_window_in_packets = kNoPacketHoldback) {
        TaskQueuePacedSender::Configuration config;
        config.clock = time_controller_.Clock();
        config.packet_sender = &packet_sender_;
        pacer_ = std::make_unique<TaskQueuePacedSender>(config,
                                                        task_queue_.get(),
                                                        max_holdback_window, 
                                                        max_hold_window_in_packets);
    }

    std::vector<RtpPacketToSend> 
    GeneratePadding(size_t target_size) {
        // 224 bytes is the max padding size for plain padding packets generated by
        // RTPSender::GeneratePadding().
        const size_t kMaxPaddingPacketSize = 224;
        size_t padding_generated = 0;
        std::vector<RtpPacketToSend> padding_packets;
        while (padding_generated < target_size) {
            size_t packet_size = std::min(target_size - padding_generated, kMaxPaddingPacketSize);
            padding_generated += packet_size;
            RtpPacketToSend packet{nullptr};
            packet.set_ssrc(kPaddingSsrc);
            packet.set_packet_type(RtpPacketType::PADDING);
            packet.SetPadding(packet_size);
            padding_packets.emplace_back(std::move(packet));
        }
        return padding_packets;
    }

    RtpPacketToSend BuildRtpPacket(RtpPacketType type) {
        RtpPacketToSend rtp_packet{nullptr};
        rtp_packet.set_packet_type(type);
        switch (type) {
        case RtpPacketType::AUDIO:
            rtp_packet.set_ssrc(kAudioSsrc);
            break;
        case RtpPacketType::VIDEO:
            rtp_packet.set_ssrc(kVideoSsrc);
            break;
        case RtpPacketType::RETRANSMISSION:
            rtp_packet.set_ssrc(kVideoRtxSsrc);
            break;
        case RtpPacketType::PADDING:
            rtp_packet.set_ssrc(kPaddingSsrc);
            break;
        case RtpPacketType::FEC:
            rtp_packet.set_ssrc(kFlexFecSsrc);
            break;
        }
        rtp_packet.set_payload_size(kDefaultPacketSize);
        return rtp_packet;
    }

    std::vector<RtpPacketToSend> GeneratePackets(RtpPacketType type, size_t num_packets) {
        std::vector<RtpPacketToSend> packets;
        for (size_t i = 0; i < num_packets; ++i) {
            packets.push_back(BuildRtpPacket(type));
        }
        return packets;
    }

protected:
    SimulatedTimeController time_controller_;
    ::testing::NiceMock<MockPacketSender> packet_sender_;
    std::unique_ptr<SimulatedTaskQueue, SimulatedTaskQueue::Deleter> task_queue_;
    std::unique_ptr<TaskQueuePacedSender> pacer_;
};

MY_TEST_F(TaskQueuePacedSenderTest, PacesPackets) {
    // Insert a number of packets, covering one second.
    const size_t kPacketToSend = 42;
    pacer_->SetPacingBitrates(DataRate::BitsPerSec(kDefaultPacketSize * 8 * kPacketToSend),
                             DataRate::Zero());
    pacer_->EnsureStarted();
    pacer_->EnqueuePackets(GeneratePackets(RtpPacketType::VIDEO, kPacketToSend));

    // Expect all of them to be sent.
    size_t packets_sent = 0;
    Timestamp end_time = Timestamp::PlusInfinity();
    EXPECT_CALL(packet_sender_, SendPacket).
        WillRepeatedly([&](RtpPacketType packet_type, 
                           uint32_t ssrc){
        ++packets_sent;
        if (packets_sent == kPacketToSend) {
            end_time = time_controller_.Clock()->CurrentTime();
        }
    });

    const auto start_time = time_controller_.Clock()->CurrentTime();
    // Packets should be sent over a period of close to 1s.
    // Expect a little lower than this since initial probing
    // is a bit quicker.
    time_controller_.AdvanceTime(TimeDelta::Seconds(1));
    EXPECT_EQ(packets_sent, kPacketToSend);
    ASSERT_TRUE(end_time.IsFinite());
    auto elapsed_time = end_time - start_time;
    EXPECT_NEAR(elapsed_time.ms<double>(), 1000.0, 50.0);
    GTEST_COUT << "Elapsed time=" << elapsed_time.ms() << std::endl;
}

MY_TEST_F(TaskQueuePacedSenderTest, ReschedulesProcessOnBitrateChange) {
    // Insert a number of packets to be sent as 200ms interval.
    const TimeDelta kPacketSendInterval = TimeDelta::Millis(200);
    const size_t kPacketsPerSecond = 5; // 5 = 1s / 200ms
    const DataRate kPacingBitrate = DataRate::BitsPerSec(kDefaultPacketSize * 8 * kPacketsPerSecond);
    pacer_->SetPacingBitrates(kPacingBitrate, DataRate::Zero());
    pacer_->EnsureStarted();

    // Send some initial packets to be rid of any probes.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc)).Times(kPacketsPerSecond);
    pacer_->EnqueuePackets(GeneratePackets(RtpPacketType::VIDEO, kPacketsPerSecond));
    time_controller_.AdvanceTime(TimeDelta::Seconds(1));

    // Insert three packets, and record send time of each of them.
    // After the second packet is sent, double the send rate so we can
    // check the third packets is sent after half the wait time.
    Timestamp first_packet_time = Timestamp::MinusInfinity();
    Timestamp second_packet_time = Timestamp::MinusInfinity();
    Timestamp third_packet_time = Timestamp::MinusInfinity();

    EXPECT_CALL(packet_sender_, SendPacket)
        .Times(3)
        .WillRepeatedly([&](RtpPacketType packet_type,
                            uint32_t ssrc) {
            if (first_packet_time.IsInfinite()) {
                first_packet_time = time_controller_.Clock()->CurrentTime();
            } else if (second_packet_time.IsInfinite()) {
                second_packet_time = time_controller_.Clock()->CurrentTime();
                // Pacer will reschedule process after updating pacing bitrate.
                pacer_->SetPacingBitrates(2 * kPacingBitrate, DataRate::Zero());
            } else {
                third_packet_time = time_controller_.Clock()->CurrentTime();
            }
        });

    pacer_->EnqueuePackets(GeneratePackets(RtpPacketType::VIDEO, 3));
    time_controller_.AdvanceTime(TimeDelta::Millis(500));
    ASSERT_TRUE(third_packet_time.IsFinite());
    EXPECT_NEAR((second_packet_time - first_packet_time).ms<double>(), kPacketSendInterval.ms(),
                1.0);
    EXPECT_NEAR((third_packet_time - second_packet_time).ms<double>(), kPacketSendInterval.ms() / 2,
                1.0);

}

MY_TEST_F(TaskQueuePacedSenderTest, SendsAudioImmediately) {
    const DataRate kPacingDataRate = DataRate::KilobitsPerSec(125); // 125kbps
    const TimeDelta kPacketPacingTime = kDefaultPacketSize / kPacingDataRate;

    pacer_->SetPacingBitrates(kPacingDataRate, DataRate::Zero());
    pacer_->EnsureStarted();

    // Add some initial video packets, only one should be sent.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc)).Times(1);
    pacer_->EnqueuePackets(GeneratePackets(RtpPacketType::VIDEO, 10));
    time_controller_.AdvanceTime(TimeDelta::Zero());
    
    // Advance time, but still before next packet should be sent.
    time_controller_.AdvanceTime(kPacketPacingTime / 2);

    // Insert an audio packet, it should be sent immediately.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::AUDIO, kAudioSsrc)).Times(1);
    pacer_->EnqueuePackets(GeneratePackets(RtpPacketType::AUDIO, 1));
    time_controller_.AdvanceTime(TimeDelta::Zero());
}

MY_TEST_F(TaskQueuePacedSenderTest, SleepsDuringHoldBackWindow) {
    const TimeDelta kMaxHoldBackWindow = TimeDelta::Millis(5);
    CreatePacer(kMaxHoldBackWindow);

    // Set rates so one packet adds one ms of buffer level.
    const TimeDelta kPacketPacingTime = TimeDelta::Millis(1);
    // Send one packet per millis second.
    const DataRate kPacingDataRate = kDefaultPacketSize / kPacketPacingTime;

    pacer_->SetPacingBitrates(kPacingDataRate, DataRate::Zero());
    pacer_->EnsureStarted();

    // Add 10 packets. The first should be sent immediately since the buffers
    // are clear.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc)).Times(1);
    pacer_->EnqueuePackets(GeneratePackets(RtpPacketType::VIDEO, 10));
    time_controller_.AdvanceTime(TimeDelta::Zero());

    // Advance time to 1ms before the coalescing window ends. No packets should
    // be sent.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc)).Times(0);
    time_controller_.AdvanceTime(kMaxHoldBackWindow - TimeDelta::Millis(1));

    // Advance time to where coalescing window ends. All packets that should
    // have been sent up til now will be sent.
    EXPECT_CALL(packet_sender_, SendPacket(RtpPacketType::VIDEO, kVideoSsrc)).Times(5);
    time_controller_.AdvanceTime(TimeDelta::Millis(1));
}

} // namespace test
} // namespace naivertc