/*!
 * \file serial/impl/win_compat.h
 *
 * Centralized inclusion of <windows.h> for the serial library. Pulling
 * windows.h in with its default settings drags in a long list of macros
 * (small, min, max, ERROR, interface, near, far, ...) that collide with
 * perfectly reasonable C++ identifiers used elsewhere in this codebase
 * and in downstream consumers.
 *
 * This header is the single point where windows.h is included. Every
 * other translation unit that needs windows.h must include this header
 * instead of <windows.h> directly.
 */

#ifndef SERIAL_IMPL_WIN_COMPAT_H
#define SERIAL_IMPL_WIN_COMPAT_H

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#  define NOMINMAX
#endif

#include <windows.h>

/* Undefine the one lowercase macro that windows.h leaks and that is a
 * valid C++ identifier used in this codebase (and in downstream
 * consumers). Other macros like near/far/interface/ERROR are kept in
 * place: they are used as types/keywords inside the Windows SDK itself
 * (commctrl.h, COM headers, ...) and undefining them breaks those
 * headers when they are transitively included. */
#ifdef small
#  undef small
#endif

#endif /* _WIN32 */

#endif /* SERIAL_IMPL_WIN_COMPAT_H */
