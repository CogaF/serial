/* Hotplug callback registry (platform-neutral layer).
 *
 * Platform-specific device monitors live in hotplug_win.cc,
 * hotplug_linux.cc and hotplug_osx.cc. They call dispatchHotplugEvent
 * when they observe an add or remove. */

#include "serial/impl/hotplug.h"

#include <map>
#include <mutex>

namespace serial {
namespace {

std::mutex& callbacks_mutex ()
{
    static std::mutex m;
    return m;
}

std::map<int, HotplugCallback>& callbacks ()
{
    static std::map<int, HotplugCallback> m;
    return m;
}

int& next_token ()
{
    static int t = 1;
    return t;
}

} // namespace

namespace detail {

void dispatchHotplugEvent (const PortInfo& info, bool added)
{
    std::map<int, HotplugCallback> snapshot;
    {
        std::lock_guard<std::mutex> lock (callbacks_mutex ());
        snapshot = callbacks ();
    }
    for (auto& kv : snapshot) {
        try { kv.second (info, added); } catch (...) { /* swallow */ }
    }
}

} // namespace detail

int registerHotplugCallback (HotplugCallback cb)
{
    bool first = false;
    int token;
    {
        std::lock_guard<std::mutex> lock (callbacks_mutex ());
        first = callbacks ().empty ();
        token = next_token ()++;
        callbacks ().emplace (token, std::move (cb));
    }
    if (first) {
        if (!detail::startHotplugWorker ()) {
            std::lock_guard<std::mutex> lock (callbacks_mutex ());
            callbacks ().erase (token);
            return -1;
        }
    }
    return token;
}

void unregisterHotplugCallback (int token)
{
    bool last = false;
    {
        std::lock_guard<std::mutex> lock (callbacks_mutex ());
        callbacks ().erase (token);
        last = callbacks ().empty ();
    }
    if (last) {
        detail::stopHotplugWorker ();
    }
}

} // namespace serial
