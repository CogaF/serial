/*!
 * \file serial/impl/hotplug.h
 *
 * Internal declarations for the hotplug callback subsystem. The public
 * entry points (registerHotplugCallback / unregisterHotplugCallback)
 * live in serial::namespace in serial/serial.h.
 */

#ifndef SERIAL_IMPL_HOTPLUG_H
#define SERIAL_IMPL_HOTPLUG_H

#include "serial/serial.h"

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace serial {
namespace detail {

// Push an event out to every registered callback. Called by the
// platform-specific hotplug thread when it detects an add/remove.
void dispatchHotplugEvent (const PortInfo& info, bool added);

// Lists the ports and reports the ones added and removed since `known`
// (the names last seen; updated). The first call (first == true) only
// fills `known`. Used by the workers that compare lists.
void comparePorts (std::set<std::string>& known, bool first);

// Start / stop the platform-specific hotplug worker. The subsystem
// starts on the first registration and stops when the last callback is
// unregistered. Returns true if the worker is now running (or already
// was); false if hotplug is unsupported on this build (e.g. Linux
// without libudev).
bool startHotplugWorker ();
void stopHotplugWorker  ();

// The platform worker's thread. Kept in a function-local static (see each
// platform's worker()), so it is created after the callback registry and
// destroyed before it: a program that ends without unregistering stops and
// joins it here instead of reaching std::terminate (a std::thread still
// running at exit). stop() called from the worker itself (a callback that
// unregisters) detaches instead of joining itself.
class HotplugThread {
public:
    ~HotplugThread () { stop (); }
    bool running () const { return thread_.joinable (); }
    template <class F> void start (F f)
    {
        stop_.store (false);
        thread_ = std::thread (f);
    }
    void stop ()
    {
        stop_.store (true);
        if (!thread_.joinable ()) return;
        if (thread_.get_id () == std::this_thread::get_id ()) thread_.detach ();
        else thread_.join ();
    }
    bool stopping () const { return stop_.load (); }
    std::mutex& mutex () { return mutex_; }
private:
    std::thread       thread_;
    std::atomic<bool> stop_ { false };
    std::mutex        mutex_;
};

} // namespace detail
} // namespace serial

#endif // SERIAL_IMPL_HOTPLUG_H
