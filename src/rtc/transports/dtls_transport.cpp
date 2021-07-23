#include "rtc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <sys/time.h>
#include <future>

namespace naivertc {

DtlsTransport::DtlsTransport(std::shared_ptr<IceTransport> lower, const Config config) 
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

bool DtlsTransport::HandleVerify(std::string_view fingerprint) {
    return task_queue_.SyncPost<bool>([this, &fingerprint]() -> bool {
        return verify_callback_ != nullptr ? verify_callback_(fingerprint) : false;
    });
}

bool DtlsTransport::Start() { 
    return task_queue_.SyncPost<bool>([this](){
        if (is_stoped_) {
            this->UpdateState(State::CONNECTING);
            // Start to handshake
            this->InitHandshake();
            // TODO: Do we should use delay post to check handshake timeout in 30s here?
            RegisterIncoming();
            is_stoped_ = false;
        }
        return true;
    });
}

bool DtlsTransport::Stop() {
    return task_queue_.SyncPost<bool>([this](){
        if (!is_stoped_) {
            SSL_shutdown(this->ssl_);
            this->ssl_ = NULL;
            DeregisterIncoming();
            is_stoped_ = true;
        }
        return true;
    });
}

void DtlsTransport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {
    task_queue_.Post([this, packet = std::move(packet), callback](){
        bool sent_size = SendInternal(std::move(packet));
        callback(sent_size);
    });
}

int DtlsTransport::Send(std::shared_ptr<Packet> packet) {
    return task_queue_.SyncPost<int>([this, packet = std::move(packet)](){
        return SendInternal(std::move(packet));
    });
}

void DtlsTransport::Incoming(std::shared_ptr<Packet> in_packet) {
    task_queue_.Post([this, in_packet = std::move(in_packet)](){
        if (!in_packet || !ssl_) {
            return;
        }
        try {
            PLOG_VERBOSE << "Incoming DTLS packet size: " << in_packet->size();

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
                PLOG_VERBOSE << "DTLS is not connected yet.";
                return;
            }

            uint8_t read_buffer[DEFAULT_SSL_BUFFER_SIZE];
            int read_size = SSL_read(ssl_, read_buffer, DEFAULT_SSL_BUFFER_SIZE);

            // Read failed
            if (!openssl::check(ssl_, read_size)) {
                PLOG_ERROR << "Failed to read from ssl ";
                return;
            }

            PLOG_VERBOSE << "SSL read size: " << read_size;

            if (read_size > 0) {
                ForwardIncomingPacket(Packet::Create(read_buffer, size_t(read_size)));
            }

        }catch (const std::exception& exp) {
            PLOG_WARNING << "Error occurred when processing incoming packet: " << exp.what();
        }
    });
}

int DtlsTransport::HandleDtlsWrite(const char* in_data, int in_size) {
    return task_queue_.SyncPost<int>([this, in_data, in_size](){
        auto bytes = reinterpret_cast<const uint8_t*>(in_data);
        auto pakcet = Packet::Create(std::move(bytes), in_size);
        return Outgoing(std::move(pakcet));
    });
}
    
int DtlsTransport::Outgoing(std::shared_ptr<Packet> out_packet) {
    if (out_packet->dscp() == 0) {
        out_packet->set_dscp(curr_dscp_);
    }
    return ForwardOutgoingPacket(std::move(out_packet));
}

int DtlsTransport::SendInternal(std::shared_ptr<Packet> packet) {
     if (!packet || state() != State::CONNECTED) {
        return -1;
    }

    this->curr_dscp_ = packet->dscp();
    int ret = SSL_write(this->ssl_, packet->data(), int(packet->size()));

    if (openssl::check(this->ssl_, ret)) {
        PLOG_VERBOSE << "Did send size=" << ret;
        return ret;
    }else {
        PLOG_VERBOSE << "Failed to send size=" << ret;
        return -1;
    }
    
}
    
} // namespace naivertc

