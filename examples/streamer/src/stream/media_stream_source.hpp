#ifndef _MEDIA_STREAM_SOURCE_H_
#define _MEDIA_STREAM_SOURCE_H_

#include <cstdint>
#include <vector>
#include <functional>

class MediaStreamSource {
public:
    using Sample = std::vector<uint8_t>;
    using SampleAvailableCallback = std::function<void(const Sample sample, int64_t capture_time_ms)>;
public:
    virtual ~MediaStreamSource() = default;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void OnSampleAvailable(SampleAvailableCallback callback) = 0;
};

#endif