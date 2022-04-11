#include "channels/base_channel.hpp"

#include <nlohmann/json.hpp>
#include <plog/Log.h>

namespace signaling {
namespace {

using json = nlohmann::json;

bool ParseURL(const std::string& signaling_url, URLParts& parts) {
  if (!URLParts::Parse(signaling_url, parts)) {
    throw std::exception();
  }

  std::string default_port;
  if (parts.scheme == "wss") {
    return true;
  } else if (parts.scheme == "ws") {
    return false;
  } else {
    throw std::exception();
  }
}

} // namespace

BaseChannel::BaseChannel(boost::asio::io_context& ioc, Observer* observer)
                        : ioc_(ioc),
                          observer_(observer),
                          is_connected_(false) {}

BaseChannel::~BaseChannel() {
    Close();
    if (ws_) {
        ws_.reset();
    }
    ice_servers_.clear();
}

void BaseChannel::Connect(Configuration config) {
    if (is_connected_ || is_connecting_) {
        return;
    }
    is_connecting_ = true;
    config_ = config;
    std::string signaling_url = config_.signaling_url;
    URLParts parts;
    
    if (config.ice_server_urls.size() > 0) {
        for (std::string url : config.ice_server_urls) {
            naivertc::IceServer ice_server(std::move(url));
            ice_servers_.push_back(ice_server);
        }
    }

    // FIXME: error occuring when signaling_url == ""
    if (ParseURL(signaling_url, parts)) {
        ws_.reset(new Websocket(ioc_, Websocket::ssl_tag(), config_.insecure));
    } else {
        ws_.reset(new Websocket(ioc_));
    }
    ws_->Connect(signaling_url,
                 std::bind(&BaseChannel::OnConnect, this,
                         std::placeholders::_1));
}

void BaseChannel::OnConnect(boost::system::error_code ec) {
    is_connecting_ = false;
    if (ec) {
        is_connected_ = false;
        observer_->OnClosed(ec);
        return;
    }
    is_connected_ = true;
    DoRead();
    DoRegister();
}

void BaseChannel::DoRead() {
    ws_->Read(std::bind(&BaseChannel::OnRead, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
}

void BaseChannel::Close() {
    if (!is_connected_ || is_closing_) {
        return;
    }
    is_closing_ = true;
    ws_->Close(std::bind(&BaseChannel::OnClose, this,
                       std::placeholders::_1));
}

void BaseChannel::OnClose(boost::system::error_code ec) {
    if (ec) {
        PLOG_WARNING << "Closed with error: " << ec.message();
    }
    is_closing_ = false;
    is_connected_ = false;
    observer_->OnClosed(ec);
}

void BaseChannel::OnRead(boost::system::error_code ec,
                     std::size_t bytes_transferred,
                     std::string text) {
   
    boost::ignore_unused(bytes_transferred);

    // Do not treat this as an error as this error will occur when the read process is canceled due to writing
    if (ec == boost::asio::error::operation_aborted)
        return;

    // If WebSocket returns a closed error, immediately call Close (); to exit the OnRead function.
    if (ec == boost::beast::websocket::error::closed) {
        // When WebSocket is closed by Close () ;, the function is called in the order of OnClose ();-> ReconnectAfter ();-> OnWatchdogExpired () ;.
        // WebSocket is reconnected
        Close();
        return;
    }

    if (ec) {
        PLOG_ERROR << "error: " << ec.message();
        observer_->OnClosed(ec);
        return;
    }

    // Check if we need to read more.
    if (!OnIncomingMessage(text)) {
        return;
    }

    // Read more data.
    DoRead();
    
}

void BaseChannel::SendSDP(std::string_view sdp, bool is_offer) {
    json json_message = {
        {"type", is_offer ? "offer" : "answer"}, 
        {"sdp", sdp}
    };
    ws_->WriteText(json_message.dump());
}

void BaseChannel::SendCandidate(std::string_view sdp_mid, 
                            int sdp_mlineindex, 
                            std::string_view candidate) {
    // ayame uses the `ice` property in exchange for candidate sdp. Note that it is not `candidate`
    json json_message = {
        {"type", "candidate"}
    };
    // Set candidate information as object in ice property and send
    json_message["ice"] = {{"candidate", candidate},
                            {"sdpMLineIndex", sdp_mlineindex},
                            {"sdpMid", sdp_mid}};
    ws_->WriteText(json_message.dump());
}

// Protected methods
void BaseChannel::DoRegister() {
    // Require overwritten by the derived class.
}
     
bool BaseChannel::OnIncomingMessage(std::string text) {
    // Require overwritten by the derived class.
    return false;
}

} // namespace signaling
