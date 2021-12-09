#include "rtc/congestion_controller/goog_cc/delay_based_bwe_unittest_helper.hpp"

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
    : updated_(false), latest_bitrate_(0) {}

TestBitrateObserver::~TestBitrateObserver() = default;

void TestBitrateObserver::OnReceiveBitrateChanged(uint32_t bitrate) {
    latest_bitrate_ = bitrate;
    updated_ = true;
}
    
void TestBitrateObserver::Reset() {
    updated_ = false;
    latest_bitrate_ = 0;
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
    
} // namespace test
} // namespace naivertc
