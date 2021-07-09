#ifndef _PC_SCTP_PACKET_H_
#define _PC_SCTP_PACKET_H_

#include "base/defines.hpp"
#include "base/packet.hpp"

#include <chrono>
#include <variant>

namespace naivertc {

class RTC_CPP_EXPORT SctpPacket final: public Packet {
public:
    enum class Type {
        BINARY,
        STRING,
        CONTROL,
        RESET
    };
    struct Reliability {
        enum class Policy { NONE = 0, RTX, TTL };

        Policy polity = Policy::NONE;
        // Data received in the same order it was sent. 
        bool ordered = true;
        std::variant<int, std::chrono::milliseconds> rexmit;
    };
public:
    static std::shared_ptr<SctpPacket> Create(const char* data, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr) {
        // 使用reinterpret_cast(re+interpret+cast：重新诠释转型)对data中的数据格式进行重新映射: char -> byte
        auto bytes = reinterpret_cast<const std::byte*>(data);
        return std::shared_ptr<SctpPacket>(new SctpPacket(bytes, size, type, stream_id, reliability));
    }

    static std::shared_ptr<SctpPacket> Create(const std::byte* bytes, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr) {
        return std::shared_ptr<SctpPacket>(new SctpPacket(bytes, size, type, stream_id, reliability));
    }

    static std::shared_ptr<SctpPacket> Create(std::vector<std::byte>&& bytes, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr) {
        return std::shared_ptr<SctpPacket>(new SctpPacket(std::move(bytes), type, stream_id, reliability));
    }
  
    ~SctpPacket();

    Type type() const { return type_; }
    StreamId stream_id() const { return stream_id_; }
    const std::shared_ptr<Reliability> reliability() const { return reliability_; }

    size_t message_size() {
        return (type_ == Type::BINARY || type_ == Type::STRING) ? size() : 0;
    }

    bool is_empty() const {
        return Packet::is_empty();
    }
   
protected:
    SctpPacket(const std::byte* data, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr);
    SctpPacket(std::vector<std::byte>&& bytes, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr);

private:
    Type type_;
    StreamId stream_id_;
    std::shared_ptr<Reliability> reliability_;
};

}

#endif