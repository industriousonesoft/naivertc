#include "rtc/rtp_rtcp/rtcp_packets/loss_notification.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

namespace naivertc {
namespace rtcp {

// Loss Notification
// -----------------
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| FMT=15  |   PT=206      |             length            |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  0 |                  SSRC of packet sender                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                  SSRC of media source                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |  Unique identifier 'L' 'N' 'T' 'F'                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 | Last Decoded Sequence Number  | Last Received SeqNum Delta  |D|
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

LossNotification::LossNotification()
    : last_decoded_(0), last_received_(0), decodability_flag_(false) {}

LossNotification::LossNotification(uint16_t last_decoded,
                                   uint16_t last_received,
                                   bool decodability_flag)
    : last_decoded_(last_decoded),
      last_received_(last_received),
      decodability_flag_(decodability_flag) {}

LossNotification::LossNotification(const LossNotification& rhs) = default;

LossNotification::~LossNotification() = default;

size_t LossNotification::PacketSize() const {
    return kRtcpCommonHeaderSize + kCommonFeedbackSize + kLossNotificationPayloadSize;
}

bool LossNotification::PackInto(uint8_t* packet,
                                size_t* index,
                                size_t max_length,
                                PacketReadyCallback callback) const {
    while (*index + PacketSize() > max_length) {
        if (!OnBufferFull(packet, index, callback))
        return false;
    }

    const size_t index_end = *index + PacketSize();

    // Note: |index| updated by the function below.
    PackCommonHeader(Psfb::kAfbMessageType, kPacketType, PacketSizeWithoutCommonHeader(), packet,
                index);

    PackCommonFeedback(packet + *index);
    *index += kCommonFeedbackSize;

    ByteWriter<uint32_t>::WriteBigEndian(packet + *index, kUniqueIdentifier);
    *index += sizeof(uint32_t);

    ByteWriter<uint16_t>::WriteBigEndian(packet + *index, last_decoded_);
    *index += sizeof(uint16_t);

    const uint16_t last_received_delta = last_received_ - last_decoded_;
    assert(last_received_delta <= 0x7fff);
    const uint16_t last_received_delta_and_decodability =
        (last_received_delta << 1) | (decodability_flag_ ? 0x0001 : 0x0000);

    ByteWriter<uint16_t>::WriteBigEndian(packet + *index,
                                        last_received_delta_and_decodability);
    *index += sizeof(uint16_t);

    assert(index_end == *index);
    return true;
}

bool LossNotification::Parse(const CommonHeader& packet) {
    assert(packet.type() == kPacketType);
    assert(packet.feedback_message_type() == Psfb::kAfbMessageType);

    if (packet.payload_size() < kCommonFeedbackSize + kLossNotificationPayloadSize) {
        return false;
    }

    const uint8_t* const payload = packet.payload();

    if (ByteReader<uint32_t>::ReadBigEndian(&payload[8]) != kUniqueIdentifier) {
        return false;
    }

    ParseCommonFeedback(payload);

    last_decoded_ = ByteReader<uint16_t>::ReadBigEndian(&payload[12]);

    const uint16_t last_received_delta_and_decodability =
        ByteReader<uint16_t>::ReadBigEndian(&payload[14]);
    last_received_ = last_decoded_ + (last_received_delta_and_decodability >> 1);
    decodability_flag_ = (last_received_delta_and_decodability & 0x0001);

    return true;
}

bool LossNotification::Set(uint16_t last_decoded,
                           uint16_t last_received,
                           bool decodability_flag) {
    const uint16_t delta = last_received - last_decoded;
    if (delta > 0x7fff) {
        return false;
    }
    last_received_ = last_received;
    last_decoded_ = last_decoded;
    decodability_flag_ = decodability_flag;
    return true;
}
    
} // namespace rtcp
} // namespace naivert 