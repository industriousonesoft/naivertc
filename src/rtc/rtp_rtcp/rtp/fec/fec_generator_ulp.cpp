#include "rtc/rtp_rtcp/rtp/fec/fec_generator_ulp.hpp"

namespace naivertc {
namespace {

constexpr size_t kRedForFecHeaderLength = 1;

// This controls the maximum amount of excess overhead (actual - target)
// allowed in order to trigger EncodeFec(), before |params_.max_fec_frames|
// is reached. Overhead here is defined as relative to number of media packets.
constexpr int kMaxExcessOverhead = 50;  // Q8.

// This is the minimum number of media packets required (above some protection
// level) in order to trigger EncodeFec(), before |params_.max_fec_frames| is
// reached.
constexpr size_t kMinMediaPackets = 4;

// Threshold on the received FEC protection level, above which we enforce at
// least |kMinMediaPackets| packets for the FEC code. Below this
// threshold |kMinMediaPackets| is set to default value of 1.
//
// The range is between 0 and 255, where 255 corresponds to 100% overhead
// (relative to the number of protected media packets).
constexpr uint8_t kHighProtectionThreshold = 80; // Q8

// This threshold is used to adapt the |kMinMediaPackets| threshold, based
// on the average number of packets per frame seen so far. When there are few
// packets per frame (as given by this threshold), at least
// |kMinMediaPackets| + 1 packets are sent to the FEC code.
constexpr float kMinMediaPacketsAdaptationThreshold = 2.0f;
    
} // namespace


UlpFecGenerator::UlpFecGenerator(int red_payload_type, 
                                 int fec_payload_type) 
    : red_payload_type_(red_payload_type),
      fec_payload_type_(fec_payload_type),
      num_protected_frames_(0),
      min_num_media_packets_(1),
      contains_key_frame_(false),
      fec_encoder_(FecEncoder::CreateUlpFecEncoder()),
      last_protected_media_packet_(std::nullopt),
      generated_fec_packets_(nullptr) {
}
    
UlpFecGenerator::~UlpFecGenerator() {}

size_t UlpFecGenerator::MaxPacketOverhead() const {
    return this->fec_encoder_->MaxPacketOverhead();
}

void UlpFecGenerator::SetProtectionParameters(const FecProtectionParams& delta_params, const FecProtectionParams& key_params) {
    assert(delta_params.fec_rate >= 0 && delta_params.fec_rate <= 255);
    assert(key_params.fec_rate >= 0 && key_params.fec_rate <= 255);
    this->pending_params_.emplace(std::make_pair(delta_params, key_params));
}

void UlpFecGenerator::PushMediaPacket(std::shared_ptr<RtpPacketToSend> media_packet) {
    if (this->pending_params_.has_value()) {
        this->current_params_ = this->pending_params_.value();
        this->pending_params_.reset();
    }

    // Set the minimum media packets to protect
    if (this->CurrentParams().fec_rate > kHighProtectionThreshold) {
        this->min_num_media_packets_ = kMinMediaPackets;
    } else {
        this->min_num_media_packets_ = 1;
    }

    // Enable key-protection-parameters if encountering a key frame packet once.
    if (media_packet->is_key_frame()) {
        this->contains_key_frame_ = true;
    }
    
    const bool complete_frame = media_packet->marker();
    // UlpFec packet masks can only protect up to 48 media packets
    if (this->media_packets_.size() < kUlpFecMaxMediaPackets /* 48 */) {
        this->media_packets_.push_back(media_packet);
        // Keep a reference of the last media packet, so we can copy the RTP
        // header from it when creating newly RED+FEC packets later.
        this->last_protected_media_packet_ = std::move(media_packet);
    } else {
        // TODO: How to handle packets not added into media packtets??
    }

    if (complete_frame) {
        this->num_protected_frames_ += 1;
    }

    auto curr_params = this->CurrentParams();

    // Produce FEC over at most |params_.max_fec_frames| frames, or as soon as:
    // (1) the excess overhead (actual overhead - requested/target overhead) is
    // less than |kMaxExcessOverhead|, and
    // (2) at least |min_num_media_packets_| media packets is reached.
    if (complete_frame && 
        (this->num_protected_frames_ >= curr_params.max_fec_frames || 
        (MaxExcessOverheadNotReached(curr_params.fec_rate) && 
        MinimumMediaPacketsReached()))) {
        // We are not using Unequal Protection feature of the parity erasure code.
        constexpr int kNumImportantPackets = 0;
        constexpr bool kUseUnequalProtection = false;
        generated_fec_packets_ = this->fec_encoder_->Encode(this->media_packets_, 
                                                            curr_params.fec_rate, 
                                                            kNumImportantPackets, 
                                                            kUseUnequalProtection, 
                                                            curr_params.fec_mask_type);
        if (this->generated_fec_packets_.empty()) {
            this->Reset();
        }
    }
}

std::vector<std::shared_ptr<RtpPacketToSend>> UlpFecGenerator::PopFecPackets() {
    std::vector<std::shared_ptr<RtpPacketToSend>> rtp_fec_packets;
    if (this->generated_fec_packets_.empty()) {
        return rtp_fec_packets;
    }
    auto last_media_packet = this->last_protected_media_packet_.value_or(nullptr);
    if (last_media_packet == nullptr) {
        return rtp_fec_packets;
    }
    
    // Wrap FEC packet (including FEC headers) in a RED packet. Since the
    // FEC packets in |generated_fec_packets_| don't have RTP headers, we
    // reuse the header from the last media packet.
    rtp_fec_packets.reserve(this->generated_fec_packets_.size());

    size_t total_fec_size_bytes = 0;
    for (auto it = this->generated_fec_packets_.cbegin(); it != this->generated_fec_packets_.end(); ++it) {
        const FecPacket* fec_packet = it;
     
        std::shared_ptr<RtpPacketToSend> red_packet = std::make_shared<RtpPacketToSend>(last_media_packet->capacity());
        red_packet->CopyHeaderFrom(*last_media_packet.get());
        red_packet->set_payload_type(this->red_payload_type_);
        red_packet->set_marker(false);
        
        assert(red_packet->header_size() + kRedForFecHeaderLength + fec_packet->size() < red_packet->capacity());
        uint8_t* payload_buffer = red_packet->SetPayloadSize(kRedForFecHeaderLength + fec_packet->size());
        assert(payload_buffer != nullptr);
        // Primary RED header with F bit unset.
        // See https://tools.ietf.org/html/rfc2198#section-3
        // RED header, 1 byte
        payload_buffer[0] = static_cast<uint8_t>(this->fec_payload_type_);
        memcpy(&payload_buffer[1], fec_packet->data(), fec_packet->size());
        total_fec_size_bytes += red_packet->size();
        red_packet->set_packet_type(RtpPacketType::FEC);
        red_packet->set_allow_retransmission(false);
        red_packet->set_is_red(true);
        red_packet->set_fec_protection_need(false);
        red_packet->set_red_protection_need(false);
        rtp_fec_packets.push_back(std::move(red_packet));
    }

    Reset();

    return rtp_fec_packets;
}

const FecProtectionParams& UlpFecGenerator::CurrentParams() const {
    return contains_key_frame_ ? this->current_params_.second : this->current_params_.first;
}

// Private methods
bool UlpFecGenerator::MaxExcessOverheadNotReached(size_t target_fec_rate) const {
    assert(media_packets_.size() > 0);
    size_t num_fec_packets = fec_encoder_->NumFecPackets(media_packets_.size(), target_fec_rate);
    // Actual fec rate in Q8 [0~255]
    size_t actual_fec_rate = (num_fec_packets << 8) / media_packets_.size();
    return (actual_fec_rate - target_fec_rate) < kMaxExcessOverhead;
}

bool UlpFecGenerator::MinimumMediaPacketsReached() const {
    float average_num_packets_per_frame = static_cast<float>(media_packets_.size()) / num_protected_frames_;
    size_t num_media_packets = media_packets_.size();
    if (average_num_packets_per_frame < kMinMediaPacketsAdaptationThreshold) {
        return num_media_packets >= min_num_media_packets_;
    } else {
        return num_media_packets >= min_num_media_packets_ + 1;
    }
}

void UlpFecGenerator::Reset() {
    media_packets_.clear();
    last_protected_media_packet_.reset();
    generated_fec_packets_.Reset();
    num_protected_frames_ = 0;
    min_num_media_packets_ = 1;
    contains_key_frame_ = false;
}
    
} // namespace naivertc
