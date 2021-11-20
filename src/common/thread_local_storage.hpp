#ifndef _RTC_COMMON_THREAD_LOCAL_STORAGE_H_
#define _RTC_COMMON_THREAD_LOCAL_STORAGE_H_

#include "base/defines.hpp"

namespace naivertc {

class RTC_CPP_EXPORT ThreadLocalStorage {
public:
    static void* GetSpecific();
    static void SetSpecific(void* value);
};

} // namespace naivertc

#endif