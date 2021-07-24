#ifndef _TASK_QUEUE_TESTS_H_
#define _TASK_QUEUE_TESTS_H_

#include <common/task_queue.hpp>

namespace taskqueue {

class Example {
public:
    Example();
    ~Example();

    void DelayPost();
    void Post();

private:
    naivertc::TaskQueue task_queue_;

};

} // namespace taskqueue


#endif