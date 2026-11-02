#if defined(_WIN32)

/* Windows hotplug monitor. The worker thread owns a message-only window
 * registered with RegisterDeviceNotification for every device interface
 * class: when a device arrives or goes away (WM_DEVICECHANGE), the port
 * list is compared a moment later (the COM name is registered shortly
 * after the arrival). A comparison every 5 s also catches drivers that
 * announce nothing; if the window cannot be made, every second.
 * (event driven, polling fallback) */

#include "serial/impl/hotplug.h"
#include "serial/impl/win_compat.h"

#include <dbt.h>

#include <chrono>
#include <set>
#include <string>

namespace serial {
namespace detail {

namespace {

HotplugThread& worker ()
{
    static HotplugThread w;
    return w;
}

// Set by the window procedure (same thread as the loop below).
bool device_changed = false;

LRESULT CALLBACK window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_DEVICECHANGE &&
        (wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE ||
         wparam == DBT_DEVNODES_CHANGED)) {
        device_changed = true;
    }
    return DefWindowProcW (hwnd, msg, wparam, lparam);
}

void run ()
{
    const wchar_t* class_name = L"serial_hotplug_window";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof (wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleW (nullptr);
    wc.lpszClassName = class_name;
    RegisterClassExW (&wc); // fails harmlessly if already registered
    HWND hwnd = CreateWindowExW (0, class_name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                 nullptr, wc.hInstance, nullptr);
    HDEVNOTIFY notify = nullptr;
    if (hwnd) {
        DEV_BROADCAST_DEVICEINTERFACE_W filter = {};
        filter.dbcc_size = sizeof (filter);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        notify = RegisterDeviceNotificationW (hwnd, &filter,
            DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    }
    const auto safety = std::chrono::milliseconds (notify ? 5000 : 1000);

    std::set<std::string> known;
    comparePorts (known, true);
    auto last = std::chrono::steady_clock::now ();
    bool pending = false;
    auto pending_since = last;
    while (!worker ().stopping ()) {
        // Wait for window messages or 100 ms, then handle them.
        MsgWaitForMultipleObjects (0, nullptr, FALSE, 100, QS_ALLINPUT);
        MSG msg;
        while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage (&msg);
            DispatchMessageW (&msg);
        }
        const auto now = std::chrono::steady_clock::now ();
        if (device_changed) {
            device_changed = false;
            pending = true;
            pending_since = now;
        }
        // A device changed 300 ms ago, or the safety interval passed.
        if ((pending && now - pending_since >= std::chrono::milliseconds (300)) ||
            now - last >= safety) {
            comparePorts (known, false);
            pending = false;
            last = now;
        }
    }
    if (notify) UnregisterDeviceNotification (notify);
    if (hwnd) DestroyWindow (hwnd);
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

#endif // _WIN32
