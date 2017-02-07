//////////////////////////////////////////////////////////////////////////
// mfRoutines.h: Media Foundation routines
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

#include "capture.h"

void printMfAudioInfo(void);
void printErrorDescription(HRESULT hr);
void initialize();
void shutdown();
