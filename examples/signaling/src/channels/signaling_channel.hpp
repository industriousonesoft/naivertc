#ifndef _SIGNALING_CHANNELS_CHANNEL_H_
#define _SIGNALING_CHANNELS_CHANNEL_H_

#include <boost/asio/io_context.hpp>
#include <string>

namespace signaling {

class Channel {
public:
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void OnConnected() = 0;
        virtual void OnClosed(const std::string err_reason) = 0;
        // Return true for reading more.
        virtual bool OnRead(const std::string msg) = 0;
    };
public:
    virtual ~Channel() = default;
    virtual void Connect(std::string signaling_url, bool insecure) = 0;
    virtual void Close() = 0;
    virtual void Send(std::string msg) = 0;
};

// Create default signaling channel.
std::unique_ptr<Channel> CreateDefaultChannel(boost::asio::io_context& ioc, Channel::Observer* observer);

};

#endif