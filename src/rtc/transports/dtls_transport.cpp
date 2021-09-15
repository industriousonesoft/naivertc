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
      curr_dscp_(0) {
    InitOpenSSL(config_);
    WeakPtrManager::SharedInstance()->Register(this);
}

DtlsTransport::~DtlsTransport() {
    DeinitOpenSSL();
    WeakPtrManager::SharedInstance()->Deregister(this);
}

void DtlsTransport::OnVerify(VerifyCallback callback) {
    task_queue_->Async([this, callback](){
        verify_callback_ = callback;
    });
}

bool DtlsTransport::HandleVerify(std::string_view fingerprint) {
    return task_queue_->Sync<bool>([this, &fingerprint]() -> bool {
        return verify_callback_ != nullptr ? verify_callback_(fingerprint) : false;
    });
}

bool DtlsTransport::is_client() const {
    return task_queue_->Sync<bool>([this]() -> bool {
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
        }
        return true;
    });
}

int DtlsTransport::Send(Packet packet) {
    return task_queue_->Sync<int>([this, packet = std::move(packet)]() mutable {
        if (packet.empty() || state_ != State::CONNECTED) {
            return -1;
        }
        curr_dscp_ = packet.dscp();
        int ret = SSL_write(ssl_, packet.cdata(), int(packet.size()));
        if (openssl::check(ssl_, ret)) {
            PLOG_VERBOSE << "Send size=" << ret;
            return ret;
        }else {
            PLOG_VERBOSE << "Failed to send size=" << ret;
            return -1;
        }
    });
}

void DtlsTransport::Incoming(Packet in_packet) {
    task_queue_->Async([this, in_packet = std::move(in_packet)](){
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
                }else {
                    if (IsHandshakeTimeout()) {
                        UpdateState(State::FAILED);
                    }
                    return;
                }
            // Do SSL reading after connected
            }else if (state_ != State::CONNECTED) {
                PLOG_VERBOSE << "DTLS is not connected yet.";
                return;
            }

            static uint8_t read_buffer[DEFAULT_SSL_BUFFER_SIZE];
            int read_size = SSL_read(ssl_, read_buffer, DEFAULT_SSL_BUFFER_SIZE);

            // Read failed
            if (!openssl::check(ssl_, read_size)) {
                PLOG_ERROR << "Failed to read from ssl: " << read_size;
                return;
            }

            // PLOG_VERBOSE << "SSL read size: " << read_size;

            if (read_size > 0) {
                ForwardIncomingPacket(Packet(read_buffer, read_size));
            }

        }catch (const std::exception& exp) {
            PLOG_WARNING << "Error occurred when processing incoming packet: " << exp.what();
        }
    });
}

int DtlsTransport::HandleDtlsWrite(const char* in_data, int in_size) {
    return task_queue_->Sync<int>([this, in_data, in_size](){
        auto bytes = reinterpret_cast<const uint8_t*>(in_data);
        return Outgoing(Packet(bytes, in_size));
    });
}
    
int DtlsTransport::Outgoing(Packet out_packet) {
    if (out_packet.dscp() == 0) {
        out_packet.set_dscp(curr_dscp_);
    }
    return ForwardOutgoingPacket(std::move(out_packet));
}
    
} // namespace naivertc

