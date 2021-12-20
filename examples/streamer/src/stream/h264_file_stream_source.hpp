#ifndef _H264_FILE_STREAM_SOURCE_H_
#define _H264_FILE_STREAM_SOURCE_H_

#include "stream/media_file_stream_source.hpp"

class H264FileStreamSource final : public MediaFileStreamSource {
public:
    class Frame : public Sample {
    public:
        Frame() : Sample(), 
                  is_key_frame_(false) {};

        bool is_key_frame() const { return is_key_frame_; };
        void set_is_key_frame(bool is_key) { is_key_frame_ = is_key; };
    private:
        bool is_key_frame_;
    };
public:
    H264FileStreamSource(const std::string directory, int fps, bool loop = true);
    ~H264FileStreamSource();
private:
    Sample CreateSample(std::ifstream& source) override;

};

#endif