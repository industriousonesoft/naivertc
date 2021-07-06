#ifndef _PC_DATA_CHANNEL_H_
#define _PC_DATA_CHANNEL_H_

#include "base/defines.hpp"
#include "pc/transports/sctp_transport.hpp"

#include <memory>
#include <queue>

namespace naivertc {

class RTC_CPP_EXPORT DataChannel : public std::enable_shared_from_this<DataChannel> {
public:
    DataChannel(StreamId stream_id, std::string label, std::string protocol);
    virtual ~DataChannel();

    StreamId stream_id() const;
    std::string label() const;
    std::string protocol() const;

private:
    StreamId stream_id_;
    std::string label_;
    std::string protocol_;

    std::queue<SctpMessage> recv_message_queue_;
};

} // namespace naivertc

#endif