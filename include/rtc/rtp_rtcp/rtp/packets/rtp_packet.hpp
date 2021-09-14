#ifndef _RTC_RTP_PACKET_H_
#define _RTC_RTP_PACKET_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/base/packet.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extension_manager.hpp"

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
    using HeaderExtension = rtp::HeaderExtension;
public:
    RtpPacket();
    RtpPacket(size_t capacity);
    RtpPacket(const RtpPacket&);
    explicit RtpPacket(std::shared_ptr<ExtensionManager> extension_manager);
    RtpPacket(std::shared_ptr<ExtensionManager> extension_manager, size_t capacity);
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
    size_t payload_size() const { return payload_size_; }
    ArrayView<const uint8_t> payload() const {
        return ArrayView(cdata() + payload_offset_, payload_size_);
    }
    CopyOnWriteBuffer PayloadBuffer() const {
        return CopyOnWriteBuffer(cdata() + payload_offset_, payload_size_);
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
    void set_csrcs(ArrayView<const uint32_t> csrcs);

    void CopyHeaderFrom(const RtpPacket& other);
    bool SetPadding(uint8_t padding_size);

    void SetPayload(ArrayView<const uint8_t> payload);
    void SetPayload(const uint8_t* buffer, size_t size);
    // Reserve size_bytes for payload. Returns nullptr on failure.
    uint8_t* SetPayloadSize(size_t size);
    // Same as SetPayloadSize but doesn't guarantee to keep current payload.
    uint8_t* AllocatePayload(size_t size);
    
    bool Parse(const uint8_t* buffer, size_t size);

    // Header extensions
    template <typename Extension>
    bool HasExtension() const;
    bool HasExtension(ExtensionType type) const;

    bool RemoveExtension(ExtensionType type);

    template <typename Extension>
    bool IsRegistered() const;

    template <typename Extension>
    std::shared_ptr<Extension> GetExtension() const;

    template <typename Extension, typename... Values>
    bool SetExtension(const Values&... values);

    template <typename Extension>
    bool ReserveExtension();

    ArrayView<uint8_t> AllocateExtension(ExtensionType type, size_t size);
    ArrayView<const uint8_t> FindExtension(ExtensionType type) const;

private:
    inline void WriteAt(size_t offset, uint8_t byte);
    inline uint8_t* WriteAt(size_t offset);
    inline const uint8_t* ReadAt(size_t offset) const;

    bool ParseInternal(const uint8_t* buffer, size_t size);

    // Extension methods
    ArrayView<uint8_t> AllocateRawExtension(int id, size_t size);
    uint16_t UpdateaExtensionSizeByAddZeroPadding(size_t extensions_offset);
    void PromoteToTwoByteHeaderExtension();

private:
    struct ExtensionInfo {
        explicit ExtensionInfo(uint8_t id) : ExtensionInfo(id, 0, 0) {}
        ExtensionInfo(uint8_t id, uint8_t size, uint16_t offset)
            : id(id), size(size), offset(offset) {}
        uint8_t id;
        uint8_t size;
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
};

template <typename Extension>
bool RtpPacket::HasExtension() const {
    return HasExtension(Extension::kType);
}

template <typename Extension>
bool RtpPacket::IsRegistered() const {
   return extension_manager_->IsRegistered(Extension::kId);
}

template <typename Extension>
std::shared_ptr<Extension> RtpPacket::GetExtension() const {
    std::shared_ptr<Extension> result = std::make_shared<Extension>();
    auto raw = FindExtension(Extension::kType);
    if (raw.empty() || result->Parse(raw.data(), raw.size())) {
        return nullptr;
    }
    return std::move(result);
}

template <typename Extension, typename... Values>
bool RtpPacket::SetExtension(const Values&... values) {
    const size_t value_size = Extension::ValueSize(values...);
    auto buffer = AllocateExtension(Extension::kType, value_size);
    if (buffer.empty())
        return false;
    return Extension::PackInto(buffer.data(), buffer.size(), values...);
}

template <typename Extension>
bool RtpPacket::ReserveExtension() {
    auto buffer = AllocateExtension(Extension::kType, Extension::kValueSizeBytes);
    if (buffer.empty())
        return false;
    memset(buffer.data(), 0, Extension::kValueSizeBytes);
    return true;
}

} // namespace naivertc


#endif