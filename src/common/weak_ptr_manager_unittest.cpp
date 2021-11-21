#include "common/weak_ptr_manager.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

class T(WeakPtrManagerTest) : public testing::Test {
public:
    void SetUp() override {
        WeakPtrManager::SharedInstance()->Register(this);
    };

    void TearDown() override {
        WeakPtrManager::SharedInstance()->Deregister(this);
    };
};

MY_TEST_F(WeakPtrManagerTest, Lock) {
    auto ret = WeakPtrManager::SharedInstance()->Lock(this);
    EXPECT_EQ(ret.has_value(), true);
}

} // namespace test
} // namespace naivertc