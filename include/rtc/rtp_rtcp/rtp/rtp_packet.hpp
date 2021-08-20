#ifndef _RTC_RTP_PACKET_H_
#define _RTC_RTP_PACKET_H_

#include "base/defines.hpp"
#include "rtc/base/packet.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_header_extension_manager.hpp"

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

    using ExtensionType = RtpExtensionType;
    using ExtensionManager = rtp::ExtensionManager;
public:
    RtpPacket();
    RtpPacket(size_t capacity);
    RtpPacket(const RtpPacket&);
    explicit RtpPacket(std::shared_ptr<ExtensionManager> extension_manager);
    RtpPacket(std::shared_ptr<ExtensionManager>, size_t capacity);
    virtual ~RtpPacket();

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
    const uint8_t* payload_data() const {
        return &(BinaryBuffer::at(payload_offset_));
    }
    size_t payload_size() const { return payload_size_; }
    BinaryBuffer PayloadBuffer() const {
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

    // Reserve size_bytes for payload. Returns nullptr on failure.
    uint8_t* SetPayloadSize(size_t size);
    // Same as SetPayloadSize but doesn't guarantee to keep current payload.
    uint8_t* AllocatePayload(size_t size);
    
    // Helper function for Parse. Fill header fields using data in given buffer,
    // but does not touch packet own buffer, leaving packet in invalid state.
    bool Parse(const uint8_t* buffer, size_t size);

    // Header extensions
    template <typename Extension>
    bool HasExtension() const;
    bool HasExtension(ExtensionType type) const;

    // Removes extension of given |type|, returns false is extension was not
    // registered in packet's extension map or not present in the packet. Only
    // extension that should be removed must be registered, other extensions may
    // not be registered and will be preserved as is.
    bool RemoveExtension(ExtensionType type);

    // Returns whether there is an associated id for the extension and thus it is
    // possible to set the extension.
    template <typename Extension>
    bool IsRegistered() const;

    template <typename Extension, typename FirstValue, typename... Values>
    bool GetExtension(FirstValue, Values...) const;

    template <typename Extension>
    std::optional<typename Extension::value_type> GetExtension() const;

    template <typename Extension, typename... Values>
    bool SetExtension(const Values&...);

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

    const ExtensionInfo* FindExtensionInfo(int id) const;
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
    std::shared_ptr<ExtensionManager> extension_manager_;
    std::vector<ExtensionInfo> extension_entries_;
    std::vector<rtp::HeaderExtension> extensions_;
};

template <typename Extension>
bool RtpPacket::HasExtension() const {
    return HasExtension(Extension::kId);
}

template <typename Extension>
bool RtpPacket::IsRegistered() const {
   return extension_manager_->IsRegistered(Extension::kId);
}

template <typename Extension, typename FirstValue, typename... Values>
bool RtpPacket::GetExtension(FirstValue first, Values... values) const {
    auto raw = FindExtension(Extension::kId);
    if (raw.empty())
        return false;
    return Extension::Parse(raw, first, values...);
}

template <typename Extension>
std::optional<typename Extension::value_type> RtpPacket::GetExtension() const {
    std::optional<typename Extension::value_type> result;
    auto raw = FindExtension(Extension::kId);
    if (raw.empty() || !Extension::Parse(raw, &result.emplace()))
        result = std::nullopt;
    return result;
}

template <typename Extension, typename... Values>
bool RtpPacket::SetExtension(const Values&... values) {
    const size_t value_size = Extension::ValueSize(values...);
    auto buffer = AllocateExtension(Extension::kId, value_size);
    if (buffer.empty())
        return false;
    return Extension::Write(buffer, values...);
}

} // namespace naivertc


#endif