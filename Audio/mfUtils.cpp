#include "stdafx.h"
#include "mfUtils.h"

TCHAR szDebugString[PRINT_STRING_SIZE];

WCHAR *getFriendlyGuidString(GUID guid) {
	// Audio
	if(guid == MFAudioFormat_Base) {
		return L"MFAudioFormat_Base";
	} else if(guid == MFAudioFormat_PCM) {
		return L"MFAudioFormat_PCM";
	} else if(guid == MFAudioFormat_Float) {
		return L"MFAudioFormat_Float";
	} else if(guid == MFAudioFormat_DTS) {
		return L"MFAudioFormat_DTS";
	} else if(guid == MFAudioFormat_Dolby_AC3_SPDIF) {
		return L"MFAudioFormat_Dolby_AC3_SPDIF";
	} else if(guid == MFAudioFormat_DRM) {
		return L"MFAudioFormat_DRM";
	} else if(guid == MFAudioFormat_WMAudioV8) {
		return L"MFAudioFormat_WMAudioV8";
	} else if(guid == MFAudioFormat_WMAudioV9) {
		return L"MFAudioFormat_WMAudioV9";
	} else if(guid == MFAudioFormat_WMAudio_Lossless) {
		return L"MFAudioFormat_WMAudio_Lossless";
	} else if(guid == MFAudioFormat_WMASPDIF) {
		return L"MFAudioFormat_WMASPDIF";
	} else if(guid == MFAudioFormat_MSP1) {
		return L"MFAudioFormat_MSP1";
	} else if(guid == MFAudioFormat_MP3) {
		return L"MFAudioFormat_MP3";
	} else if(guid == MFAudioFormat_MPEG) {
		return L"MFAudioFormat_MPEG";
	} else if(guid == MFAudioFormat_AAC) {
		return L"MFAudioFormat_AAC";
	} else if(guid == MFAudioFormat_ADTS) {
		return L"MFAudioFormat_ADTS";
		// Major
	} else if(guid == MFMediaType_Default) {
		return L"MFMediaType_Default";
	} else if(guid == MFMediaType_Audio) {
		return L"MFMediaType_Audio";
	} else if(guid == MFMediaType_Video) {
		return L"MFMediaType_Video";
	} else {
		return L"Unrecognized";
	}
}

// Returns a string representation of a GUID into the given buffer
// with at mose nChars characters
void getFriendlyGuidString(GUID guid, WCHAR *szString, int nChars) {
	WCHAR *szFriendlyString = getFriendlyGuidString(guid);
	if(wcscmp(szFriendlyString, L"Unrecognized")) {
		swprintf_s(szString, nChars, szFriendlyString);
	} else {
		WCHAR *guidString = getGuidString(guid);
		swprintf_s(szString, nChars, guidString);
		::CoTaskMemFree(guidString);
	}
}

// Returns a string representation of a GUID
// Needs to be freed with ::CoTaskMemFree(guidString);
WCHAR *getGuidString(GUID guid) {
	WCHAR *guidString;
	StringFromCLSID(guid, &guidString);
	return guidString;
}

TCHAR *getErrorDescription(HRESULT hr) { 
	if(FACILITY_WINDOWS == HRESULT_FACILITY(hr)) 
		hr = HRESULT_CODE(hr); 
	TCHAR* szErrMsg; 

	if(FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, 
		NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPTSTR)&szErrMsg, 0, NULL) != 0) { 
			return szErrMsg;
	} else {
		return NULL;
	}
}

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
		_tprintf( TEXT("[No further information for HRESULT 0x%#X]\n"), hr); 
	}
}

void initializeMfCom() {
	// Initialize the COM library
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// Initialize Media Foundation
	if (SUCCEEDED(hr))  {
		hr = MFStartup(MF_VERSION);
	}
}

void shutdownMfCom() {
	MFShutdown();
	CoUninitialize();
}

void ShowMessage(HRESULT hrErr, const TCHAR *format, ...) {
	const size_t MESSAGE_LEN = 1024;
	TCHAR message[MESSAGE_LEN];

	va_list vargs;
	va_start(vargs,format);
	_vstprintf_s(szDebugString, format, vargs);
	va_end(vargs);

	// Get the description and add the error part
	TCHAR *szErrMsg = getErrorDescription(hrErr);

	HRESULT hr = StringCchPrintf (message, MESSAGE_LEN,
		_T("%s (HRESULT = 0x%X)\n%s\n"),
		szDebugString, hrErr,
		szErrMsg != NULL ? szErrMsg : TEXT("No Further Information"));
	if (SUCCEEDED(hr)) {
		_tprintf(message);
#ifdef _DEBUG
		OutputDebugString(szDebugString);
#endif
	}

	// Free the description
	if(szErrMsg) {
		LocalFree(szErrMsg);
	}
}

int debugMsg(const TCHAR *format, ...)
{
	va_list vargs;

	va_start(vargs,format);
	_vstprintf_s(szDebugString,format,vargs);
	va_end(vargs);

	if(szDebugString[0] == '\0') return 0;

	// Display the string.
	OutputDebugString(szDebugString);
	return 0;
}
