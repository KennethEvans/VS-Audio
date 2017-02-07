#pragma once

#include "stdafx.h"

HRESULT WriteWmaFile(
					  IMFSourceReader *pReader,   // Pointer to the source reader.
					  WCHAR *szFileName,          // Name of the output file.
					  LONG msecAudioData          // Maximum amount of audio data to write, in msec.
					  );
