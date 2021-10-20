#include "common/weak_ptr_manager.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

class Common_WeakPtrManagerTest : public testing::Test {
public:
    void SetUp() override {
        WeakPtrManager::SharedInstance()->Register(this);
    };

    void TearDown() override {
        WeakPtrManager::SharedInstance()->Deregister(this);
    };
};

TEST_F(Common_WeakPtrManagerTest, Lock) {
    auto ret = WeakPtrManager::SharedInstance()->Lock(this);
    EXPECT_EQ(ret.has_value(), true);
}

} // namespace test
} // namespace naivertc