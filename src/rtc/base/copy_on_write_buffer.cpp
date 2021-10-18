#include "rtc/base/copy_on_write_buffer.hpp"

#include <plog/Log.h>

namespace naivertc {

CopyOnWriteBuffer::CopyOnWriteBuffer() : buffer_(nullptr) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(const CopyOnWriteBuffer& other) 
    : buffer_(other.buffer_) {
    // PLOG_DEBUG << "Called copy consrtuctor.";
}

CopyOnWriteBuffer::CopyOnWriteBuffer(CopyOnWriteBuffer&& other) 
    : buffer_(std::move(other.buffer_)) {
    // PLOG_DEBUG << "Called move consrtuctor.";
}

CopyOnWriteBuffer::CopyOnWriteBuffer(const BinaryBuffer& other_buffer) 
    : buffer_(std::make_shared<BinaryBuffer>(other_buffer)) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(BinaryBuffer&& other_buffer) 
    : buffer_(std::make_shared<BinaryBuffer>(std::move(other_buffer))) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(size_t size) 
    : buffer_(size > 0 ? std::make_shared<BinaryBuffer>(size) : nullptr) {}

CopyOnWriteBuffer::CopyOnWriteBuffer(size_t size, size_t capacity) 
    : buffer_((size > 0 || capacity > 0) ? std::make_shared<BinaryBuffer>(size) : nullptr) {
    if (buffer_) {
        assert(capacity >= size);
        buffer_->reserve(capacity);
    }
}

CopyOnWriteBuffer& CopyOnWriteBuffer::operator=(const CopyOnWriteBuffer& other) {
    if (&other != this) {
        buffer_ = other.buffer_;
    }
    // PLOG_DEBUG << "Called copy =.";
    return *this;
}

CopyOnWriteBuffer& CopyOnWriteBuffer::operator=(CopyOnWriteBuffer&& other) {
    if (&other != this) {
        buffer_ = std::move(other.buffer_);
    }
    // PLOG_DEBUG << "Called move =.";
    return *this;
}

CopyOnWriteBuffer::~CopyOnWriteBuffer() {
    buffer_.reset();
};

const uint8_t* CopyOnWriteBuffer::data() const {
    return cdata();
}

const uint8_t* CopyOnWriteBuffer::cdata() const {
    return buffer_ != nullptr ? buffer_->data() : nullptr;
}

uint8_t* CopyOnWriteBuffer::data() {
    if (!buffer_) {
        return nullptr;
    }
    CloneIfNecessary(capacity());
    return buffer_->data();
}

size_t CopyOnWriteBuffer::size() const {
    return buffer_ != nullptr ? buffer_->size() : 0;
}

size_t CopyOnWriteBuffer::capacity() const {
    return buffer_ != nullptr ? buffer_->capacity() : 0;
}

bool CopyOnWriteBuffer::operator==(const CopyOnWriteBuffer& other) const {
    return size() == other.size() && 
           (cdata() == other.cdata() || (memcmp(cdata(), other.cdata(), size()) == 0));
}

uint8_t& CopyOnWriteBuffer::operator[](size_t index) {
    return at(index);
}

const uint8_t& CopyOnWriteBuffer::operator[](size_t index) const {
    return at(index);
}

uint8_t& CopyOnWriteBuffer::at(size_t index) {
    assert(buffer_ != nullptr);
    assert(index < buffer_->size());
    return data()[index];
}

const uint8_t& CopyOnWriteBuffer::at(size_t index) const {
    assert(buffer_ != nullptr);
    assert(index < buffer_->size());
    return cdata()[index];
}

std::vector<uint8_t>::iterator CopyOnWriteBuffer::begin() {
    if (!buffer_) {
        buffer_ = std::make_shared<BinaryBuffer>();
    } else {
        CloneIfNecessary(capacity());
    }
    return buffer_->begin();
}

std::vector<uint8_t>::iterator CopyOnWriteBuffer::end() {
    if (!buffer_) {
        buffer_ = std::make_shared<BinaryBuffer>();
    } else {
        CloneIfNecessary(capacity());
    }
    return buffer_->end();
}

std::vector<uint8_t>::const_iterator CopyOnWriteBuffer::cbegin() const {
    // This const_const is not pretty, but the alternative is to declare 
    // the member as mutable.
    const_cast<CopyOnWriteBuffer*>(this)->CreateEmptyBufferIfNecessary();
    return buffer_->cbegin();
}

std::vector<uint8_t>::const_iterator CopyOnWriteBuffer::cend() const {
    const_cast<CopyOnWriteBuffer*>(this)->CreateEmptyBufferIfNecessary();
    return buffer_->cend();
}

std::vector<uint8_t>::reverse_iterator CopyOnWriteBuffer::rbegin() {
    if (!buffer_) {
        buffer_ = std::make_shared<BinaryBuffer>();
    } else {
        CloneIfNecessary(capacity());
    }
    return buffer_->rbegin();
}

std::vector<uint8_t>::reverse_iterator CopyOnWriteBuffer::rend() {
    if (!buffer_) {
        buffer_ = std::make_shared<BinaryBuffer>();
    } else {
        CloneIfNecessary(capacity());
    }
    return buffer_->rend();
}

std::vector<uint8_t>::const_reverse_iterator CopyOnWriteBuffer::crbegin() const {
    const_cast<CopyOnWriteBuffer*>(this)->CreateEmptyBufferIfNecessary();
    return buffer_->crbegin();
}

std::vector<uint8_t>::const_reverse_iterator CopyOnWriteBuffer::crend() const {
    const_cast<CopyOnWriteBuffer*>(this)->CreateEmptyBufferIfNecessary();
    return buffer_->crend();
}

void CopyOnWriteBuffer::Append(std::vector<uint8_t>::const_iterator begin, 
                               std::vector<uint8_t>::const_iterator end) {
    if (!buffer_) {
        buffer_ = std::make_shared<BinaryBuffer>(begin, end);
        return;
    }
    CloneIfNecessary(capacity());
    buffer_->insert(buffer_->end(), begin, end);
}

void CopyOnWriteBuffer::Insert(std::vector<uint8_t>::iterator pos, 
                               std::vector<uint8_t>::const_iterator begin, 
                               std::vector<uint8_t>::const_iterator end) {
    assert(buffer_ != nullptr);
    CloneIfNecessary(buffer_->capacity());                 
    buffer_->insert(pos, begin, end);
}

void CopyOnWriteBuffer::Resize(size_t size) {
    if (!buffer_) {
        if (size > 0) {
            buffer_ = std::make_shared<BinaryBuffer>(size);
        }
        return;
    }
    size_t new_capacity = std::max(buffer_->capacity(), size);
    CloneIfNecessary(new_capacity);
    buffer_->resize(size);
}

void CopyOnWriteBuffer::Clear() {
    if (buffer_ == nullptr) {
        return;
    }
    if (buffer_.use_count() == 1) {
        buffer_->clear();
    } else {
        size_t capacity = buffer_->capacity();
        buffer_ = std::make_shared<BinaryBuffer>(0);
        buffer_->reserve(capacity);
    }
}

void CopyOnWriteBuffer::Swap(CopyOnWriteBuffer& other) {
    if (buffer_ == nullptr) {
        buffer_ = std::move(other.buffer_);
        other.buffer_ = nullptr;
        return;
    } else {
        CloneIfNecessary(buffer_->capacity());
    }

    if (other.buffer_ == nullptr) {
        other.buffer_ = std::move(buffer_);
        buffer_ = nullptr;
    } else {
        buffer_->swap(*other.buffer_.get());
    }
}

void CopyOnWriteBuffer::EnsureCapacity(size_t new_capacity) {
    if (!buffer_) {
        buffer_ = std::make_shared<BinaryBuffer>();
        buffer_->reserve(new_capacity);
        return;
    } else if (new_capacity <= capacity()) {
        return;
    }
    CloneIfNecessary(new_capacity);
}

// Private methods
void CopyOnWriteBuffer::CloneIfNecessary(size_t new_capacity) {
    if (buffer_.use_count() == 1) {
        if (new_capacity > capacity()) {
            buffer_->reserve(new_capacity);
        }
        return;
    }
    buffer_ = std::make_shared<BinaryBuffer>(buffer_->data(), buffer_->data() + buffer_->size());
    buffer_->reserve(new_capacity);
}

void CopyOnWriteBuffer::CreateEmptyBufferIfNecessary() {
    if (!buffer_) {
        buffer_ = std::make_shared<BinaryBuffer>();
    }
}

} // namespace naivertc