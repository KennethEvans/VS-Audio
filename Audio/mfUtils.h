//////////////////////////////////////////////////////////////////////////
// mfUtils.h: Media Foundation utilities
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"

#define PRINT_STRING_SIZE 1024
// Global string for print routines.  Must be defined somewhere.
extern TCHAR szDebugString[PRINT_STRING_SIZE];

WCHAR *getFriendlyGuidString(GUID guid);
void getFriendlyGuidString(GUID guid, WCHAR *szString, int nChars);
OLECHAR *getGuidString(GUID guid);
TCHAR *getErrorDescription(HRESULT hr);
void printErrorDescription(HRESULT hr);
void initializeMfCom();
void shutdownMfCom();
void ShowMessage(HRESULT hrErr, const TCHAR *format, ...);
int debugMsg(const TCHAR *format, ...);

// This has to be included in each file that uses it and so is in the header
template <class T> void SafeRelease(T **ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

