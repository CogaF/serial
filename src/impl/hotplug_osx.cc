#if defined(__APPLE__)

/* macOS hotplug monitor. A full IOKit IOServiceAddMatchingNotification
 * implementation would be preferable; this build uses a periodic
 * list_ports() diff instead to avoid a hard CoreFoundation run-loop
 * dependency inside the serial library. (polling fallback) */

#include "serial/impl/hotplug.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace serial {
namespace detail {

namespace {

HotplugThread& worker ()
{
    static HotplugThread w;
    return w;
}

void run ()
{
    std::set<std::string> known;
    bool first = true;
    while (!worker ().stopping ()) {
        comparePorts (known, first);
        first = false;
        for (int i = 0; i < 10 && !worker ().stopping (); ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }
}

} // namespace

bool startHotplugWorker ()
{
    std::lock_guard<std::mutex> lock (worker ().mutex ());
    if (!worker ().running ()) worker ().start (&run);
    return true;
}

void stopHotplugWorker ()
{
    std::lock_guard<std::mutex> lock (worker ().mutex ());
    worker ().stop ();
}

} // namespace detail
} // namespace serial

#endif // __APPLE__
