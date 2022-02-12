#include "rtc/transports/base_transport.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

BaseTransport::BaseTransport(BaseTransport* lower)
    : attached_queue_(TaskQueueImpl::Current()),
      lower_(lower),
      is_stoped_(true),
      state_(State::DISCONNECTED) {}

BaseTransport::~BaseTransport() = default;

bool BaseTransport::is_stoped() const {
    RTC_RUN_ON(&sequence_checker_);
    return is_stoped_;
}

BaseTransport::State BaseTransport::state() const {
    RTC_RUN_ON(&sequence_checker_);
    return state_;
}

void BaseTransport::OnStateChanged(StateChangedCallback callback) {
    RTC_RUN_ON(&sequence_checker_);
    state_changed_callback_ = std::move(callback);
}

// Protected methods
void BaseTransport::UpdateState(State state) {
    RTC_RUN_ON(&sequence_checker_);
    if (state_ == state) {
        return;
    }
    state_ = state;
    if (state_changed_callback_) {
        state_changed_callback_(state);
    }
}

int BaseTransport::ForwardOutgoingPacket(CopyOnWriteBuffer packet, PacketOptions options) {
    RTC_RUN_ON(&sequence_checker_);
    try {
        if (lower_) {
            return lower_->Send(std::move(packet), std::move(options));
        } else {
            return -1;
        }
    }catch (std::exception& e) {
        PLOG_WARNING << "Failed to forward outgoing packet: " << e.what();
        return -1;
    }
}

void BaseTransport::ForwardIncomingPacket(CopyOnWriteBuffer packet) {
    RTC_RUN_ON(&sequence_checker_);
    try {
        if (packet_recv_callback_) {
            packet_recv_callback_(std::move(packet));
        }
    } catch (std::exception& e) {
        PLOG_WARNING << "Failed to forward incoming packet: " << e.what();
    }
}

void BaseTransport::RegisterIncoming() {
    RTC_RUN_ON(&sequence_checker_);
    if (lower_) {
        PLOG_VERBOSE << "Registering incoming callback";
        lower_->packet_recv_callback_ = std::bind(&BaseTransport::Incoming, this, std::placeholders::_1);
    }
}

void BaseTransport::DeregisterIncoming() {
    RTC_RUN_ON(&sequence_checker_);
    if (lower_) {
        lower_->packet_recv_callback_ = nullptr;
        PLOG_VERBOSE << "Deregistered incoming callback";
    }
}

} // namespace naivertc