#include "rtc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <sys/time.h>
#include <future>

namespace naivertc {

DtlsTransport::Configuration::Configuration(std::shared_ptr<Certificate> certificate, std::optional<size_t> mtu) 
 : certificate(std::move(certificate)),
   mtu(std::move(mtu)) {}

DtlsTransport::DtlsTransport(Configuration config, IceTransport* lower, TaskQueue* task_queue) 
    : Transport(lower, task_queue),
      config_(std::move(config)),
      is_client_(lower != nullptr ? lower->role() == sdp::Role::ACTIVE : false),
      handshake_packet_options_(DSCP::DSCP_AF21 /* Assured Forwarding class 2, low drop precedence, the recommendation for high-priority data in RFC 8837 */) {
    task_queue_->Async([this](){
        InitOpenSSL(config_);
        WeakPtrManager::SharedInstance()->Register(this);
    });
}

DtlsTransport::~DtlsTransport() {
    task_queue_->Async([this](){
        DeinitOpenSSL();
        WeakPtrManager::SharedInstance()->Deregister(this);
    });
}

void DtlsTransport::OnVerify(VerifyCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        verify_callback_ = callback;
    });
    
}

bool DtlsTransport::HandleVerify(std::string fingerprint) {
    return task_queue_->Sync<bool>([this, fingerprint=std::move(fingerprint)](){
        return verify_callback_ != nullptr ? verify_callback_(std::move(fingerprint)) : false;
    });
    
}

bool DtlsTransport::is_client() const {
    return task_queue_->Sync<bool>([this](){
        return is_client_;
    });
}

bool DtlsTransport::Start() {
    return task_queue_->Sync<bool>([this](){
        if (is_stoped_) {
            UpdateState(State::CONNECTING);
            // Start to handshake
            // TODO: Do we should use delay post to check handshake timeout in 30s here?
            InitHandshake();
            RegisterIncoming();
            is_stoped_ = false;
        }
        return true;      
    });

}

bool DtlsTransport::Stop() {
    return task_queue_->Sync<bool>([this](){
        if (!is_stoped_) {
            // Cut down incomming data
            DeregisterIncoming();
            // Shutdown SSL connection
            SSL_shutdown(ssl_);
            ssl_ = NULL;
            is_stoped_ = true;
            UpdateState(State::DISCONNECTED);
        }
        return true;
    });
}

int DtlsTransport::Send(CopyOnWriteBuffer packet, PacketOptions options) {
    return task_queue_->Sync<int>([this, packet=std::move(packet), options=std::move(options)](){
        if (packet.empty() || state_ != State::CONNECTED) {
            return -1;
        }
        user_packet_options_ = std::move(options);
        int ret = SSL_write(ssl_, packet.cdata(), int(packet.size()));
        if (openssl::check(ssl_, ret)) {
            PLOG_VERBOSE << "Send size=" << ret;
            return ret;
        } else {
            PLOG_VERBOSE << "Failed to send size=" << ret;
            return -1;
        }
    });

}

// Protected && private methods
void DtlsTransport::Incoming(CopyOnWriteBuffer in_packet) {
    RTC_RUN_ON(task_queue_);
    if (in_packet.empty() || !ssl_) {
        return;
    }
    try {
        // PLOG_VERBOSE << "Incoming DTLS packet size: " << in_packet.size();

        // Write into SSL in BIO, and will be retrieved by SSL_read
        BIO_write(in_bio_, in_packet.cdata(), int(in_packet.size()));

        // In non-blocking mode, We may try to do handshake multiple time. 
        if (state_ == State::CONNECTING) {
            if (TryToHandshake()) {
                // DTLS Connected
                UpdateState(State::CONNECTED);
            } else {
                if (IsHandshakeTimeout()) {
                    UpdateState(State::FAILED);
                }
                return;
            }
        // Do SSL reading after connected
        } else if (state_ != State::CONNECTED) {
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
            ForwardIncomingPacket(CopyOnWriteBuffer(ssl_read_buffer_, read_size));
        }

    }catch (const std::exception& exp) {
        PLOG_WARNING << "Error occurred when processing incoming packet: " << exp.what();
    }
}

int DtlsTransport::HandleDtlsWrite(CopyOnWriteBuffer data) {
    RTC_RUN_ON(task_queue_);
    if (state_ != State::CONNECTED) {
        return Outgoing(std::move(data), handshake_packet_options_);
    } else {
        PacketOptions options = user_packet_options_ ? std::move(*user_packet_options_): PacketOptions(DSCP::DSCP_DF);
        return Outgoing(std::move(data), std::move(options));
    }
}
    
int DtlsTransport::Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) {
    RTC_RUN_ON(task_queue_);
    return ForwardOutgoingPacket(std::move(out_packet), std::move(options));
}
    
} // namespace naivertc

