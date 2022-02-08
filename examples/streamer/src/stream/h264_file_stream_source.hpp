#ifndef _H264_FILE_STREAM_SOURCE_H_
#define _H264_FILE_STREAM_SOURCE_H_

#include "stream/media_file_stream_source.hpp"

class H264FileStreamSource final : public MediaFileStreamSource {
public:
    H264FileStreamSource(const std::string directory, int fps, bool loop = true);
    ~H264FileStreamSource();
private:
    void GenerateSample(std::ifstream& source, int64_t now_ms) override;
};

#endif