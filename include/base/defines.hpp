#ifndef _BASE_DEFINES_H_
#define _BASE_DEFINES_H_

#ifndef RTC_CPP_EXPORT
#define RTC_CPP_EXPORT
#endif

#define DISALLOW_COPY_AND_ASSIGN(TypeName)  \
    TypeName(const TypeName&) = delete;     \
    TypeName& operator=(const TypeName&) = delete

#include <cstdint>

using TimeInterval = long;
using StreamId = uint16_t;
const StreamId STREAM_ID_MAX_VALUE = 65535;

#endif



