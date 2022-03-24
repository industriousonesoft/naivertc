#include "stream/media_file_stream_source.hpp"

#include "rtc/base/units/time_delta.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#include <limits>
#include <plog/Log.h>

MediaFileStreamSource::MediaFileStreamSource(std::string directory, std::string extension, int samplesPerSecond, bool loop) 
    : directory_(std::move(directory)),
      extension_(std::move(extension)),
      loop_(loop),
      counter_(-1),
      next_sample_time_ms_(std::numeric_limits<int64_t>::max()),
      sample_duration_ms_(1000 / samplesPerSecond),
      is_stoped_(true),
      sample_callback_(nullptr),
      worker_queue_(naivertc::TaskQueueImpl::Current()) {
    assert(worker_queue_ != nullptr);
}

MediaFileStreamSource::~MediaFileStreamSource() = default;

void MediaFileStreamSource::Start() {
    RTC_RUN_ON(&sequence_checker_);
    is_stoped_ = false;
    LoadNextSample();
}

void MediaFileStreamSource::Stop() {
    RTC_RUN_ON(&sequence_checker_);
    is_stoped_ = true;
    sample_callback_ = nullptr;
}

bool MediaFileStreamSource::IsRunning() const {
    RTC_RUN_ON(&sequence_checker_);
    return is_stoped_ == false;
}

void MediaFileStreamSource::OnSampleAvailable(SampleAvailableCallback callback) {
    RTC_RUN_ON(&sequence_checker_);
    this->sample_callback_ = std::move(callback);
}

// Protected methods
void MediaFileStreamSource::GenerateSample(std::ifstream& source, int64_t now_ms) {
    Sample sample((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    if (sample_callback_) {
        sample_callback_(std::move(sample), false, now_ms);
    }
}

// Private methods
void MediaFileStreamSource::LoadNextSample() {
    RTC_RUN_ON(&sequence_checker_);
    if (is_stoped_) {
        return;
    }

    int64_t start_ms = this->clock_.now_ms();

    std::string frame_id = std::to_string(++counter_);

    std::string file_path = directory_ + "sample-" + frame_id + extension_;
    std::ifstream source(file_path, std::ios_base::binary);
    if (!source) {
        if (loop_ && counter_ > 0) {
            counter_ = -1;
            LoadNextSample();
            PLOG_VERBOSE << "Start a new loop.";
            return;
        }
        if (sample_callback_) {
            sample_callback_({}, false, clock_.now_ms());
        }
        is_stoped_ = true;
        PLOG_VERBOSE << "Media file source stoped.";
        return;
    }

    int64_t now_ms = clock_.now_ms();
    GenerateSample(source, now_ms);
    
    int64_t delay_ms = 0;
    int64_t eslapsed_ms = now_ms - start_ms;
    if (eslapsed_ms < sample_duration_ms_) {
        delay_ms = sample_duration_ms_ - eslapsed_ms;
        PLOG_VERBOSE_IF(false) << "Load next sample in " << delay_ms << " ms.";
    }
    worker_queue_->PostDelayed(naivertc::TimeDelta::Millis(delay_ms), [this](){
        this->LoadNextSample();
    });
}
