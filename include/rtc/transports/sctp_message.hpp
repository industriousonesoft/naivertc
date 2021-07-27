#ifndef _RTC_SCTP_PACKET_H_
#define _RTC_SCTP_PACKET_H_

#include "base/defines.hpp"
#include "base/packet.hpp"

#include <chrono>
#include <variant>

namespace naivertc {

class RTC_CPP_EXPORT SctpMessage final: public Packet {
public:
    enum class Type {
        BINARY,
        STRING,
        CONTROL,
        RESET
    };
    struct Reliability {
        enum class Policy { NONE = 0, RTX, TTL };

        Policy policy = Policy::NONE;
        // Data received in the same order it was sent. 
        bool unordered = false;
        std::variant<int, std::chrono::milliseconds> rexmit;
    };
public:
    static std::shared_ptr<SctpMessage> Create(const char* data, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr) {
        // 使用reinterpret_cast(re+interpret+cast：重新诠释转型)对data中的数据格式进行重新映射: char -> byte
        auto bytes = reinterpret_cast<const uint8_t*>(data);
        return std::shared_ptr<SctpMessage>(new SctpMessage(bytes, size, type, stream_id, reliability));
    }

    static std::shared_ptr<SctpMessage> Create(const uint8_t* bytes, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr) {
        return std::shared_ptr<SctpMessage>(new SctpMessage(bytes, size, type, stream_id, reliability));
    }

    static std::shared_ptr<SctpMessage> Create(std::vector<uint8_t>&& bytes, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr) {
        return std::shared_ptr<SctpMessage>(new SctpMessage(std::move(bytes), type, stream_id, reliability));
    }
  
    ~SctpMessage();

    Type type() const { return type_; }
    StreamId stream_id() const { return stream_id_; }
    const std::shared_ptr<Reliability> reliability() const { return reliability_; }

    size_t payload_size() {
        return (type_ == Type::BINARY || type_ == Type::STRING) ? size() : 0;
    }

    bool is_empty() const {
        return Packet::is_empty();
    }
   
protected:
    SctpMessage(const uint8_t* data, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr);
    SctpMessage(std::vector<uint8_t>&& bytes, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability = nullptr);

private:
    Type type_;
    StreamId stream_id_;
    std::shared_ptr<Reliability> reliability_;
};

}

#endif