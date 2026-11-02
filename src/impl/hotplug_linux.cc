#if defined(__linux__)

/* Linux hotplug monitor.
 *
 * When built against libudev (SERIAL_HAVE_LIBUDEV) the worker opens a
 * NETLINK_KOBJECT_UEVENT monitor filtered on the tty subsystem and
 * dispatches udev's own add/remove events. Without libudev (or if the
 * monitor cannot start) the worker compares list_ports() every second.
 * (polling fallback) */

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

HotplugThread& worker ()
{
    static HotplugThread w;
    return w;
}

#ifdef SERIAL_HAVE_LIBUDEV
// udev's own add / remove events; false if the monitor cannot start.
bool run_udev ()
{
    struct udev* u = udev_new ();
    if (!u) return false;
    struct udev_monitor* mon = udev_monitor_new_from_netlink (u, "udev");
    if (!mon) { udev_unref (u); return false; }
    udev_monitor_filter_add_match_subsystem_devtype (mon, "tty", nullptr);
    udev_monitor_enable_receiving (mon);
    int fd = udev_monitor_get_fd (mon);
    while (!worker ().stopping ()) {
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
    return true;
}
#endif

// Without libudev (or if its monitor cannot start): the port list is
// compared every second. (polling fallback)
void run_polling ()
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

void run ()
{
#ifdef SERIAL_HAVE_LIBUDEV
    if (run_udev ()) return;
#endif
    run_polling ();
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

#endif // __linux__
