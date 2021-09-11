#ifndef _RTC_BASE_COPY_ON_WRITE_BUFFER_H_
#define _RTC_BASE_COPY_ON_WRITE_BUFFER_H_

#include "base/defines.hpp"

#include <memory>
#include <type_traits>

namespace naivertc {

class RTC_CPP_EXPORT CopyOnWriteBuffer {
public:
    CopyOnWriteBuffer();
    CopyOnWriteBuffer(const CopyOnWriteBuffer&);
    CopyOnWriteBuffer(CopyOnWriteBuffer &&);

    explicit CopyOnWriteBuffer(size_t size);
    CopyOnWriteBuffer(size_t size, size_t capacity);

    CopyOnWriteBuffer(const uint8_t* data, size_t size);
    CopyOnWriteBuffer(const uint8_t* data, size_t size, size_t capacity);

    CopyOnWriteBuffer& operator=(const CopyOnWriteBuffer& other);
    CopyOnWriteBuffer& operator=(CopyOnWriteBuffer&& other);
   
    virtual ~CopyOnWriteBuffer();

    const uint8_t* data() const;
    uint8_t* data();

    size_t size() const;
    size_t capacity() const;

    bool operator==(const CopyOnWriteBuffer& other) const;
    bool operator!=(const CopyOnWriteBuffer& other) const {
        return !(*this == other);
    }
    uint8_t operator[](size_t index) const;

    void Assign(const uint8_t* data, size_t size);
    void Resize(size_t size);
    void Clear();

private:
    std::shared_ptr<BinaryBuffer> buffer_ = nullptr;
};

} // namespace naivertc

#endif