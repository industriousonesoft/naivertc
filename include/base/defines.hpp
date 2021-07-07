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

#endif



