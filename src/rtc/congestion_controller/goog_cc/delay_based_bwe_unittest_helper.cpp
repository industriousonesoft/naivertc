#include "rtc/congestion_controller/goog_cc/delay_based_bwe_unittest_helper.hpp"
#include "rtc/congestion_controller/goog_cc/bitrate_estimator.hpp"

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
    size_t bits_per_frame = (bitrate_bps_ * 1.0) / fps_ + 0.5;
    size_t num_packets = std::max<size_t>((bits_per_frame * 1.0) / (8 * kMtu) + 0.5, 1u);
    size_t bytes_per_packet = (bits_per_frame * 1.0) / (8 * num_packets) + 0.5;
    for (size_t i = 0; i < num_packets; ++i) {
        PacketResult packet;
        packet.sent_packet.send_time = Timestamp::Micros(now_us + kSendSideOffsetUs);
        packet.sent_packet.size = bytes_per_packet;
        packets.push_back(std::move(packet));
    }
    next_rtp_time_us_ = (now_us + kSendSideOffsetUs) * 1.0 / fps_ + 0.5;
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
        double deviding_ratio = bitrate_before * 1.0 / total_bitrate_before;
        int64_t bitrate_after = deviding_ratio * new_bitrate_bps + 0.5;
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
        int64_t transport_time_us = (8 * 1000.0 * packet.sent_packet.size) / capacity_bpus + 0.5;
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
        probe_bitrate_estimator_->HandleProbeAndEstimateBitrate(packet_feedback);
    }

    TransportPacketsFeedback msg;
    msg.feedback_time = Timestamp::Millis(clock_.now_ms());
    msg.packet_feedbacks.emplace_back(std::move(packet_feedback));
    ack_bitrate_estimator_->IncomingPacketFeedbackVector(msg.SortedByReceiveTime());
    DelayBasedBwe::Result ret = bandwidth_estimator_->IncomingPacketFeedbackVector(msg, 
                                                                                   ack_bitrate_estimator_->Estimate(),
                                                                                   probe_bitrate_estimator_->FetchAndResetLastEstimatedBitrate(),
                                                                                   /*in_alr=*/false);
    if (ret.updated) {
        bitrate_observer_.OnReceiveBitrateChanged(ret.target_bitrate.bps());
    }
}

bool T(DelayBasedBweTest)::GenerateAndProcessFrame(uint32_t ssrc, uint32_t bitrate_bps) {
    stream_generator_->SetBitrateBps(bitrate_bps);
  
    auto [packets, next_time_us] = stream_generator_->GenerateFrame(clock_.now_ms());
    if (packets.empty()) {
        return false;
    }

    bool overuse = false;
    bitrate_observer_.Reset();
    clock_.AdvanceTimeUs(packets.back().recv_time.us() - clock_.now_us());

    for (auto& packet : packets) {
        packet.recv_time += TimeDelta::Millis(recv_time_offset_ms_);
        if (packet.sent_packet.pacing_info.probe_cluster) {
            probe_bitrate_estimator_->HandleProbeAndEstimateBitrate(packet);
        }
    }

    ack_bitrate_estimator_->IncomingPacketFeedbackVector(packets);
    TransportPacketsFeedback msg;
    msg.packet_feedbacks = packets;
    msg.feedback_time = Timestamp::Millis(clock_.now_ms());

    DelayBasedBwe::Result ret = bandwidth_estimator_->IncomingPacketFeedbackVector(msg,
                                                                                   ack_bitrate_estimator_->Estimate(),
                                                                                   probe_bitrate_estimator_->FetchAndResetLastEstimatedBitrate(),
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
    
} // namespace test
} // namespace naivertc
