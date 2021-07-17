#ifndef _RTC_DATA_CHANNEL_H_
#define _RTC_DATA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/transports/sctp_transport.hpp"
#include "rtc/sdp/sdp_defines.hpp"

#include <memory>
#include <queue>

namespace naivertc {

class RTC_CPP_EXPORT DataChannel : public std::enable_shared_from_this<DataChannel> {
public:
    struct RTC_CPP_EXPORT Config {
        std::string label;
        std::string protocol;
        std::optional<StreamId> stream_id;

        Config(const std::string label, const std::string protocol = "", std::optional<StreamId> stream_id = std::nullopt);
    };
public:
    DataChannel(const StreamId stream_id, const std::string label, const std::string protocol);
    virtual ~DataChannel();

    StreamId stream_id() const;
    const std::string label() const;
    const std::string protocol() const;
    void HintStreamIdForRole(sdp::Role role);

private:
    StreamId stream_id_;
    std::string label_;
    std::string protocol_;

    std::queue<SctpPacket> recv_message_queue_;
};

} // namespace naivertc

#endif