#include "common/weak_ptr_manager.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

class WeakPtrManagerTest : public testing::Test {
public:
    void SetUp() override {
        WeakPtrManager::SharedInstance()->Register(this);
    };

    void TearDown() override {
        WeakPtrManager::SharedInstance()->Deregister(this);
    };
};

TEST_F(WeakPtrManagerTest, Lock) {
    auto ret = WeakPtrManager::SharedInstance()->Lock(this);
    EXPECT_EQ(ret.has_value(), true);
}

} // namespace test
} // namespace naivertc