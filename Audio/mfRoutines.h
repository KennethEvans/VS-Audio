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

#include "mfUtils.h"
#include "capture.h"

HRESULT enumerateTypesForStreams(IMFSourceReader *pReader);
HRESULT enumerateTypesForStream(IMFSourceReader *pReader, DWORD dwStreamIndex);
HRESULT FixUpChunkSizes(
						HANDLE hFile,           // Output file.
						DWORD cbHeader,         // Size of the 'fmt ' chuck.
						DWORD cbAudioData       // Size of the 'data' chunk.
						);
HRESULT WriteWaveData(
					  HANDLE hFile,               // Output file.
					  IMFSourceReader *pReader,   // Source reader.
					  DWORD cbMaxAudioData,       // Maximum amount of audio data (bytes).
					  DWORD *pcbDataWritten       // Receives the amount of data written.
					  );
DWORD CalculateMaxAudioDataSize(
								IMFMediaType *pAudioType,    // The PCM audio format.
								DWORD cbHeader,              // The size of the WAVE file header.
								DWORD msecAudioData          // Maximum duration, in milliseconds.
								);
HRESULT WriteToFile(HANDLE hFile, void* p, DWORD cb);
HRESULT WriteWaveHeader(
						HANDLE hFile,               // Output file.
						IMFMediaType *pMediaType,   // PCM audio format.
						DWORD *pcbWritten           // Receives the size of the header.
						);
HRESULT ConfigureAudioStream(
							 IMFSourceReader *pReader,   // Pointer to the source reader.
							 IMFMediaType **ppPCMAudio   // Receives the audio format.
							 );
HRESULT WriteWaveFile(
					  IMFSourceReader *pReader,   // Pointer to the source reader.
					  HANDLE hFile,               // Handle to the output file.
					  LONG msecAudioData          // Maximum amount of audio data to write, in msec.
					  );

void printMfAudioInfo(void);
