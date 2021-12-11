#include "rtc/congestion_controller/goog_cc/delay_based_bwe_unittest_helper.hpp"
#include "rtc/congestion_controller/goog_cc/bitrate_estimator.hpp"
#include "common/utils_numeric.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr size_t kMtu = 1200;
constexpr uint32_t kAcceptedBitrateErrorBps = 50000;

// Number of packets needed before we have a valid estimate.
constexpr int kNumInitialPackets = 2;

constexpr int kInitialProbingPackets = 5;
    
} // namespace


// TestBitrateObserver
TestBitrateObserver::TestBitrateObserver() 
    : updated_(false), latest_bitrate_bps_(0) {}

TestBitrateObserver::~TestBitrateObserver() = default;

void TestBitrateObserver::OnReceiveBitrateChanged(uint32_t bitrate_bps) {
    latest_bitrate_bps_ = bitrate_bps;
    updated_ = true;
}
    
void TestBitrateObserver::Reset() {
    updated_ = false;
    latest_bitrate_bps_ = 0;
}

// RtpStream
RtpStream::RtpStream(int fps, int bitrate_bps) 
    : fps_(fps),
      bitrate_bps_(bitrate_bps) {}

RtpStream::~RtpStream() = default;

std::vector<PacketResult> RtpStream::GenerateFrame(int64_t now_us) {
    std::vector<PacketResult> packets;
    if (now_us < next_rtp_time_us_) {
        return packets;
    }
    size_t bits_per_frame = utils::numeric::division_with_roundup(bitrate_bps_, fps_);
    size_t num_packets = std::max<size_t>(utils::numeric::division_with_roundup(bits_per_frame, 8 * kMtu), 1u);
    size_t bytes_per_packet = utils::numeric::division_with_roundup(bits_per_frame, 8 * num_packets);
    for (size_t i = 0; i < num_packets; ++i) {
        PacketResult packet;
        packet.sent_packet.send_time = Timestamp::Micros(now_us + kSendSideOffsetUs);
        packet.sent_packet.size = bytes_per_packet;
        packets.push_back(std::move(packet));
    }
    next_rtp_time_us_ = utils::numeric::division_with_roundup<int64_t>(now_us + kSendSideOffsetUs, fps_);
    return packets;
}

bool RtpStream::Compare(const std::unique_ptr<RtpStream>& lhs,
                        const std::unique_ptr<RtpStream>& rhs) {
    return lhs->next_rtp_time_us_ < rhs->next_rtp_time_us_;
}

// RtpStreamGenerator
RtpStreamGenerator::RtpStreamGenerator(int link_capacity_bps, int64_t now_us) 
    : link_capacity_bps_(link_capacity_bps),
      pre_arrival_time_us_(now_us) {}

RtpStreamGenerator::~RtpStreamGenerator() = default;

void RtpStreamGenerator::AddStream(std::unique_ptr<RtpStream> stream) {
    streams_.push_back(std::move(stream));
}

void RtpStreamGenerator::set_link_capacity_bps(int link_capacity_bps) {
    link_capacity_bps_ = link_capacity_bps;
}

void RtpStreamGenerator::SetBitrateBps(int new_bitrate_bps) {
    int total_bitrate_before = 0;
    for (const auto& stream : streams_) {
        total_bitrate_before += stream->bitrate_bps();
    }
    int64_t bitrate_before = 0;
    int total_bitrate_after = 0;
    for (const auto& stream : streams_) {
        bitrate_before += stream->bitrate_bps();
        int64_t bitrate_after = utils::numeric::division_with_roundup<int64_t>(bitrate_before * new_bitrate_bps, total_bitrate_before);;
        stream->set_bitrate_bps(bitrate_after - total_bitrate_after);
        total_bitrate_after += stream->bitrate_bps();
    }
    ASSERT_EQ(bitrate_before, total_bitrate_before);
    EXPECT_EQ(total_bitrate_after, new_bitrate_bps);
}

std::pair<std::vector<PacketResult>, int64_t> RtpStreamGenerator::GenerateFrame(int64_t now_us) {
    auto it = std::min_element(streams_.begin(), streams_.end(), RtpStream::Compare);
    auto packets = (*it)->GenerateFrame(now_us);
    int i = 0;
    for (PacketResult& packet : packets) {
        int capacity_bpus = link_capacity_bps_ / 1000'000;
        int64_t transport_time_us = utils::numeric::division_with_roundup<int64_t>(8 * 1000 * packet.sent_packet.size, capacity_bpus);
        int64_t arrival_time_us_ = std::max(now_us + transport_time_us, pre_arrival_time_us_ + transport_time_us);
        packet.recv_time = Timestamp::Micros(arrival_time_us_);
        pre_arrival_time_us_ = arrival_time_us_;
        ++i;
    }
    it = std::min_element(streams_.begin(), streams_.end(), RtpStream::Compare);
    return {packets, std::max((*it)->next_rtp_time_us(), now_us)};
}

// T(DelayBasedBweTest)
T(DelayBasedBweTest)::T(DelayBasedBweTest)() 
    : clock_(Timestamp::Millis(100)),
      ack_bitrate_estimator_(std::make_unique<AcknowledgedBitrateEstimator>(std::make_unique<BitrateEstimator>(BitrateEstimator::Configuration()))),
      probe_bitrate_estimator_(std::make_unique<ProbeBitrateEstimator>()),
      bandwidth_estimator_(std::make_unique<DelayBasedBwe>(DelayBasedBwe::Configuration())),
      stream_generator_(std::make_unique<RtpStreamGenerator>(/*link_capacity_bps=*/1e6, clock_.now_ms())),
      recv_time_offset_ms_(0),
      first_update_(true) {}

T(DelayBasedBweTest)::~T(DelayBasedBweTest)() = default;

void T(DelayBasedBweTest)::AddStream(int fps, int bitrate_bps) {
    stream_generator_->AddStream(std::make_unique<RtpStream>(fps, bitrate_bps));
}

void T(DelayBasedBweTest)::IncomingFeedback(int64_t recv_time_ms,
                                            int64_t send_time_ms,
                                            size_t payload_size) {
    IncomingFeedback(recv_time_ms, send_time_ms, payload_size, PacedPacketInfo());
}   

void T(DelayBasedBweTest)::IncomingFeedback(int64_t recv_time_ms,
                                            int64_t send_time_ms,
                                            size_t payload_size,
                                            const PacedPacketInfo& pacing_info) {
    ASSERT_GE(recv_time_ms + recv_time_offset_ms_, 0);
    PacketResult packet_feedback;
    packet_feedback.recv_time = Timestamp::Millis(recv_time_ms + recv_time_offset_ms_);
    packet_feedback.sent_packet.send_time = Timestamp::Millis(send_time_ms);
    packet_feedback.sent_packet.size = payload_size;
    packet_feedback.sent_packet.pacing_info = pacing_info;
    if (packet_feedback.sent_packet.pacing_info.probe_cluster) {
        probe_bitrate_estimator_->IncomingProbePacketFeedback(packet_feedback);
    }

    TransportPacketsFeedback msg;
    msg.feedback_time = Timestamp::Millis(clock_.now_ms());
    msg.packet_feedbacks.emplace_back(std::move(packet_feedback));
    ack_bitrate_estimator_->IncomingPacketFeedbacks(msg.SortedByReceiveTime());
    DelayBasedBwe::Result ret = bandwidth_estimator_->IncomingPacketFeedbacks(msg, 
                                                                              ack_bitrate_estimator_->Estimate(),
                                                                              probe_bitrate_estimator_->Estimate(),
                                                                              /*in_alr=*/false);
    if (ret.updated) {
        bitrate_observer_.OnReceiveBitrateChanged(ret.target_bitrate.bps());
    }
}

bool T(DelayBasedBweTest)::GenerateAndProcessFrame(uint32_t ssrc, uint32_t bitrate_bps) {
    // Generates frame with a given bitrate.
    stream_generator_->SetBitrateBps(bitrate_bps);
    auto [packets, next_time_us] = stream_generator_->GenerateFrame(clock_.now_ms());
    if (packets.empty()) {
        return false;
    }

    // Process the packets of the generated frame above.
    bool overuse = false;
    bitrate_observer_.Reset();
    // Simulate all the packets have arrived.
    clock_.AdvanceTimeUs(packets.back().recv_time.us() - clock_.now_us());

    // Try to process probe packets.
    for (auto& packet : packets) {
        packet.recv_time += TimeDelta::Millis(recv_time_offset_ms_);
        if (packet.sent_packet.pacing_info.probe_cluster) {
            probe_bitrate_estimator_->IncomingProbePacketFeedback(packet);
        }
    }

    // Try to process incoming packets and estimate bitrate.
    ack_bitrate_estimator_->IncomingPacketFeedbacks(packets);
    TransportPacketsFeedback msg;
    msg.packet_feedbacks = packets;
    msg.feedback_time = Timestamp::Millis(clock_.now_ms());

    DelayBasedBwe::Result ret = bandwidth_estimator_->IncomingPacketFeedbacks(msg,
                                                                              ack_bitrate_estimator_->Estimate(),
                                                                              probe_bitrate_estimator_->Estimate(),
                                                                                   /*in_alr=*/false);
    if (ret.updated) {
        bitrate_observer_.OnReceiveBitrateChanged(ret.target_bitrate.bps());
        if (!first_update_ && ret.target_bitrate.bps() < bitrate_bps) {
            overuse = true;
        }
        first_update_ = false;
    }

    clock_.AdvanceTimeUs(next_time_us - clock_.now_us());
    return overuse;
}

uint32_t T(DelayBasedBweTest)::SteadyStateRun(uint32_t ssrc,
                                              int num_of_frames,
                                              uint32_t start_bitrate,
                                              uint32_t min_bitrate,
                                              uint32_t max_bitrate,
                                              uint32_t target_bitrate) {
    uint32_t bitrate_bps = start_bitrate;
    bool bitrate_update_seen = false;
    // Produce `num_of_frames` frames and give them to the estimator.
    for (int i = 0; i < num_of_frames; ++i) {
        bool overuse = GenerateAndProcessFrame(ssrc, bitrate_bps);
        if (overuse) {
            EXPECT_LT(bitrate_observer_.latest_bitrate_bps(), max_bitrate);
            EXPECT_GT(bitrate_observer_.latest_bitrate_bps(), min_bitrate);
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            bitrate_update_seen = true;
        } else if (bitrate_observer_.updated()) {
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            bitrate_observer_.Reset();
        }
        if (bitrate_update_seen && bitrate_bps > target_bitrate) {
            break;
        }
    }
    EXPECT_TRUE(bitrate_update_seen);
    return bitrate_bps;
}

void T(DelayBasedBweTest)::LinkCapacityDropTestHelper(int num_of_streams,
                                                      uint32_t expected_bitrate_drop_delta,
                                                      int64_t receiver_clock_offset_change_ms) {
    const int kFrameRate = 30;
    const int kStartBitrate = 900e3;
    const int kMinExpectedBitrate = 800e3;
    const int kMaxExpectedBitrate = 1100e3;
    const uint32_t kInitialCapacityBps = 1000e3;
    const uint32_t kReducedCapcacityBps = 500e3;

    int steady_state_time = 0;
    if (num_of_streams <= 1) {
        steady_state_time = 10;
        AddStream();
    } else {
        steady_state_time = 10 * num_of_streams;
        int bitrate_sum = 0;
        int kBitrateDenom = num_of_streams * (num_of_streams - 1);
        for (int i = 0; i < num_of_streams; ++i) {
            // First stream gets half available bitrate, while the rest share the
            // remaining half i.e.: 1/2 = Sum[n/(N*(N-1))] for n=1..N-1 (rounded up)
            int bitrate = kStartBitrate / 2;
            if (i > 0) {
                bitrate = utils::numeric::division_with_roundup(kStartBitrate * i, kBitrateDenom);
            }
            stream_generator_->AddStream(std::make_unique<RtpStream>(kFrameRate, bitrate));
            bitrate_sum += bitrate;
        }
        ASSERT_EQ(bitrate_sum, kStartBitrate);
    }

    // Run in steady state to make the estimator converge.
    stream_generator_->set_link_capacity_bps(kInitialCapacityBps);
    uint32_t bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                          steady_state_time * kFrameRate,
                                          kStartBitrate,
                                          kMinExpectedBitrate,
                                          kMaxExpectedBitrate,
                                          kInitialCapacityBps);
    EXPECT_NEAR(kInitialCapacityBps, bitrate_bps, 180'000u);
    bitrate_observer_.Reset();

    // // Add an offset to make sure the BWE can handle it.
    // recv_time_offset_ms_ += receiver_clock_offset_change_ms;

    // // Reduce the capacity and verify the decrease time.
    // stream_generator_->set_link_capacity_bps(kReducedCapcacityBps);
    // int64_t overuse_start_time = clock_.now_ms();
    // int64_t bitrate_drop_time = -1;
    // for (int i = 0; i < 100 * num_of_streams; ++i) {
    //     GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    //     if (bitrate_drop_time == -1 &&
    //         bitrate_observer_.latest_bitrate_bps() <= kReducedCapcacityBps) {
    //         bitrate_drop_time = clock_.now_ms();
    //     }
    //     if (bitrate_observer_.updated()) {
    //         bitrate_bps = bitrate_observer_.latest_bitrate_bps();
    //     }
    // }

    // EXPECT_NEAR(expected_bitrate_drop_delta, bitrate_drop_time - overuse_start_time, 33);
}
    
} // namespace test
} // namespace naivertc
