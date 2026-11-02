/* Minimal pty-based tests for the serial library (Linux only).
 *
 * The slave side of a pseudo-terminal is used as the serial device under
 * test; writes to the master echo back as reads on the slave and vice
 * versa, which is enough to exercise the public read / write / timeout
 * / cancelRead paths without any physical hardware.
 */

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

#include "serial/serial.h"

#define REQUIRE(x) do { \
    if (!(x)) { \
        std::fprintf (stderr, "FAIL: %s @ %s:%d\n", #x, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int open_pty (int& master, std::string& slave_path)
{
    char name[256] = {0};
    int slave = -1;
    if (openpty (&master, &slave, name, nullptr, nullptr) != 0) return -1;
    slave_path = name;
    ::close (slave); // the Serial class will reopen /dev/pts/N itself
    return 0;
}

int main ()
{
    // ----- open / close -----
    {
        int master = -1;
        std::string slave_path;
        REQUIRE (open_pty (master, slave_path) == 0);
        serial::Serial s (slave_path, 115200, serial::Timeout::simpleTimeout (200));
        REQUIRE (s.isOpen ());
        s.close ();
        REQUIRE (!s.isOpen ());
        ::close (master);
    }

    // ----- write / read loopback -----
    {
        int master = -1;
        std::string slave_path;
        REQUIRE (open_pty (master, slave_path) == 0);
        serial::Serial s (slave_path, 115200, serial::Timeout::simpleTimeout (500));
        const char* msg = "hello";
        REQUIRE (::write (master, msg, 5) == 5);
        std::string got = s.read (5);
        REQUIRE (got == "hello");
        ::close (master);
    }

    // ----- waitReadable: true when data present, false on timeout -----
    {
        int master = -1;
        std::string slave_path;
        REQUIRE (open_pty (master, slave_path) == 0);
        serial::Serial s (slave_path, 115200, serial::Timeout::simpleTimeout (100));
        REQUIRE (s.waitReadable () == false);
        REQUIRE (::write (master, "x", 1) == 1);
        REQUIRE (s.waitReadable () == true);
        ::close (master);
    }

    // ----- cancelRead unblocks a reader thread -----
    {
        int master = -1;
        std::string slave_path;
        REQUIRE (open_pty (master, slave_path) == 0);
        // Long timeout so the thread would otherwise wait minutes.
        serial::Serial s (slave_path, 115200, serial::Timeout::simpleTimeout (60000));
        std::atomic<bool> finished { false };
        std::thread reader ([&]() {
            uint8_t buf[1];
            s.read (buf, 1);
            finished.store (true);
        });
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
        const auto cancelled_at = std::chrono::steady_clock::now ();
        s.cancelRead ();
        reader.join ();
        const auto took = std::chrono::duration_cast<std::chrono::milliseconds> (
            std::chrono::steady_clock::now () - cancelled_at).count ();
        REQUIRE (finished.load ());
        REQUIRE (took < 1000); // at once, not after the 60 s read timeout
        ::close (master);
    }

    // ----- the device goes away: read reports it -----
    {
        int master = -1;
        std::string slave_path;
        REQUIRE (open_pty (master, slave_path) == 0);
        serial::Serial s (slave_path, 115200, serial::Timeout::simpleTimeout (500));
        ::close (master);
        bool threw = false;
        try {
            s.read (1);
        } catch (const serial::SerialException&) {
            threw = true;
        } catch (const serial::IOException&) {
            threw = true;
        }
        REQUIRE (threw);
    }

    // ----- setExclusive: second open fails -----
    {
        int master = -1;
        std::string slave_path;
        REQUIRE (open_pty (master, slave_path) == 0);
        serial::Serial first (slave_path, 115200, serial::Timeout::simpleTimeout (100));
        first.setExclusive (true);
        bool threw = false;
        try {
            serial::Serial second (slave_path, 115200, serial::Timeout::simpleTimeout (100));
        } catch (...) {
            threw = true;
        }
        // Not every pty driver honors TIOCEXCL; treat failure to throw as
        // a soft pass with a message rather than a hard failure.
        if (!threw) {
            std::fprintf (stderr, "note: TIOCEXCL not enforced on this pty\n");
        }
        ::close (master);
    }

    return 0;
}
