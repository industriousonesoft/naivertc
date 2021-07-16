#ifndef _RTC_SDP_ENTRY_H_
#define _RTC_SDP_ENTRY_H_

#include "base/defines.hpp"

#include <memory>

namespace naivertc {
namespace sdp {

struct RTC_CPP_EXPORT Entry : public std::enable_shared_from_this<Entry> {
public:
    enum class Type {
        SESSION,
        MEDIA
    };
public:
    virtual ~Entry() = default;

};

} // namespace sdp
} // namespace naivert 

#endif