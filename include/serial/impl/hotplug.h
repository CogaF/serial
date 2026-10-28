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

namespace serial {
namespace detail {

// Push an event out to every registered callback. Called by the
// platform-specific hotplug thread when it detects an add/remove.
void dispatchHotplugEvent (const PortInfo& info, bool added);

// Start / stop the platform-specific hotplug worker. The subsystem
// starts on the first registration and stops when the last callback is
// unregistered. Returns true if the worker is now running (or already
// was); false if hotplug is unsupported on this build (e.g. Linux
// without libudev).
bool startHotplugWorker ();
void stopHotplugWorker  ();

} // namespace detail
} // namespace serial

#endif // SERIAL_IMPL_HOTPLUG_H
