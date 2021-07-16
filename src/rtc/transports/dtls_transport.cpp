#include "rtc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <sys/time.h>
#include <future>

namespace naivertc {

DtlsTransport::DtlsTransport(std::shared_ptr<IceTransport> lower, const Config& config) 
    : Transport(lower),
    config_(std::move(config)),
    is_client_(lower->role() == sdp::Role::ACTIVE),
    curr_dscp_(0) {

    InitOpenSSL(config_);
    WeakPtrManager::SharedInstance()->Register(this);
}

DtlsTransport::~DtlsTransport() {
    DeinitOpenSSL();
    WeakPtrManager::SharedInstance()->Deregister(this);
}

void DtlsTransport::OnVerify(VerifyCallback callback) {
    task_queue_.Post([this, callback](){
        verify_callback_ = callback;
    });
}

bool DtlsTransport::HandleVerify(const std::string& fingerprint) {
    return task_queue_.SyncPost<bool>([this, &fingerprint]() -> bool {
        return verify_callback_ != nullptr ? verify_callback_(fingerprint) : false;
    });
}

void DtlsTransport::Start(StartedCallback callback) { 
    Transport::Start([this, callback](std::optional<const std::exception> exp){
        if (exp) {
            if (callback) {
                callback(exp);
            }
            PLOG_ERROR << "Failed to start, err: " << exp->what();
            return;
        }
        try {
            this->UpdateState(State::CONNECTING);
            // Start to handshake
            this->InitHandshake();
            // TODO: Do we should use delay post to check handshake timeout in 30s here?
            if (callback) {
                callback(std::nullopt);
            }
        }catch(const std::exception& exp) {
            PLOG_ERROR << "Failed to start, err: " << exp.what();
            if (callback) {
                callback(exp);
            }
        }
    });
}

void DtlsTransport::Stop(StopedCallback callback) {
    Transport::Stop([this, callback](std::optional<const std::exception> exp){
        if (exp) {
            if (callback) {
                callback(exp);
            }
            PLOG_ERROR << "Failed to stop, err: " << exp->what();
            // return;
        }
        if (this->ssl_) {
            SSL_shutdown(this->ssl_);
            this->ssl_ = NULL;
        }
        if (callback) {
            callback(std::move(exp));
        }
    });
}

void DtlsTransport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {
    task_queue_.Post([this, packet, callback](){
        if (!packet || state() != State::CONNECTED) {
            if (callback) {
                callback(false);
            }
            return;
        }

        this->curr_dscp_ = packet->dscp();
        int ret = SSL_write(this->ssl_, packet->data(), int(packet->size()));
        int bRet = openssl::check(this->ssl_, ret);
        if (callback) {
            callback(bRet);
        }
    });
}

void DtlsTransport::Incoming(std::shared_ptr<Packet> in_packet) {
    if (!in_packet || !ssl_) {
        return;
    }
    try {
        // Write into SSL in BIO, and will be retrieved by SSL_read
        BIO_write(in_bio_, in_packet->data(), int(in_packet->size()));

        auto curr_state = state();

        // In non-blocking mode, We may try to do handshake multiple time. 
        if (curr_state == State::CONNECTING) {
            if (TryToHandshake()) {
                // DTLS Connected
                UpdateState(State::CONNECTED);
            }else {
                if (IsHandshakeTimeout()) {
                    UpdateState(State::FAILED);
                }
                return;
            }
        // Do SSL reading after connected
        }else if (curr_state != State::CONNECTED) {
            return;
        }

        std::byte read_buffer[DEFAULT_SSL_BUFFER_SIZE];
        int read_size = SSL_read(ssl_, read_buffer, DEFAULT_SSL_BUFFER_SIZE);

        // Read failed
        if (!openssl::check(ssl_, read_size)) {
            return;
        }

        if (read_size > 0) {
            HandleIncomingPacket(Packet::Create(read_buffer, size_t(read_size)));
        }

    }catch (const std::exception& exp) {
        PLOG_WARNING << "Error occurred when processing incoming packet: " << exp.what();
    }
}
    
void DtlsTransport::Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
    task_queue_.Post([this, out_packet, callback](){
        if (out_packet->dscp() == 0) {
            out_packet->set_dscp(curr_dscp_);
        }
        Transport::Outgoing(out_packet, callback);
    });
}
    
} // namespace naivertc

