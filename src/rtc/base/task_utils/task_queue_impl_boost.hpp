#ifndef _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_BOOST_H_
#define _RTC_BASE_TASK_UTILS_TASK_QUEUE_IMPL_BOOST_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/thread/thread.hpp>

#include <list>
#include <string>

namespace naivertc {

std::unique_ptr<TaskQueueImpl, TaskQueueImpl::Deleter> CreateTaskQueueBoost(std::string_view name);

} // namespace naivertc


#endif