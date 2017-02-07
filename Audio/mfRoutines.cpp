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
