#if defined(__linux__)

/* Linux hotplug monitor.
 *
 * When built against libudev (SERIAL_HAVE_LIBUDEV) the worker opens a
 * NETLINK_KOBJECT_UEVENT monitor filtered on the tty subsystem and
 * dispatches udev's own add/remove events. Without libudev the worker
 * falls back to diffing list_ports() periodically. (polling fallback) */

#include "serial/impl/hotplug.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#ifdef SERIAL_HAVE_LIBUDEV
# include <libudev.h>
# include <poll.h>
# include <cstring>
#endif

namespace serial {
namespace detail {

namespace {

std::thread            worker_thread;
std::atomic<bool>      worker_stop { false };
std::mutex             worker_mutex;

#ifdef SERIAL_HAVE_LIBUDEV
void run_udev ()
{
    struct udev* u = udev_new ();
    if (!u) return;
    struct udev_monitor* mon = udev_monitor_new_from_netlink (u, "udev");
    if (!mon) { udev_unref (u); return; }
    udev_monitor_filter_add_match_subsystem_devtype (mon, "tty", nullptr);
    udev_monitor_enable_receiving (mon);
    int fd = udev_monitor_get_fd (mon);
    while (!worker_stop.load ()) {
        struct pollfd pfd = { fd, POLLIN, 0 };
        int r = ::poll (&pfd, 1, 200);
        if (r <= 0) continue;
        struct udev_device* dev = udev_monitor_receive_device (mon);
        if (!dev) continue;
        const char* action = udev_device_get_action (dev);
        const char* node   = udev_device_get_devnode (dev);
        if (action && node) {
            bool added = (std::strcmp (action, "add") == 0);
            bool removed = (std::strcmp (action, "remove") == 0);
            if (added || removed) {
                PortInfo info;
                info.port = node;
                const char* v = udev_device_get_property_value (dev, "ID_VENDOR_ID");
                const char* p = udev_device_get_property_value (dev, "ID_MODEL_ID");
                const char* s = udev_device_get_property_value (dev, "ID_SERIAL_SHORT");
                if (v) info.vid = v;
                if (p) info.pid = p;
                if (s) info.serial_number = s;
                dispatchHotplugEvent (info, added);
            }
        }
        udev_device_unref (dev);
    }
    udev_monitor_unref (mon);
    udev_unref (u);
}
#endif

void run_polling ()
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

void run ()
{
#ifdef SERIAL_HAVE_LIBUDEV
    run_udev ();
#else
    run_polling ();
#endif
}

} // namespace

bool startHotplugWorker ()
{
    std::lock_guard<std::mutex> lock (worker_mutex);
    if (worker_thread.joinable ()) return true;
#ifndef SERIAL_HAVE_LIBUDEV
    // Spec: return false (and thus -1 from register) when libudev is
    // not available. The polling fallback above is kept for builds that
    // explicitly want it, but the default is to signal "unsupported".
    return false;
#else
    worker_stop.store (false);
    worker_thread = std::thread (&run);
    return true;
#endif
}

void stopHotplugWorker ()
{
    std::lock_guard<std::mutex> lock (worker_mutex);
    worker_stop.store (true);
    if (worker_thread.joinable ()) worker_thread.join ();
}

} // namespace detail
} // namespace serial

#endif // __linux__
