/*
 * Smoke test of the built serial library (no serial hardware needed). MIT, like the library.
 *
 * Built twice by visual_studio/serial.sln: with the static library (serial_test_lib) and with the
 * DLL (serial_test_dll, SERIAL_USE_DLL). Each build runs it: it returns 0 when every check passes,
 * otherwise 1, which fails the build. It checks the exported class and functions and that the
 * library's exceptions can be caught by the program (across the DLL boundary too).
 */
#include <cstdio>
#include <string>
#include <vector>
#include "serial/serial.h"

#ifndef SMOKETEST_KIND
#define SMOKETEST_KIND "library"
#endif

static int failures = 0;

static void check(bool ok, const char* what) {
	if (!ok) {
		std::printf("  FAILED: %s\n", what);
		++failures;
	}
}

int main() {
	std::printf("serial (%s), %d-bit\n", SMOKETEST_KIND, static_cast<int>(sizeof(void*) * 8));

	// Port list (may be empty on a PC without serial ports).
	const std::vector<serial::PortInfo> ports = serial::list_ports();
	std::printf("  %d port(s) found\n", static_cast<int>(ports.size()));
	for (const serial::PortInfo& p : ports) std::printf("    %s - %s\n", p.port.c_str(), p.description.c_str());

	// A Serial object without a port: settings are kept, nothing is opened.
	serial::Serial s;
	check(!s.isOpen(), "a new Serial is closed");
	check(s.getPort().empty(), "no port set");
	s.setBaudrate(115200);
	check(s.getBaudrate() == 115200, "baud rate kept");
	s.setBytesize(serial::sevenbits);
	check(s.getBytesize() == serial::sevenbits, "byte size kept");
	s.setParity(serial::parity_even);
	check(s.getParity() == serial::parity_even, "parity kept");
	s.setStopbits(serial::stopbits_two);
	check(s.getStopbits() == serial::stopbits_two, "stop bits kept");
	s.setFlowcontrol(serial::flowcontrol_hardware);
	check(s.getFlowcontrol() == serial::flowcontrol_hardware, "flow control kept");
	serial::Timeout timeout = serial::Timeout::simpleTimeout(250);
	s.setTimeout(timeout);
	check(s.getTimeout().read_timeout_constant == 250, "timeout kept");

	// Errors are reported as the library's exceptions, and the program can catch them.
	bool caught = false;
	try {
		s.write(std::string("x"));
	}
	catch (const serial::PortNotOpenedException&) {
		caught = true;
	}
	catch (...) {
	}
	check(caught, "write() on a closed port throws PortNotOpenedException");

	caught = false;
	s.setPort("COM254"); // practically never present
	check(s.getPort() == L"COM254", "port name kept");
	try {
		s.open();
	}
	catch (const serial::IOException& e) {
		caught = true;
		std::printf("  expected error: %s\n", e.what());
	}
	catch (...) {
	}
	check(caught || s.isOpen(), "open() of a missing port throws IOException");
	if (s.isOpen()) s.close();

	if (failures == 0) std::printf("  all checks passed\n");
	else std::printf("  %d check(s) FAILED\n", failures);
	return failures == 0 ? 0 : 1;
}
