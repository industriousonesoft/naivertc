#include "rtc/base/copy_on_write_buffer.hpp"

namespace naivertc {

CopyOnWriteBuffer::CopyOnWriteBuffer() : buffer_(nullptr) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(const CopyOnWriteBuffer& other) 
    : buffer_(other.buffer_) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(CopyOnWriteBuffer && other) 
    : buffer_(std::move(other.buffer_)) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(size_t size) 
    : buffer_(size > 0 ? std::make_shared<BinaryBuffer>(size) : nullptr) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(size_t size, size_t capacity) 
    : buffer_((size > 0 || capacity > 0) ? std::make_shared<BinaryBuffer>(size) : nullptr) {
    if (buffer_) {
        assert(capacity >= size);
        buffer_->reserve(capacity);
    }
}

CopyOnWriteBuffer::CopyOnWriteBuffer(const uint8_t* data, size_t size) 
    : CopyOnWriteBuffer(data, size, size) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(const uint8_t* data, size_t size, size_t capacity)
    : CopyOnWriteBuffer(size, capacity) {
    if (buffer_) {
        std::memcpy(buffer_->data(), data, size);
    }
}

CopyOnWriteBuffer::~CopyOnWriteBuffer() = default;

const uint8_t* CopyOnWriteBuffer::data() const {
    return buffer_ != nullptr ? buffer_->data() : nullptr;
}

uint8_t* CopyOnWriteBuffer::data() {
    return buffer_ != nullptr ? buffer_->data() : nullptr;
}

size_t CopyOnWriteBuffer::size() const {
    return buffer_ != nullptr ? buffer_->size() : 0;
}

size_t CopyOnWriteBuffer::capacity() const {
    return buffer_ != nullptr ? buffer_->capacity() : 0;
}

CopyOnWriteBuffer& CopyOnWriteBuffer::operator=(const CopyOnWriteBuffer& other) {
    if (&other != this) {
        buffer_ = other.buffer_;
    }
    return *this;
}

CopyOnWriteBuffer& CopyOnWriteBuffer::operator=(CopyOnWriteBuffer&& other) {
    if (&other != this) {
        buffer_ = std::move(other.buffer_);
    }
    return *this;
}

bool CopyOnWriteBuffer::operator==(const CopyOnWriteBuffer& other) const {
    return size() == other.size() && 
           (data() == other.data() || (memcmp(data(), other.data(), size())));
}

uint8_t CopyOnWriteBuffer::operator[](size_t index) const {
    assert(buffer_ != nullptr);
    assert(index < buffer_->size());
    return data()[index];
}

void CopyOnWriteBuffer::Assign(const uint8_t* data, size_t size) {
    if (!buffer_) {
        buffer_ = size > 0 ? std::make_shared<BinaryBuffer>(data, data + size) : nullptr;
    }else if (buffer_.use_count() == 1) {
        if (size > buffer_->capacity()) {
            buffer_->reserve(size);
        }
        buffer_->assign(data, data + size);
    }else {
        size_t capacity = std::max(buffer_->capacity(), size);
        buffer_ = std::make_shared<BinaryBuffer>(data, data + size);
        buffer_->reserve(capacity);
    }
}

void CopyOnWriteBuffer::Resize(size_t size) {
    if (!buffer_) {
        if (size > 0) {
            buffer_ = std::make_shared<BinaryBuffer>(size);
        }
        return;
    }
    if (buffer_.use_count() == 1) {
        if (size > buffer_->capacity()) {
            buffer_->reserve(size);
        }
        buffer_->resize(size);
    }else {
        size_t capacity = std::max(buffer_->capacity(), size);
        buffer_ = std::make_shared<BinaryBuffer>(size);
        buffer_->reserve(capacity);
    }
}

void CopyOnWriteBuffer::Clear() {
    if (buffer_ == nullptr) {
        return;
    }
    if (buffer_.use_count() == 1) {
        buffer_->clear();
    }else {
        size_t capacity = buffer_->capacity();
        buffer_ = std::make_shared<BinaryBuffer>(0);
        buffer_->reserve(capacity);
    }
}

} // namespace naivertc