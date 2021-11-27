#include "rtc/transports/transport.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

Transport::Transport(std::weak_ptr<Transport> lower) 
    : lower_(std::move(lower)), 
      is_stoped_(true),
      state_(State::DISCONNECTED) {}

Transport::~Transport() = default;

bool Transport::is_stoped() const {
    RTC_RUN_ON(&sequence_checker_);
    return is_stoped_;
}

Transport::State Transport::state() const {
    RTC_RUN_ON(&sequence_checker_);
    return state_;
}

void Transport::OnStateChanged(StateChangedCallback callback) {
    RTC_RUN_ON(&sequence_checker_);
    state_changed_callback_ = std::move(callback);
}

// Protected methods
void Transport::UpdateState(State state) {
    RTC_RUN_ON(&sequence_checker_);
    if (state_ == state) {
        return;
    }
    state_ = state;
    if (state_changed_callback_) {
        state_changed_callback_(state);
    }
}

int Transport::ForwardOutgoingPacket(CopyOnWriteBuffer packet, const PacketOptions& options) {
    RTC_RUN_ON(&sequence_checker_);
    try {
        if (auto lower = lower_.lock()) {
            return lower->Send(std::move(packet), options);
        } else {
            return -1;
        }
    }catch (std::exception& e) {
        PLOG_WARNING << "Failed to forward outgoing packet: " << e.what();
        return -1;
    }
}

void Transport::ForwardIncomingPacket(CopyOnWriteBuffer packet) {
    RTC_RUN_ON(&sequence_checker_);
    try {
        if (packet_recv_callback_) {
            packet_recv_callback_(std::move(packet));
        }
    } catch (std::exception& e) {
        PLOG_WARNING << "Failed to forward incoming packet: " << e.what();
    }
}

void Transport::RegisterIncoming() {
    RTC_RUN_ON(&sequence_checker_);
    if (auto lower = lower_.lock()) {
        PLOG_VERBOSE << "Registering incoming callback";
        lower->packet_recv_callback_ = std::bind(&Transport::Incoming, this, std::placeholders::_1);
    }
}

void Transport::DeregisterIncoming() {
    RTC_RUN_ON(&sequence_checker_);
    if (auto lower = lower_.lock()) {
        lower->packet_recv_callback_ = nullptr;
        PLOG_VERBOSE << "Deregistered incoming callback";
    }
}

} // namespace naivertc