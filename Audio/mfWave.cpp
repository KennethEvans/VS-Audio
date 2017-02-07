#include "stdafx.h"
#include "mfWave.h"
#include "mfRoutines.h"

// Selects an audio stream from the source file, and configures the
// stream to read MFAudioFormat_Float audio
HRESULT ConfigureWaveReader(
							IMFSourceReader *pReader,   // Pointer to the source reader.
							IMFMediaType **ppPCMAudio   // Receives the audio format.
							)
{
	IMFMediaType *pReaderType = NULL;
	IMFMediaType *pSubtype = NULL;

	// Select the first audio stream, and deselect all other streams.
	HRESULT hr = pReader->SetStreamSelection(
		(DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	if (SUCCEEDED(hr)) {
		hr = pReader->SetStreamSelection(
			(DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
	} else {
		printf("ConfigureWaveReader: Error in SetStreamSelection\n");
		printErrorDescription(hr);
		return hr;
	}

	// Create a media subtype that specifies float audio.
	hr = MFCreateMediaType(&pSubtype);
	if (SUCCEEDED(hr))  {
		hr = pSubtype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	} else {
		printf("ConfigureWaveReader: Error in SetGUID MFMediaType_Audio\n");
		printErrorDescription(hr);
		return hr;
	}
	if (SUCCEEDED(hr))  {
		//hr = pSubtype->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		hr = pSubtype->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
	} else {
		printf("ConfigureWaveReader: Error in SetGUID MFAudioFormat_Float\n");
		printErrorDescription(hr);
		return hr;
	}

	// Set this type on the source reader. The source reader will
	// load the necessary decoder.
	if (SUCCEEDED(hr)) {
		hr = pReader->SetCurrentMediaType(
			(DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			NULL, pSubtype);
	} else {
		printf("ConfigureWaveReader: Error in SetCurrentMediaType\n");
		printErrorDescription(hr);
		return hr;
	}

	// Get the complete uncompressed format.
	if (SUCCEEDED(hr)) {
		hr = pReader->GetCurrentMediaType(
			(DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			&pReaderType);
	} else {
		printf("ConfigureWaveReader: Error in GetCurrentMediaType\n");
		if(hr == MF_E_TOPO_CODEC_NOT_FOUND) {
			// Codec not found
			printf("MF_E_TOPO_CODEC_NOT_FOUND 0xC00D5212\n"
				"No suitable transform was found to encode or decode the content\n");
		} else {
			printErrorDescription(hr);
		}
		return hr;
	}

	// Ensure the stream is selected.
	if (SUCCEEDED(hr)) {
		hr = pReader->SetStreamSelection(
			(DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			TRUE);
	} else {
		printf("ConfigureWaveReader: Error in SetStreamSelection\n");
		printErrorDescription(hr);
		return hr;
	}

	// Return the format to the caller.
	if (SUCCEEDED(hr)) {
		*ppPCMAudio = pReaderType;
		(*ppPCMAudio)->AddRef();
	} else {
		printf("ConfigureWaveReader: Error in AddRef\n");
		printErrorDescription(hr);
		return hr;
	}

	SafeRelease(&pReaderType);
	SafeRelease(&pSubtype);
	return hr;
}

// Writes a block of data to a file
// hFile: Handle to the file.
// buf: Pointer to the buffer to write.
// count: Size of the buffer, in bytes.
HRESULT WriteToFile(HANDLE hFile, void* buf, DWORD count) {
	DWORD countWritten = 0;
	HRESULT hr = S_OK;
	BOOL bResult = WriteFile(hFile, buf, count, &countWritten, NULL);
	if (!bResult) {
		hr = HRESULT_FROM_WIN32(GetLastError());
	}
	return hr;
}

// Writes the file-size information into the WAVE file header.
// WAVE files use the RIFF file format. Each RIFF chunk has a data
// size, and the RIFF header has a total file size.
HRESULT FixUpChunkSizes(
						HANDLE hFile,           // Output file.
						DWORD cbHeader,         // Size of the 'fmt ' chuck.
						DWORD cbAudioData       // Size of the 'data' chunk.
						)
{
	HRESULT hr = S_OK;
	LARGE_INTEGER ll;
	ll.QuadPart = cbHeader - sizeof(DWORD);
	if (0 == SetFilePointerEx(hFile, ll, NULL, FILE_BEGIN)) {
		hr = HRESULT_FROM_WIN32(GetLastError());
	}

	// Write the data size.
	if (SUCCEEDED(hr)) {
		hr = WriteToFile(hFile, &cbAudioData, sizeof(cbAudioData));
	} else {
		printf("Error in WriteToFile: cbAudioData\n");
		printErrorDescription(hr);
		return hr;
	}

	if (SUCCEEDED(hr)) {
		// Write the file size.
		ll.QuadPart = sizeof(FOURCC);
		if (0 == SetFilePointerEx(hFile, ll, NULL, FILE_BEGIN)) {
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (SUCCEEDED(hr)) {
		DWORD cbRiffFileSize = cbHeader + cbAudioData - 8;
		// NOTE: The "size" field in the RIFF header does not include
		// the first 8 bytes of the file. (That is, the size of the
		// data that appears after the size field.)
		hr = WriteToFile(hFile, &cbRiffFileSize, sizeof(cbRiffFileSize));
	} else {
		printf("Error in WriteToFile: cbRiffFileSize\n");
		printErrorDescription(hr);
		return hr;
	}


	return hr;
}

// Decodes audio data from the source file and writes it to
// the WAVE file.
HRESULT WriteWaveData(
					  HANDLE hFile,               // Output file.
					  IMFSourceReader *pReader,   // Source reader.
					  DWORD cbMaxAudioData,       // Maximum amount of audio data (bytes).
					  DWORD *pcbDataWritten       // Receives the amount of data written.
					  )
{
	HRESULT hr = S_OK;
	DWORD cbAudioData = 0;
	DWORD cbBuffer = 0;
	BYTE *pAudioData = NULL;
	IMFSample *pSample = NULL;
	IMFMediaBuffer *pBuffer = NULL;

	// Get audio samples from the source reader.
	while (true) {
		DWORD dwFlags = 0;

		// Read the next sample.
		hr = pReader->ReadSample(
			(DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			0, NULL, &dwFlags, NULL, &pSample );

		if (FAILED(hr)) { break; }

		if (dwFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
			printf("Type change - not supported by WAVE file format.\n");
			break;
		}
		if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
			printf("End of input file.\n");
			break;
		}

		if (pSample == NULL) {
			//printf("No sample\n");
			continue;
		}

		// Get a pointer to the audio data in the sample.
		hr = pSample->ConvertToContiguousBuffer(&pBuffer);
		if (FAILED(hr)) { break; }

		hr = pBuffer->Lock(&pAudioData, NULL, &cbBuffer);
		if (FAILED(hr)) { break; }

		// Make sure not to exceed the specified maximum size.
		if (cbMaxAudioData - cbAudioData < cbBuffer) {
			cbBuffer = cbMaxAudioData - cbAudioData;
		}

		// Write this data to the output file.
		hr = WriteToFile(hFile, pAudioData, cbBuffer);

		if (FAILED(hr)) { break; }

		// Unlock the buffer.
		hr = pBuffer->Unlock();
		pAudioData = NULL;

		if (FAILED(hr)) { break; }

		// Update running total of audio data.
		cbAudioData += cbBuffer;

		if (cbAudioData >= cbMaxAudioData) {
			break;
		}

		SafeRelease(&pSample);
		SafeRelease(&pBuffer);
	}

	if (SUCCEEDED(hr)) {
		printf("Wrote %d bytes of audio data.\n", cbAudioData);
		*pcbDataWritten = cbAudioData;
	}

	if (pAudioData) {
		pBuffer->Unlock();
	}

	SafeRelease(&pBuffer);
	SafeRelease(&pSample);
	return hr;
}


// Calculates how much audio to write to the WAVE file, given the
// audio format and the maximum duration of the WAVE file.
DWORD CalculateMaxAudioDataSize(
								IMFMediaType *pReaderType,    // The audio format.
								DWORD cbHeader,              // The size of the WAVE file header.
								DWORD msecAudioData          // Maximum duration, in milliseconds.
								)
{
	UINT32 cbBlockSize = 0;         // Audio frame size, in bytes.
	UINT32 cbBytesPerSecond = 0;    // Bytes per second.

	// Get the audio block size and number of bytes/second from the audio format.
	cbBlockSize = MFGetAttributeUINT32(pReaderType, MF_MT_AUDIO_BLOCK_ALIGNMENT, 0);
	cbBytesPerSecond = MFGetAttributeUINT32(pReaderType, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 0);

	// Calculate the maximum amount of audio data to write.
	// This value equals (duration in seconds x bytes/second), but cannot
	// exceed the maximum size of the data chunk in the WAVE file.
	// Size of the desired audio clip in bytes:
	DWORD cbAudioClipSize = (DWORD)MulDiv(cbBytesPerSecond, msecAudioData, 1000);

	// Largest possible size of the data chunk:
	DWORD cbMaxSize = MAXDWORD - cbHeader;

	// Maximum size altogether.
	cbAudioClipSize = min(cbAudioClipSize, cbMaxSize);

	// Round to the audio block size, so that we do not write a partial audio frame.
	cbAudioClipSize = (cbAudioClipSize / cbBlockSize) * cbBlockSize;

	return cbAudioClipSize;
}

// Write the WAVE file header.
// Note: This function writes placeholder values for the file size
// and data size, as these values will need to be filled in later.
HRESULT WriteWaveHeader(
						HANDLE hFile,               // Output file.
						IMFMediaType *pMediaType,   // The audio format.
						DWORD *pcbWritten           // Receives the size of the header.
						)
{
	HRESULT hr = S_OK;
	UINT32 cbFormat = 0;
	WAVEFORMATEX *pWav = NULL;
	*pcbWritten = 0;

	// Convert the audio format into a WAVEFORMATEX structure.
	hr = MFCreateWaveFormatExFromMFMediaType(pMediaType, &pWav, &cbFormat);

	// Write the 'RIFF' header and the start of the 'fmt ' chunk.
	if (SUCCEEDED(hr))  {
		DWORD header[] = {
			// RIFF header
			FCC('RIFF'),
			0,
			FCC('WAVE'),
			// Start of 'fmt ' chunk
			FCC('fmt '),
			cbFormat
		};
		DWORD dataHeader[] = { FCC('data'), 0 };
		hr = WriteToFile(hFile, header, sizeof(header));

		// Write the WAVEFORMATEX structure.
		if (SUCCEEDED(hr)) {
			hr = WriteToFile(hFile, pWav, cbFormat);
		}

		// Write the start of the 'data' chunk
		if (SUCCEEDED(hr)) {
			hr = WriteToFile(hFile, dataHeader, sizeof(dataHeader));
		}
		if (SUCCEEDED(hr)) {
			*pcbWritten = sizeof(header) + cbFormat + sizeof(dataHeader);
		}
	}

	CoTaskMemFree(pWav);
	return hr;
}

// Writes a WAVE file by getting audio data from the source reader.
HRESULT WriteWaveFile(
					  IMFSourceReader *pReader,   // Pointer to the source reader.
					  WCHAR *szFileName,           // Name of the output file.
					  LONG msecAudioData          // Maximum amount of audio data to write, in msec.
					  )
{
	HRESULT hr = S_OK;
	DWORD cbHeader = 0;         // Size of the WAVE file header, in bytes.
	DWORD cbAudioData = 0;      // Total bytes of audio data written to the file.
	DWORD cbMaxAudioData = 0;
	IMFMediaType *pReaderType = NULL;    // Represents the incoming audio format.

	// Create the output file
	HANDLE hFile = INVALID_HANDLE_VALUE;
	hFile = CreateFileW(szFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
		CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		wprintf(L"Cannot create output file: %s\n", szFileName, hr);
		goto CLEANUP;
	}

	// Configure the source reader
	hr = ConfigureWaveReader(pReader, &pReaderType);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureWaveReader failed"));
		goto CLEANUP;
	}

	// Write the WAVE file header.
	if (SUCCEEDED(hr)) {
		hr = WriteWaveHeader(hFile, pReaderType, &cbHeader);
	}

	// Calculate the maximum amount of audio to decode, in bytes and decode
	if (SUCCEEDED(hr)) {
		cbMaxAudioData = CalculateMaxAudioDataSize(pReaderType, cbHeader, msecAudioData);
		// Decode audio data to the file.
		hr = WriteWaveData(hFile, pReader, cbMaxAudioData, &cbAudioData);
	}

	// Fix up the RIFF headers with the correct sizes.
	if (SUCCEEDED(hr)) {
		hr = FixUpChunkSizes(hFile, cbHeader, cbAudioData);
	}

CLEANUP:
	if (hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
	}
	SafeRelease(&pReaderType);
	return hr;
}
