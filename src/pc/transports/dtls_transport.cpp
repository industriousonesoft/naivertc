#include "pc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <sys/time.h>

namespace naivertc {

DtlsTransport::DtlsTransport(std::shared_ptr<IceTransport> lower, const Config& config) 
    : Transport(lower),
    config_(std::move(config)),
    is_client_(lower->role() == sdp::Role::ACTIVE) {
    InitOpenSSL(config_);
    WeakPtrManager::SharedInstance()->Register(this);
}

DtlsTransport::~DtlsTransport() {
    DeinitOpenSSL();
    WeakPtrManager::SharedInstance()->Deregister(this);
}

void DtlsTransport::OnVerify(VerifyCallback callback) {
    verify_callback_ = callback;
}

bool DtlsTransport::Verify(const std::string& fingerprint) {
    if (verify_callback_) {
        return verify_callback_(fingerprint);
    }else {
        return false;
    }
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

void DtlsTransport::Incoming(std::shared_ptr<Packet> in_packet) {
    if (!in_packet || !ssl_) {
        return;
    }
    try {
        // Write into SSL in BIO, and will be retrieved by SSL_read
        BIO_write(in_bio_, in_packet->data(), int(in_packet->size()));

        auto curr_state = state();

        // 非阻塞模式下，SSL_do_handshake可能需要调用多次才能握手成功
        if (curr_state == State::CONNECTING) {
            if (TryToHandshake()) {
                // DTLS Connected
                UpdateState(State::CONNECTED);
            }else {
                // TODO: To detect if the handshake was timeout
                return;
            }
        // 必须握手成功后才可以开始从SSL中读取数据
        }else if (curr_state != State::CONNECTED) {
            // TODO: To detect if the handshake was timeout
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

}
    
} // namespace naivertc

