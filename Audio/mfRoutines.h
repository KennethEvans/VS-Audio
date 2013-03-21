//////////////////////////////////////////////////////////////////////////
// mfRoutines.h: Media Foundation routines
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"
#include "mfUtils.h"

HRESULT enumerateTypesForStreams(IMFSourceReader *pReader);
HRESULT enumerateTypesForStream(IMFSourceReader *pReader, DWORD dwStreamIndex);
void printMfAudioInfo(void);
