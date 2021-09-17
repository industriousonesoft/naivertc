#include "rtc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <sys/time.h>
#include <future>

namespace naivertc {

DtlsTransport::DtlsTransport(Configuration config, std::weak_ptr<IceTransport> lower, std::shared_ptr<TaskQueue> task_queue) 
    : Transport(lower, std::move(task_queue)),
      config_(std::move(config)),
      is_client_(lower.lock() != nullptr ? lower.lock()->role() == sdp::Role::ACTIVE : false),
      handshake_packet_options_(DSCP::DSCP_AF21 /* Assured Forwarding class 2, low drop precedence, the recommendation for high-priority data in RFC 8837 */),
      user_packet_options_(DSCP::DSCP_DF) {
    InitOpenSSL(config_);
    WeakPtrManager::SharedInstance()->Register(this);
}

DtlsTransport::~DtlsTransport() {
    DeinitOpenSSL();
    WeakPtrManager::SharedInstance()->Deregister(this);
}

void DtlsTransport::OnVerify(VerifyCallback callback) {
    task_queue_->Async([this, callback](){
        this->verify_callback_ = callback;
    });
}

bool DtlsTransport::HandleVerify(std::string_view fingerprint) {
    return task_queue_->Sync<bool>([this, &fingerprint]() -> bool {
        return this->verify_callback_ != nullptr ? this->verify_callback_(fingerprint) : false;
    });
}

bool DtlsTransport::is_client() const {
    return task_queue_->Sync<bool>([this]() -> bool {
        return this->is_client_; 
    });
}

bool DtlsTransport::Start() { 
    return task_queue_->Sync<bool>([this](){
        if (this->is_stoped_) {
            this->UpdateState(State::CONNECTING);
            // Start to handshake
            // TODO: Do we should use delay post to check handshake timeout in 30s here?
            this->InitHandshake();
            this->RegisterIncoming();
            this->is_stoped_ = false;
        }
        return true;
    });
}

bool DtlsTransport::Stop() {
    return task_queue_->Sync<bool>([this](){
        if (!this->is_stoped_) {
            // Cut down incomming data
            this->DeregisterIncoming();
            // Shutdown SSL connection
            SSL_shutdown(ssl_);
            this->ssl_ = NULL;
            this->is_stoped_ = true;
            UpdateState(State::DISCONNECTED);
        }
        return true;
    });
}

int DtlsTransport::Send(CopyOnWriteBuffer packet, const PacketOptions& options) {
    return task_queue_->Sync<int>([this, packet = std::move(packet), &options]() mutable {
        if (packet.empty() || this->state_ != State::CONNECTED) {
            return -1;
        }
        this->user_packet_options_ = options;
        int ret = SSL_write(this->ssl_, packet.cdata(), int(packet.size()));
        if (openssl::check(this->ssl_, ret)) {
            PLOG_VERBOSE << "Send size=" << ret;
            return ret;
        }else {
            PLOG_VERBOSE << "Failed to send size=" << ret;
            return -1;
        }
    });
}

void DtlsTransport::Incoming(CopyOnWriteBuffer in_packet) {
    task_queue_->Async([this, in_packet = std::move(in_packet)](){
        if (in_packet.empty() || !this->ssl_) {
            return;
        }
        try {
            // PLOG_VERBOSE << "Incoming DTLS packet size: " << in_packet.size();

            // Write into SSL in BIO, and will be retrieved by SSL_read
            BIO_write(this->in_bio_, in_packet.cdata(), int(in_packet.size()));

            // In non-blocking mode, We may try to do handshake multiple time. 
            if (this->state_ == State::CONNECTING) {
                if (this->TryToHandshake()) {
                    // DTLS Connected
                    this->UpdateState(State::CONNECTED);
                }else {
                    if (this->IsHandshakeTimeout()) {
                        this->UpdateState(State::FAILED);
                    }
                    return;
                }
            // Do SSL reading after connected
            }else if (this->state_ != State::CONNECTED) {
                PLOG_VERBOSE << "DTLS is not connected yet.";
                return;
            }

            int read_size = SSL_read(ssl_, ssl_read_buffer_, DEFAULT_SSL_BUFFER_SIZE);

            // Read failed
            if (!openssl::check(ssl_, read_size)) {
                PLOG_ERROR << "Failed to read from ssl: " << read_size;
                return;
            }

            // PLOG_VERBOSE << "SSL read size: " << read_size;

            if (read_size > 0) {
                this->ForwardIncomingPacket(CopyOnWriteBuffer(ssl_read_buffer_, read_size));
            }

        }catch (const std::exception& exp) {
            PLOG_WARNING << "Error occurred when processing incoming packet: " << exp.what();
        }
    });
}

int DtlsTransport::HandleDtlsWrite(const char* in_data, int in_size) {
    return task_queue_->Sync<int>([this, in_data, in_size](){
        auto bytes = reinterpret_cast<const uint8_t*>(in_data);
        if (this->state_ != State::CONNECTED) {
            return this->Outgoing(CopyOnWriteBuffer(bytes, in_size), this->handshake_packet_options_);
        }else {
            return this->Outgoing(CopyOnWriteBuffer(bytes, in_size), this->user_packet_options_);
        }
    });
}
    
int DtlsTransport::Outgoing(CopyOnWriteBuffer out_packet, const PacketOptions& options) {
    return ForwardOutgoingPacket(std::move(out_packet), options);
}
    
} // namespace naivertc

