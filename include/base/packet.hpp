#ifndef _BASE_PACKET_H_
#define _BASE_PACKET_H_

#include "base/defines.hpp"

#include <memory>
#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT Packet : public std::enable_shared_from_this<Packet> {
public:
    static std::shared_ptr<Packet> Create(const char* data, size_t size) {
        // 使用reinterpret_cast(re+interpret+cast：重新诠释转型)对data中的数据格式进行重新映射: char -> byte
        auto bytes = reinterpret_cast<const std::byte*>(data);
        return std::shared_ptr<Packet>(new Packet(bytes,  size));
    }
    static std::shared_ptr<Packet> Create(const std::byte* bytes, size_t size) {
        return std::shared_ptr<Packet>(new Packet(bytes, size));
    }

    virtual ~Packet();

    const char* data() const;
    char* data();
    size_t size() const;
    const std::vector<std::byte> bytes() const;

    unsigned int dscp() const { return dscp_; };
    void set_dscp(unsigned int dscp) { dscp_ = dscp; }

    bool is_empty() const { return bytes_.empty(); }

protected:
    Packet(const std::byte* bytes, size_t size);
    Packet(std::vector<std::byte>&& bytes);

    void Resize(size_t new_size);

private:
    std::vector<std::byte> bytes_;

    // Differentiated Services Code Point
    unsigned int dscp_;
};

}

#endif