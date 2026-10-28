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

std::thread            worker_thread;
std::atomic<bool>      worker_stop { false };
std::mutex             worker_mutex;

void run ()
{
    std::set<std::string> known;
    bool first_pass = true;
    while (!worker_stop.load ()) {
        auto ports = list_ports ();
        std::set<std::string> current;
        for (auto& p : ports) current.insert (p.port);
        if (!first_pass) {
            for (auto& p : ports) {
                if (known.find (p.port) == known.end ())
                    dispatchHotplugEvent (p, true);
            }
            for (auto& old : known) {
                if (current.find (old) == current.end ()) {
                    PortInfo removed; removed.port = old;
                    dispatchHotplugEvent (removed, false);
                }
            }
        }
        known = std::move (current);
        first_pass = false;
        for (int i = 0; i < 10 && !worker_stop.load (); ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }
}

} // namespace

bool startHotplugWorker ()
{
    std::lock_guard<std::mutex> lock (worker_mutex);
    if (worker_thread.joinable ()) return true;
    worker_stop.store (false);
    worker_thread = std::thread (&run);
    return true;
}

void stopHotplugWorker ()
{
    std::lock_guard<std::mutex> lock (worker_mutex);
    worker_stop.store (true);
    if (worker_thread.joinable ()) worker_thread.join ();
}

} // namespace detail
} // namespace serial

#endif // __APPLE__
