/* Isolated termios2 / BOTHER wrapper.
 *
 * glibc's <termios.h> and the kernel's <asm/termbits.h> both define
 * `struct termios`. Including them in the same translation unit is a
 * redefinition error on modern glibc. This file deliberately avoids
 * <termios.h> and only pulls the kernel header, so the BOTHER-based
 * custom-baud ioctl can be issued without polluting the main unix.cc
 * translation unit.
 *
 * The wrapper returns 0 on success and the errno value otherwise. A
 * build without <asm/termbits.h> or without BOTHER links a stub that
 * returns ENOTSUP; callers in unix.cc fall back to the legacy
 * TIOCGSERIAL / ASYNC_SPD_CUST path. */

#include <cstdint>
#include <cerrno>

#if defined(__linux__) && __has_include(<asm/termbits.h>)
#  include <asm/termbits.h>
#  include <sys/ioctl.h>
#  ifdef BOTHER
#    define SERIAL_TERMIOS2_IMPL 1
#  endif
#endif

namespace serial {
namespace detail {

int set_custom_baud_termios2 (int fd, std::uint32_t baudrate)
{
#ifdef SERIAL_TERMIOS2_IMPL
    struct termios2 tio2;
    if (ioctl (fd, TCGETS2, &tio2) == -1) {
        return errno ? errno : EIO;
    }
    tio2.c_cflag &= ~CBAUD;
    tio2.c_cflag |= BOTHER;
    tio2.c_ispeed = baudrate;
    tio2.c_ospeed = baudrate;
    if (ioctl (fd, TCSETS2, &tio2) == -1) {
        return errno ? errno : EIO;
    }
    return 0;
#else
    (void) fd;
    (void) baudrate;
    return ENOTSUP;
#endif
}

bool termios2_available ()
{
#ifdef SERIAL_TERMIOS2_IMPL
    return true;
#else
    return false;
#endif
}

} // namespace detail
} // namespace serial
