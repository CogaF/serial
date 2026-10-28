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

/* Undefine the handful of lowercase / common-name macros that windows.h
 * leaks into the global namespace. These names appear in the serial API
 * (and in common downstream code) and must behave as plain C++
 * identifiers. */
#ifdef small
#  undef small
#endif
#ifdef ERROR
#  undef ERROR
#endif
#ifdef interface
#  undef interface
#endif
#ifdef near
#  undef near
#endif
#ifdef far
#  undef far
#endif

#endif /* _WIN32 */

#endif /* SERIAL_IMPL_WIN_COMPAT_H */
