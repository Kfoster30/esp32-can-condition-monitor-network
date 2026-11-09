#include "TemperatureState.hpp"

// These assertions execute at compile time in every firmware build. They
// exercise the actual C++ state functions rather than a Python reimplementation.
namespace {
constexpr auto boot = temperature::initial();
constexpr auto first = temperature::observe(boot, true, 240, 2000);
constexpr auto qualified = temperature::observe(first, true, 250, 4000);
constexpr auto failed = temperature::observe(qualified, false, 0, 6000);
constexpr auto recovery1 = temperature::observe(failed, true, 260, 8000);
constexpr auto recovery2 = temperature::observe(recovery1, true, 270, 10000);

static_assert(temperature::quality(boot, 0) == temperature::Quality::kMissing, "Boot is missing");
static_assert(temperature::wireValue(boot, 0) == protocol::kTemperatureUnavailable, "No fake boot value");
static_assert(temperature::quality(first, 2000) == temperature::Quality::kMissing, "Two good reads required");
static_assert(temperature::quality(qualified, 4000) == temperature::Quality::kValid, "Qualified reading");
static_assert(temperature::wireValue(qualified, 4000) == 250, "Qualified temperature encoded");
static_assert(temperature::quality(failed, 6000) == temperature::Quality::kStale, "Failed read marks stale");
static_assert(temperature::wireValue(failed, 6000) == 250, "Stale retains labeled last value");
static_assert(temperature::quality(failed, 9000) == temperature::Quality::kStale, "Age boundary inclusive");
static_assert(temperature::quality(failed, 9001) == temperature::Quality::kMissing, "Old value expires");
static_assert(temperature::quality(recovery1, 8000) == temperature::Quality::kStale, "Recovery qualification");
static_assert(temperature::quality(recovery2, 10000) == temperature::Quality::kValid, "Recovery completes");
static_assert(temperature::wireValue(recovery2, 10000) == 270, "Recovery uses new measurement");
static_assert(temperature::warnings(failed, 6000) == 128, "Stale warning bit");
static_assert(temperature::warnings(failed, 10000) == 64, "Missing warning bit");
constexpr temperature::State nearWrap = {true, 2, 240, 0xFFFFFF00u};
static_assert(temperature::quality(nearWrap, 100) == temperature::Quality::kValid, "Millis rollover");
static_assert(protocol::validTemperatureEncoding(64, -32768), "Missing encoding valid");
static_assert(!protocol::validTemperatureEncoding(0, -32768), "Unlabeled sentinel rejected");
static_assert(!protocol::validTemperatureEncoding(192, -32768), "Conflicting quality rejected");
static_assert(!protocol::sessionFresh(false, 0, 0, 250), "Unknown session is not attributable");
static_assert(protocol::sessionFresh(true, 100, 350, 250), "Session age boundary inclusive");
static_assert(!protocol::sessionFresh(true, 100, 351, 250), "Old session marker expires");
static_assert(!protocol::sessionFresh(true, 100, 600, 250), "Pre-restart marker expires during setup");
static_assert(protocol::sessionFresh(true, 0xFFFFFF00u, 0, 300), "Session millis rollover");
}  // namespace
