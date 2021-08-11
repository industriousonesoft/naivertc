#ifndef _RTC_RTP_PACKET_H_
#define _RTC_RTP_PACKET_H_

#include "base/defines.hpp"
#include "rtc/base/packet.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacket : public Packet {
public:
    static std::shared_ptr<RtpPacket> Create() {
        return std::shared_ptr<RtpPacket>(new RtpPacket());
    }
    static std::shared_ptr<RtpPacket> Create(size_t capacity) {
        return std::shared_ptr<RtpPacket>(new RtpPacket(capacity));
    }

public:
    RtpPacket();
    RtpPacket(size_t capacity);
    RtpPacket(const RtpPacket&);

    ~RtpPacket();

    // Header
    bool marker() const { return marker_; }
    uint8_t payload_type() const { return payload_type_; }
    bool has_padding() const { return has_padding_; }
    uint8_t padding_size() const { return padding_size_; }
    uint16_t sequence_number() const { return sequence_num_; }
    uint32_t timestamp() const { return timestamp_; }
    uint32_t ssrc() const { return ssrc_; }
    std::vector<uint32_t> csrcs() const;

    size_t header_size() const { return payload_offset_; }
    // Payload
    size_t payload_size() const { return payload_size_; }
    BinaryBuffer Payload() const {
        auto paylaod_begin = begin() + payload_offset_;
        return BinaryBuffer(paylaod_begin, paylaod_begin + payload_size_);
    }

    size_t size() const {
        return payload_offset_ + payload_size_ + padding_size_;
    }
    size_t FreeCapacity() const { return capacity() - size(); }
    size_t MaxPayloadSize() const { return capacity() - header_size(); }

    // Reset all fields and buffer
    void Reset();

    // Header setters
    void set_has_padding(bool has_padding);
    void set_marker(bool marker);
    void set_payload_type(uint8_t payload_type);
    void set_sequence_number(uint16_t sequence_num);
    void set_timestamp(uint32_t timestamp);
    void set_ssrc(uint32_t ssrc);
    void set_payload(const BinaryBuffer& payload);
    void set_payload(const uint8_t* buffer, size_t size);
    
    void SetCsrcs(std::vector<uint32_t> csrcs);
    void CopyHeaderFrom(const RtpPacket& other);
    bool SetPadding(uint8_t padding_size);
    
    // Helper function for Parse. Fill header fields using data in given buffer,
    // but does not touch packet own buffer, leaving packet in invalid state.
    bool Parse(const uint8_t* buffer, size_t size);

private:
    inline void WriteAt(size_t offset, uint8_t byte);
    uint8_t* WriteAt(size_t offset);

private:
    struct ExtensionInfo {
        explicit ExtensionInfo(uint8_t id) : ExtensionInfo(id, 0, 0) {}
        ExtensionInfo(uint8_t id, uint8_t length, uint16_t offset)
            : id(id), length(length), offset(offset) {}
        uint8_t id;
        uint8_t length;
        uint16_t offset;
    };

    ExtensionInfo& FindOrCreateExtensionInfo(int id);

private:
    bool has_padding_;
    bool marker_;
    uint8_t payload_type_;
    uint8_t padding_size_;
    uint16_t sequence_num_;
    uint32_t timestamp_;
    uint32_t ssrc_;
    // Payload offset match header size with csrcs and extensions
    size_t payload_offset_;
    size_t payload_size_;

    size_t extensions_size_ = 0;
    std::vector<ExtensionInfo> extension_entries_;
};

} // namespace naivertc


#endif