// Based on code at http://msdn.microsoft.com/en-us/library/windows/desktop/dd757929%28v=vs.85%29.aspx

#pragma once

#include "stdafx.h"

HRESULT WriteWaveFile(
					  IMFSourceReader *pReader,   // Pointer to the source reader.
					  HANDLE hFile,               // Handle to the output file.
					  LONG msecAudioData          // Maximum amount of audio data to write, in msec.
					  );
