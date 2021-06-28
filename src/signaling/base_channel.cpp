#include "signaling/base_channel.hpp"

namespace naivertc {
namespace signaling {

BaseChannel::BaseChannel(std::weak_ptr<Observer> observer) 
: observer_(observer) {}

}
}