#include "pc/transport.hpp"

namespace naivertc {

Transport::Transport(std::shared_ptr<Transport> lower) 
    : lower_(lower) {}

Transport::~Transport() {

}

}