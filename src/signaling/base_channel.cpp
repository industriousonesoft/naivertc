#include "signaling/base_channel.hpp"

namespace naivertc {
namespace signaling {

BaseChannel::BaseChannel(Observer* observer) 
: observer_(observer) {}

}
}