#include "mfRoutines.h"

void printErrorDescription(HRESULT hr) { 
	if(FACILITY_WINDOWS == HRESULT_FACILITY(hr)) 
		hr = HRESULT_CODE(hr); 
	TCHAR* szErrMsg; 

	if(FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, 
		NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPTSTR)&szErrMsg, 0, NULL) != 0) { 
			_tprintf(TEXT("%s"), szErrMsg); 
			LocalFree(szErrMsg); 
	} else {
		_tprintf( TEXT("[Could not find a description for error # %#x.]\n"), hr); 
	}
}

void initialize() {
	// Initialize the COM library
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// Initialize Media Foundation
	if (SUCCEEDED(hr))  {
		hr = MFStartup(MF_VERSION);
	}
}

void shutdown() {
    MFShutdown();
    CoUninitialize();
}

HRESULT UpdateDeviceList(DeviceList &g_devices) {
	HRESULT hr = S_OK;
	WCHAR *szFriendlyName = NULL;
	g_devices.Clear();
	hr = g_devices.EnumerateDevices(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
//	hr = g_devices.EnumerateDevices(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	return hr;
}

void printMfAudioInfo(void) {
	printf("MF Audio Info\n");
	DeviceList g_devices;
	HRESULT hr = UpdateDeviceList(g_devices);
	if (FAILED(hr)) {
		printf("Cannot enumerate devices\n");
		printErrorDescription(hr);
		return;
	}
	printf("Number of devices: %d\n", g_devices.Count());

	WCHAR *szFriendlyName = NULL;
	for (UINT32 iDevice = 0; iDevice < g_devices.Count(); iDevice++) {
		// Get the friendly name of the device.
		hr = g_devices.GetDeviceName(iDevice, &szFriendlyName);
		if (FAILED(hr)) {
			printf("Error getting information for audio device &d", iDevice);
			printErrorDescription(hr);
			continue;
		} else {
			wprintf(L"%s\n", szFriendlyName);
		}
		CoTaskMemFree(szFriendlyName);
		szFriendlyName = NULL;
	}
}
