#include "PowerState.hpp"

namespace {
constexpr auto boot = power::initial();
constexpr auto first = power::observe(boot, true, 3332, 42, 100);
constexpr auto second = power::observe(first, true, 3332, 42, 210);
constexpr auto failed = power::observe(second, false, 0, 0, 320);
constexpr auto recover1 = power::observe(failed, true, 3332, 43, 430);
constexpr auto recover2 = power::observe(recover1, true, 3332, 43, 540);
static_assert(!power::available(boot, 0), "No fake startup power");
static_assert(!power::available(first, 100), "Two good acquisitions required");
static_assert(power::available(second, 210), "Qualified power");
static_assert(power::available(second, 510), "Freshness boundary inclusive");
static_assert(!power::available(second, 511), "Expired snapshot is unavailable");
static_assert(!power::available(failed, 320), "Read failure invalidates immediately");
static_assert(power::busValue(failed, 320) == 65535, "Missing bus sentinel");
static_assert(power::currentValue(failed, 320) == -32768, "Missing current sentinel");
static_assert(power::warnings(failed, 320) == 32, "Power missing warning");
static_assert(!power::available(recover1, 430), "Recovery needs qualification");
static_assert(power::available(recover2, 540), "Qualified recovery");
static_assert(power::currentValue(recover2, 540) == 43, "New current replaces missing");
constexpr auto afterStall = power::observe(second, true, 3332, 42, 600);
static_assert(!power::available(afterStall, 600), "Aged worker must requalify");
constexpr power::State beforeWrap = {2, 3300, 0, 0xFFFFFF00u};
static_assert(power::available(beforeWrap, 0), "Power freshness across millis wrap");
static_assert(!power::available(beforeWrap, 100), "Power expires across millis wrap");
static_assert(power::roundDeciMa(4250) == 43, "Positive tie rounds away from zero");
static_assert(power::roundDeciMa(-4250) == -43, "Negative tie rounds away from zero");
static_assert(power::roundDeciMa(-49) == 0, "Small signed current rounds to zero");
static_assert(protocol::validPowerEncoding(65535, -32768), "Paired missing sentinel");
static_assert(!protocol::validPowerEncoding(3300, -32768), "No half-missing current");
static_assert(!protocol::validPowerEncoding(65535, 0), "No half-missing bus");
static_assert(protocol::validPowerEncoding(0, 0), "Real zero remains numeric");
static_assert(protocol::validSourceFlags(7), "Mixed temperature and power source flags");
static_assert(protocol::validSourceFlags(15), "Combined temperature, power, and motion flags");
static_assert(!protocol::validSourceFlags(9), "Unsupported MPU-only mode rejected");
static_assert(!protocol::validSourceFlags(23), "Unknown source bits rejected");
static_assert(!power::available(power::observe(boot, true, 65535, -32768, 1), 1),
              "Missing sentinel cannot qualify as a valid observation");
}  // namespace
