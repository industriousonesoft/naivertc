#ifndef _AYAME_CHANNEL_H_
#define _AYAME_CHANNEL_H_

#include "channels/base_channel.hpp"

namespace signaling {

class AyameChannel : public BaseChannel {
public:
    AyameChannel(boost::asio::io_context& ioc, Observer* observer);
    ~AyameChannel() override;
    
private:
    void DoRegister() override;
    bool OnIncomingMessage(std::string text) override;

    void DoSendPong();
};

} // namespace signaling

#endif
