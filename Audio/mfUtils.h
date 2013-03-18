//////////////////////////////////////////////////////////////////////////
// mfUtils.h: Media Foundation utilities
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"

#include <new>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Wmcodecdsp.h>
#include <assert.h>
#include <Dbt.h>
#include <shlwapi.h>
#include <Mferror.h>

WCHAR *getFriendlyGuidString(GUID guid);
void getFriendlyGuidString(GUID guid, WCHAR *szString, int nChars);
OLECHAR *getGuidString(GUID guid);
void printErrorDescription(HRESULT hr);
void initializeMfCom();
void shutdownMfCom();

// This has to be included in each file that uses it and so is in the header
template <class T> void SafeRelease(T **ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

