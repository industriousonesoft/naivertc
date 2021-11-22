#include "rtc/base/units/unit_relative.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

using namespace naivertc;

namespace naivertc {
namespace {
class TestUnit final : public RelativeUnit<TestUnit> {
public:
    TestUnit() = delete;

    using UnitBase::FromValue;
    using UnitBase::ToValue;
    using UnitBase::ToValueOr;

    template <typename T> 
    static constexpr TestUnit FromKilo(T kilo) {
        return FromFraction(1000, kilo);
    } 

    template <typename T = int64_t> 
    T ToKilo() const {
        return UnitBase::ToFraction<1000, T>();
    }

    constexpr int64_t ToKiloOr(int64_t fallback) const {
        return UnitBase::ToFractionOr<1000>(fallback);
    }

    template <typename T>
    constexpr T ToMilli() const {
        return UnitBase::ToMultiple<1000, T>();
    }

private:
    friend class UnitBase<TestUnit>;
    static constexpr bool one_sided = false;
    using RelativeUnit<TestUnit>::RelativeUnit;
};

constexpr TestUnit TestUnitAddKilo(TestUnit value, int add_kilo) {
    value += TestUnit::FromKilo(add_kilo);
    return value;
}
} // namespace

namespace test {

MY_TEST(UnitBaseTest, ConstExpr) {
    constexpr int64_t kValue = -12345;
    constexpr TestUnit kTestUnitZero = TestUnit::Zero();
    constexpr TestUnit kTestUnitMaxValue = TestUnit::PlusInfinity();
    constexpr TestUnit kTestUnitMinValue = TestUnit::MinusInfinity();

    static_assert(kTestUnitZero.IsZero(), "");
    static_assert(kTestUnitMaxValue.IsMax(), "");
    static_assert(kTestUnitMinValue.IsMin(), "");
    static_assert(kTestUnitMaxValue.ToKiloOr(-1) == -1, "");

    static_assert(kTestUnitMaxValue > kTestUnitZero, "");

    constexpr TestUnit kTestUnitKilo = TestUnit::FromKilo(kValue);
    constexpr TestUnit kTestUnitValue = TestUnit::FromValue(kValue);

    static_assert(kTestUnitKilo.ToKiloOr(0) == kValue, "");
    static_assert(kTestUnitValue.ToValueOr(0) == kValue, "");
    static_assert(TestUnitAddKilo(kTestUnitValue, 2).ToValue() == kValue + 2000, "");
}

MY_TEST(UnitBaseTest, GetBackSameValues) {
    const int64_t kValue = 499;
    for (int sign = -1; sign <= 1; ++sign) {
        int64_t value = kValue * sign;
        EXPECT_EQ(TestUnit::FromKilo(value).ToKilo(), value);
        EXPECT_EQ(TestUnit::FromValue(value).ToValue<int64_t>(), value);
    }
    EXPECT_EQ(TestUnit::Zero().ToValue<int64_t>(), 0);
}

MY_TEST(UnitBaseTest, GetDifferentPrefix) {
    const int64_t kValue = 3000000;
    EXPECT_EQ(TestUnit::FromValue(kValue).ToKilo(), kValue / 1000);
    EXPECT_EQ(TestUnit::FromKilo(kValue).ToValue<int64_t>(), kValue * 1000);
}

MY_TEST(UnitBaseTest, ConvertsToAndFromDouble) {
    const int64_t kValue = 17017;
    const double kMilliDouble = kValue * 1e3;
    const double kValueDouble = kValue;
    const double kKiloDouble = kValue * 1e-3;

    EXPECT_EQ(TestUnit::FromValue(kValue).ToKilo<double>(), kKiloDouble);
    EXPECT_EQ(TestUnit::FromKilo(kKiloDouble).ToValue<int64_t>(), kValue);

    EXPECT_EQ(TestUnit::FromValue(kValue).ToValue<double>(), kValueDouble);
    EXPECT_EQ(TestUnit::FromValue(kValueDouble).ToValue<int64_t>(), kValue);

    EXPECT_NEAR(TestUnit::FromValue(kValue).ToMilli<double>(), kMilliDouble, 1);

    const double kMaxValue = std::numeric_limits<double>::infinity();
    const double kMinValue = -kMaxValue;

    EXPECT_EQ(TestUnit::PlusInfinity().ToKilo<double>(), kMaxValue);
    EXPECT_EQ(TestUnit::MinusInfinity().ToKilo<double>(), kMinValue);
    EXPECT_EQ(TestUnit::PlusInfinity().ToValue<double>(), kMaxValue);
    EXPECT_EQ(TestUnit::MinusInfinity().ToValue<double>(), kMinValue);
    EXPECT_EQ(TestUnit::PlusInfinity().ToMilli<double>(), kMaxValue);
    EXPECT_EQ(TestUnit::MinusInfinity().ToMilli<double>(), kMinValue);

    EXPECT_TRUE(TestUnit::FromKilo(kMaxValue).IsMax());
    EXPECT_TRUE(TestUnit::FromKilo(kMinValue).IsMin());
    EXPECT_TRUE(TestUnit::FromValue(kMaxValue).IsMax());
    EXPECT_TRUE(TestUnit::FromValue(kMinValue).IsMin());
}

MY_TEST(UnitBaseTest, InfinityOperations) {
  const int64_t kValue = 267;
  const TestUnit finite = TestUnit::FromKilo(kValue);
  EXPECT_TRUE((TestUnit::PlusInfinity() + finite).IsMax());
  EXPECT_TRUE((TestUnit::PlusInfinity() - finite).IsMax());
  EXPECT_TRUE((finite + TestUnit::PlusInfinity()).IsMax());
  EXPECT_TRUE((finite - TestUnit::MinusInfinity()).IsMax());

  EXPECT_TRUE((TestUnit::MinusInfinity() + finite).IsMin());
  EXPECT_TRUE((TestUnit::MinusInfinity() - finite).IsMin());
  EXPECT_TRUE((finite + TestUnit::MinusInfinity()).IsMin());
  EXPECT_TRUE((finite - TestUnit::PlusInfinity()).IsMin());
}
    
} // namespace test
} // namespace naivert 
