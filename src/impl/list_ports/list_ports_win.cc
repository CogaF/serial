#if defined(_WIN32)

/*
 * Copyright (c) 2014 Craig Lilley <cralilley@gmail.com>
 * This software is made available under the terms of the MIT licence.
 * A copy of the licence can be obtained from:
 * http://opensource.org/licenses/MIT
 */

#include "serial/serial.h"
#include "serial/impl/win_compat.h"
#include <tchar.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <cstring>
#include <cctype>

using serial::PortInfo;
using std::vector;
using std::string;

static const DWORD port_name_max_length = 256;
static const DWORD friendly_name_max_length = 256;
static const DWORD hardware_id_max_length = 256;

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring &wstr)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo( size_needed, 0 );
	WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

vector<PortInfo>
serial::list_ports()
{
	vector<PortInfo> devices_found;

	HDEVINFO device_info_set = SetupDiGetClassDevs(
		(const GUID *) &GUID_DEVCLASS_PORTS,
		NULL,
		NULL,
		DIGCF_PRESENT);

	unsigned int device_info_set_index = 0;
	SP_DEVINFO_DATA device_info_data;

	device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

	while(SetupDiEnumDeviceInfo(device_info_set, device_info_set_index, &device_info_data))
	{
		device_info_set_index++;

		// Get port name

		HKEY hkey = SetupDiOpenDevRegKey(
			device_info_set,
			&device_info_data,
			DICS_FLAG_GLOBAL,
			0,
			DIREG_DEV,
			KEY_READ);

		TCHAR port_name[port_name_max_length];
		DWORD port_name_length = port_name_max_length;

		LONG return_code = RegQueryValueEx(
					hkey,
					_T("PortName"),
					NULL,
					NULL,
					(LPBYTE)port_name,
					&port_name_length);

		RegCloseKey(hkey);

		if(return_code != EXIT_SUCCESS)
			continue;

		if(port_name_length > 0 && port_name_length <= port_name_max_length)
			port_name[port_name_length-1] = '\0';
		else
			port_name[0] = '\0';

		// Ignore parallel ports

		if(_tcsstr(port_name, _T("LPT")) != NULL)
			continue;

		// Get port friendly name

		TCHAR friendly_name[friendly_name_max_length];
		DWORD friendly_name_actual_length = 0;

		BOOL got_friendly_name = SetupDiGetDeviceRegistryProperty(
					device_info_set,
					&device_info_data,
					SPDRP_FRIENDLYNAME,
					NULL,
					(PBYTE)friendly_name,
					friendly_name_max_length,
					&friendly_name_actual_length);

		if(got_friendly_name == TRUE && friendly_name_actual_length > 0)
			friendly_name[friendly_name_actual_length-1] = '\0';
		else
			friendly_name[0] = '\0';

		// Get hardware ID

		TCHAR hardware_id[hardware_id_max_length];
		DWORD hardware_id_actual_length = 0;

		BOOL got_hardware_id = SetupDiGetDeviceRegistryProperty(
					device_info_set,
					&device_info_data,
					SPDRP_HARDWAREID,
					NULL,
					(PBYTE)hardware_id,
					hardware_id_max_length,
					&hardware_id_actual_length);

		if(got_hardware_id == TRUE && hardware_id_actual_length > 0)
			hardware_id[hardware_id_actual_length-1] = '\0';
		else
			hardware_id[0] = '\0';

		#ifdef UNICODE
			std::string portName = utf8_encode(port_name);
			std::string friendlyName = utf8_encode(friendly_name);
			std::string hardwareId = utf8_encode(hardware_id);
		#else
			std::string portName = port_name;
			std::string friendlyName = friendly_name;
			std::string hardwareId = hardware_id;
		#endif

		PortInfo port_entry;
		port_entry.port = portName;
		port_entry.description = friendlyName;
		port_entry.hardware_id = hardwareId;

		// Parse VID / PID out of the hardware id (format
		// "USB\\VID_xxxx&PID_yyyy...").
		{
			size_t vp = hardwareId.find("VID_");
			if (vp != std::string::npos && hardwareId.size() >= vp + 8) {
				port_entry.vid = hardwareId.substr(vp + 4, 4);
				for (auto& c : port_entry.vid) c = (char)std::tolower((unsigned char)c);
			}
			size_t pp = hardwareId.find("PID_");
			if (pp != std::string::npos && hardwareId.size() >= pp + 8) {
				port_entry.pid = hardwareId.substr(pp + 4, 4);
				for (auto& c : port_entry.pid) c = (char)std::tolower((unsigned char)c);
			}
		}

		// Serial number sits at the tail of the device instance id after
		// the last backslash: "USB\\VID_xxxx&PID_yyyy\\<serial>".
		{
			TCHAR instance_id[256];
			DWORD inst_len = 0;
			if (SetupDiGetDeviceInstanceId(device_info_set, &device_info_data,
				instance_id, 256, &inst_len))
			{
#ifdef UNICODE
				std::string iid = utf8_encode(instance_id);
#else
				std::string iid = instance_id;
#endif
				size_t last = iid.find_last_of('\\');
				if (last != std::string::npos && last + 1 < iid.size()) {
					port_entry.serial_number = iid.substr(last + 1);
				}
			}
		}

		// Manufacturer, location and driver service name.
		auto fetch_prop = [&](DWORD prop) -> std::string {
			TCHAR buf[256];
			DWORD len = 0;
			if (!SetupDiGetDeviceRegistryProperty(device_info_set, &device_info_data,
				prop, NULL, (PBYTE)buf, sizeof(buf), &len))
				return std::string();
			if (len == 0) return std::string();
#ifdef UNICODE
			return utf8_encode(buf);
#else
			return std::string(buf);
#endif
		};
		port_entry.manufacturer = fetch_prop(SPDRP_MFG);
		port_entry.location     = fetch_prop(SPDRP_LOCATION_INFORMATION);
		port_entry.driver       = fetch_prop(SPDRP_SERVICE);
		port_entry.product      = fetch_prop(SPDRP_DEVICEDESC);

		devices_found.push_back(port_entry);
	}

	SetupDiDestroyDeviceInfoList(device_info_set);

	return devices_found;
}

#endif // #if defined(_WIN32)
