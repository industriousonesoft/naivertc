#include "stream/media_file_stream_source.hpp"

#include <limits>


MediaFileStreamSource::MediaFileStreamSource(std::string directory, std::string extension, int samplesPerSecond, bool loop) 
    : directory_(std::move(directory)),
      extension_(std::move(extension)),
      loop_(loop),
      counter_(-1),
      next_sample_time_ms_(std::numeric_limits<int64_t>::max()),
      sample_duration_ms_(1000 / samplesPerSecond),
      is_stoped_(true),
      sample_callback_(nullptr) {

}

MediaFileStreamSource::~MediaFileStreamSource() = default;

void MediaFileStreamSource::Start() {
    task_queue_.Async([this](){
        is_stoped_ = false;
        this->LoadNextSample();
    });
}

void MediaFileStreamSource::Stop() {
    task_queue_.Async([this](){
        is_stoped_ = true;
    });
}

void MediaFileStreamSource::OnSampleAvailable(SampleAvailableCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
        this->sample_callback_ = std::move(callback);
    });
}

// Protected methods
const MediaFileStreamSource::Sample MediaFileStreamSource::CreateSample(std::ifstream& source) {
    return Sample((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
}

// Private methods
void MediaFileStreamSource::LoadNextSample() {
    if (is_stoped_) {
        return;
    }

    int64_t start_ms = this->clock_.now_ms();

    std::string frame_id = std::to_string(++counter_);

    std::string file_path = directory_ + "/sample-" + frame_id + extension_;
    std::ifstream source(file_path, std::ios_base::binary);
    if (!source) {
        if (loop_ && counter_ > 0) {
            counter_ = -1;
            LoadNextSample();
            return;
        }
        if (sample_callback_) {
            sample_callback_({}, clock_.now_ms());
        }
        is_stoped_ = true;
        return;
    }

    const auto sample = CreateSample(source);
    
    int64_t end_ms = clock_.now_ms();
    if (sample_callback_) {
        sample_callback_(std::move(sample), end_ms);
    }

    int64_t delay_ms = 0;
    int64_t eslapsed_ms = end_ms - start_ms;
    if (eslapsed_ms < sample_duration_ms_) {
        delay_ms = sample_duration_ms_ - eslapsed_ms;
    }
    task_queue_.AsyncAfter(delay_ms, [this](){
        this->LoadNextSample();
    });
}
