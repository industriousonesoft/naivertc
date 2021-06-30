#ifndef _PC_TRANSPORT_H_
#define _PC_TRANSPORT_H_

#include "base/defines.hpp"

#include <sigslot.h>

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT Transport: sigslot::has_slots<> {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        COMPLETED,
        FAILED
    };
public:
    Transport(std::shared_ptr<Transport> lower = nullptr);
    virtual ~Transport();

    sigslot::signal1<State> SignalStateChanged;

private:
    std::shared_ptr<Transport> lower_;

};

}

#endif