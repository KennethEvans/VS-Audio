// Audio.cpp : Defines the entry point for the console application.

#include "stdafx.h"
#include "mmRoutines.h"
#include "mfRoutines.h"
#include "mfWave.h"
#include "mfWma.h"

const LONG MAX_AUDIO_DURATION_MSEC = 10000; // 10 seconds

void printMfAudioInfo(BOOL useWma) {
	printf("MF Audio Info\n");

	IMFMediaSource *pSource = NULL;
	UINT32 count = 0;
	IMFAttributes *pAttributes = NULL;
	IMFActivate **ppDevices = NULL;

	// Create an attribute store to hold the search criteria.
	HRESULT hr = MFCreateAttributes(&pAttributes, 1);
	if (FAILED(hr)) {
		printf("Error creating attribute store\n");
		printErrorDescription(hr);
		return;
	}

	// Request audio capture devices.
	hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
	if (FAILED(hr)) {
		printf("Error requesting audio devices\n");
		printErrorDescription(hr);
		return;
	}

	// Enumerate the devices,
	hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
	if (FAILED(hr)) {
		printf("Error enumerating audio devices\n");
		printErrorDescription(hr);
		return;
	}
	printf("Number of devices: %d\n", count);

	// Loop over devices
	WCHAR *szFriendlyName = NULL;
	for (UINT32 iDevice = 0; iDevice < count; iDevice++) {
#if 0
		// DEBUG
		if(iDevice != 6) {
			continue;
		}
#endif
		// Get the friendly name of the device
		hr = S_OK;
		UINT32 cchName;
		hr = ppDevices[iDevice]->
			GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
			&szFriendlyName, &cchName);
		if (FAILED(hr)) {
			printf("Error getting information for audio device %d\n", iDevice);
			printErrorDescription(hr);
			continue;
		} else {
			wprintf(L"%d %s\n", iDevice, szFriendlyName);
		}

		// Create a media source
		hr = ppDevices[iDevice]->ActivateObject(IID_PPV_ARGS(&pSource));
		if (FAILED(hr)) {
			printf("Error creating media source for device %d\n", iDevice);
			printErrorDescription(hr);
			continue;
		}

		// Create a source reader from the media source
		IMFSourceReader *pReader;
		hr = MFCreateSourceReaderFromMediaSource(pSource, pAttributes, &pReader);
		if (FAILED(hr)) {
			printf("Error creating media source for device %d\n", iDevice);
			printErrorDescription(hr);
			goto CLEANUP;
		}

		// Print the types for this reader
		enumerateTypesForStreams(pReader);

		// Try to record
		printf("  Trying to record for %d sec...\n",
			MAX_AUDIO_DURATION_MSEC / 1000);

		// Write the file
		if(useWma) {
			WCHAR szFileName[256];
			swprintf_s(szFileName, L"MFWMA-AudioTest-%s.wma", szFriendlyName);
			hr = WriteWmaFile(pReader, szFileName, MAX_AUDIO_DURATION_MSEC);
			if (FAILED(hr)) {
				wprintf(L"Error writing WMA file for device %d for %s\n", iDevice,
					szFileName);
				printErrorDescription(hr);
				goto CLEANUP;
			}
			wprintf(L"    Output is %s\n", szFileName);
		} else {
			WCHAR szFileName[256];
			swprintf_s(szFileName, L"MFWAV-AudioTest-%s.wav", szFriendlyName);
			hr = WriteWaveFile(pReader, szFileName, MAX_AUDIO_DURATION_MSEC);
			if (FAILED(hr)) {
				wprintf(L"Error writing WAV file for device %d\n", iDevice);
				printErrorDescription(hr);
				goto CLEANUP;
			}
			wprintf(L"    Output is %s\n", szFileName);
		}

CLEANUP:
		// This should also release the media source
		SafeRelease(&pReader);
	}

	CoTaskMemFree(szFriendlyName);
	szFriendlyName = NULL;
	// Release the sources
	for (DWORD i = 0; i < count; i++)  {
		ppDevices[i]->Release();
	}
	CoTaskMemFree(ppDevices);
}

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc > 1) {
		if(!_stricmp(argv[1], _T("-mm"))) {
			printAudioInfo();
		} else if(!_stricmp(argv[1], _T("-mf"))) {
			initializeMfCom();
			printMfAudioInfo(TRUE);
			shutdownMfCom();
		} else if(!_stricmp(argv[1], _T("-mfwma"))) {
			initializeMfCom();
			printMfAudioInfo(TRUE);
			shutdownMfCom();
		} else if(!_stricmp(argv[1], _T("-mfwav"))) {
			initializeMfCom();
			printMfAudioInfo(FALSE);
			shutdownMfCom();
		} else {
			printf("Invalid option %s\n", argv[1]);
		}
	} else {
		initializeMfCom();
		printMfAudioInfo(TRUE);
		shutdownMfCom();
	}
	printf("All Done\n");
	return 0;
}

