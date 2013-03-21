#include "stdafx.h"
#include "mfRoutines.h"
#include "mfWave.h"

HRESULT enumerateTypesForStreams(IMFSourceReader *pReader) {
	HRESULT hr = S_OK;
	DWORD dwStreamIndex = 0;
	//WCHAR guidString[80];
	DWORD dwMediaTypeIndex = 0;

	while (SUCCEEDED(hr)) {
		IMFMediaType *pType = NULL;
		hr = pReader->GetNativeMediaType(dwStreamIndex, dwMediaTypeIndex, &pType);
		if (hr == MF_E_INVALIDSTREAMNUMBER) {
			hr = S_OK;
			break;
		} else if (SUCCEEDED(hr)) {
			wprintf(L"  Stream %d:\n", dwStreamIndex);
			hr = enumerateTypesForStream(pReader, dwStreamIndex);
			if (SUCCEEDED(hr)) {
				// Do something
			}
			pType->Release();
		}
		++dwStreamIndex;
	}
	return hr;
}

HRESULT enumerateTypesForStream(IMFSourceReader *pReader, DWORD dwStreamIndex) {
	HRESULT hr = S_OK;
	DWORD dwMediaTypeIndex = 0;
	WCHAR guidString[80];
	GUID type;

	while (SUCCEEDED(hr)) {
		IMFMediaType *pType = NULL;
		hr = pReader->GetNativeMediaType(dwStreamIndex, dwMediaTypeIndex, &pType);
		if (hr == MF_E_NO_MORE_TYPES) {
			hr = S_OK;
			break;
		} else if (SUCCEEDED(hr)) {
			hr = pType->GetGUID(MF_MT_SUBTYPE, &type);
			if (SUCCEEDED(hr)) {
				hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &type);
				getFriendlyGuidString(type, guidString, 79);
				wprintf(L"    Type: %s", guidString);
				if (SUCCEEDED(hr)) {
					hr = pType->GetGUID(MF_MT_SUBTYPE, &type);
					getFriendlyGuidString(type, guidString, 79);
					wprintf(L"/%s\n", guidString);
				} else {
					printf("\n");
				}
			}
			pType->Release();
		}
		++dwMediaTypeIndex;
	}
	return hr;
}

const LONG MAX_AUDIO_DURATION_MSEC = 10000; // 10 seconds

void printMfAudioInfo(void) {
	printf("MF Audio Info\n");

	IMFMediaSource *pSource = NULL;
	UINT32 count = 0;
	IMFAttributes *pConfig = NULL;
	IMFActivate **ppDevices = NULL;

	// Create an attribute store to hold the search criteria.
	HRESULT hr = MFCreateAttributes(&pConfig, 1);
	if (FAILED(hr)) {
		printf("Error creating attribute store\n");
		printErrorDescription(hr);
		return;
	}

	// Request audio capture devices.
	hr = pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
	if (FAILED(hr)) {
		printf("Error requesting audio devices\n");
		printErrorDescription(hr);
		return;
	}

	// Enumerate the devices,
	hr = MFEnumDeviceSources(pConfig, &ppDevices, &count);
	if (FAILED(hr)) {
		printf("Error enumerating audio devices\n");
		printErrorDescription(hr);
		return;
	}
	printf("Number of devices: %d\n", count);

	// Loop over devices
	WCHAR *szFriendlyName = NULL;
	for (UINT32 iDevice = 0; iDevice < count; iDevice++) {
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
		CoTaskMemFree(szFriendlyName);
		szFriendlyName = NULL;

		// Create a media source
		hr = ppDevices[iDevice]->ActivateObject(IID_PPV_ARGS(&pSource));
		if (FAILED(hr)) {
			printf("Error creating media source for device %d\n", iDevice);
			printErrorDescription(hr);
			continue;
		}

		// Create a source reader from the media source
		IMFSourceReader *pReader;
		hr = MFCreateSourceReaderFromMediaSource(pSource, pConfig, &pReader);
		if (FAILED(hr)) {
			printf("Error creating media source for device %d\n", iDevice);
			printErrorDescription(hr);
			goto CLEANUP;
		}

		// Print the types for this reader
		enumerateTypesForStreams(pReader);

		// Create the output file
		HANDLE hFile = INVALID_HANDLE_VALUE;
		char fileName[256];
		sprintf_s(fileName, "WFAudioTest-Device%02d.wav", iDevice);
		hFile = CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			hr = HRESULT_FROM_WIN32(GetLastError());
			printf("Cannot create output file: %s\n", fileName, hr);
			goto CLEANUP;
		}

		// Try to record
		printf("  Trying to record for %d sec...\n",
			MAX_AUDIO_DURATION_MSEC / 1000);

		// Write the file
		hr = WriteWaveFile(pReader, hFile, MAX_AUDIO_DURATION_MSEC);
		if (FAILED(hr)) {
			printf("Error writing file for device %d\n", iDevice);
			printErrorDescription(hr);
			goto CLEANUP;
		}
		printf("    Output is %s\n", fileName);

CLEANUP:
		if (hFile != INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
		}
		// This should also release the media source
		SafeRelease(&pReader);
	}

	// Release the sources
	for (DWORD i = 0; i < count; i++)  {
		ppDevices[i]->Release();
	}
	CoTaskMemFree(ppDevices);
}
