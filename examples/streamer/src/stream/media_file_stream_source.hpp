#ifndef _MEDIA_FILE_PARSER_H_
#define _MEDIA_FILE_PARSER_H_

#include "stream/media_stream_source.hpp"

#include <rtc/base/task_utils/task_queue.hpp>
#include <rtc/base/time/clock_real_time.hpp>

#include <string>
#include <fstream>

class MediaFileStreamSource : public MediaStreamSource {
public:
    MediaFileStreamSource(const std::string directory, const std::string extension, int samplesPerSecond, bool loop = true);
    virtual ~MediaFileStreamSource();

    virtual void Start() override;
    virtual void Stop() override;
    void OnSampleAvailable(SampleAvailableCallback callback) override;

protected:
    virtual Sample CreateSample(std::ifstream& source);

private:
    void LoadNextSample();
protected:
    const std::string directory_;
    const std::string extension_;
    bool loop_;
    uint32_t counter_;
    int64_t next_sample_time_ms_;
    int64_t sample_duration_ms_;
    bool is_stoped_;

    SampleAvailableCallback sample_callback_;

    naivertc::RealTimeClock clock_;
    naivertc::TaskQueue task_queue_;
};

#endif