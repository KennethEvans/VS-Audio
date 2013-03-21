#include "stdafx.h"
#include "mfWma.h"
#include "mfRoutines.h"

struct EncodingParameters
{
    GUID    subtype;
    UINT32  bitrate;
};

HRESULT CopyAttribute(IMFAttributes *pSrc, IMFAttributes *pDest, const GUID& key)
{
	PROPVARIANT var;
	PropVariantInit(&var);

	HRESULT hr = S_OK;

	hr = pSrc->GetItem(key, &var);
	if (SUCCEEDED(hr)) {
		hr = pDest->SetItem(key, var);
	}

	PropVariantClear(&var);
	return hr;
}

// Selects an audio stream from the source file, and configures the
// stream to read MFAudioFormat_Float audio
HRESULT ConfigureWmfReader(
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
		printf("ConfigureWmfReader: Error in SetStreamSelection\n");
		printErrorDescription(hr);
		return hr;
	}

	// Create a media subtype that specifies float audio.
	hr = MFCreateMediaType(&pSubtype);
	if (SUCCEEDED(hr))  {
		hr = pSubtype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	} else {
		printf("ConfigureWmfReader: Error in SetGUID MFMediaType_Audio\n");
		printErrorDescription(hr);
		return hr;
	}
	if (SUCCEEDED(hr))  {
		//hr = pSubtype->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		hr = pSubtype->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
	} else {
		printf("ConfigureWmfReader: Error in SetGUID MFAudioFormat_Float\n");
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
		printf("ConfigureWmfReader: Error in SetCurrentMediaType\n");
		printErrorDescription(hr);
		return hr;
	}

	// Get the complete uncompressed format.
	if (SUCCEEDED(hr)) {
		hr = pReader->GetCurrentMediaType(
			(DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			&pReaderType);
	} else {
		printf("ConfigureWmfReader: Error in GetCurrentMediaType\n");
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
		printf("ConfigureWmfReader: Error in SetStreamSelection\n");
		printErrorDescription(hr);
		return hr;
	}

	// Return the format to the caller.
	if (SUCCEEDED(hr)) {
		*ppPCMAudio = pReaderType;
		(*ppPCMAudio)->AddRef();
	} else {
		printf("ConfigureWmfReader: Error in AddRef\n");
		printErrorDescription(hr);
		return hr;
	}

	SafeRelease(&pReaderType);
	SafeRelease(&pSubtype);
	return hr;
}

HRESULT ConfigureEncoder(
						 const EncodingParameters &params,
						 IMFMediaType *pReaderType,
						 IMFSinkWriter *pWriter,
						 DWORD *sink_stream
						 )
{
	HRESULT hr = S_OK;
	IMFMediaType *pType2 = NULL;

	hr = MFCreateMediaType(&pType2);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureEncoder: MFCreateMediaType failed"));
		goto DONE;
	}

	hr = pType2->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureEncoder: SetGUID MF_MT_MAJOR_TYPE failed"));
		goto DONE;
	}

	hr = pType2->SetGUID(MF_MT_SUBTYPE, params.subtype);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureEncoder: SetGUID MF_MT_SUBTYPE failed"));
		goto DONE;
	}

	hr = pType2->SetUINT32(MF_MT_AVG_BITRATE, params.bitrate);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureEncoder: SetUINT32 MF_MT_AVG_BITRATE failed"));
		goto DONE;
	}

	UINT32 count = 0xFFFF;
	UINT32 count2 = 0xFFFF;
	hr = pReaderType->GetCount(&count);
	hr = pReaderType->GetCount(&count2);
		// Copy all the parameters
		GUID guid;
		PROPVARIANT propVariant;
		for(UINT32 i=0; i < count; i++) {
			hr = pReaderType->GetItemByIndex(i, &guid, &propVariant);
			if(FAILED(hr)) {
				debugMsg(_T("ConfigureEncoder: ")
					_T("GetItemByIndex failed for index %d (0x%08X)\n"), i);
				ShowMessage(hr, szDebugString);
				goto DONE;
			}
			hr = CopyAttribute(pReaderType, pType2, guid);
			if(FAILED(hr)) {
				debugMsg(_T("ConfigureEncoder: ")
					_T("CopyAttribute failed for index %d (0x%08X)\n"), i);
				ShowMessage(hr, szDebugString);
				goto DONE;
			}
		}

	// Debug
	debugMsg(_T("ConfigureEncoder: ")
		_T("pReaderType->GetCount=%d pType2->GetCount=%d\n"),
		count, count2);

	hr = pWriter->AddStream(pType2, sink_stream);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureEncoder: AddStream failed"));
		goto DONE;
	}

DONE:
	SafeRelease(&pType2);
	return hr;
}


// Writes a WMA file by getting audio data from the source reader.
HRESULT WriteWmaFile(
					  IMFSourceReader *pReader,   // Pointer to the source reader.
					  WCHAR *szFileName,          // Name of the output file.
					  LONG msecAudioData          // Maximum amount of audio data to write, in msec.
					  )
{
	HRESULT hr = S_OK;
	DWORD sink_stream = 0;      // Stream in the sink
	DWORD cbHeader = 0;         // Size of the WAVE file header, in bytes.
	DWORD cbAudioData = 0;      // Total bytes of audio data written to the file.
	DWORD cbMaxAudioData = 0;
	IMFMediaType *pReaderType = NULL;    // Represents the reader audio format.
	EncodingParameters params;

	// Configure the source reader to get uncompressed audio from the source file.
	hr = ConfigureWmfReader(pReader, &pReaderType);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureCapture: ConfigureWmfReader failed"));
		goto DONE;
	}

	// Create the sink writer
	IMFSinkWriter *pWriter;
	hr = MFCreateSinkWriterFromURL(szFileName, NULL, NULL,&pWriter);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("MFCreateSinkWriterFromURL failed"));
#if 0
		// Debug
		ShowMessage(hr, _T("pwszFileName=%s", pwszFileName));
#endif
		goto DONE;
	}

	params.subtype = MFAudioFormat_WMAudioV8;
	params.bitrate = 240 * 1000;
	hr = ConfigureEncoder(params, pReaderType, pWriter, &sink_stream);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("ConfigureEncoder failed"));
		goto DONE;
	}

	//// Calculate the maximum amount of audio to decode, in bytes.
	//if (SUCCEEDED(hr)) {
	//	cbMaxAudioData = CalculateMaxAudioDataSize(pReaderType, cbHeader, msecAudioData);
	//	// Decode audio data to the file.
	//	hr = WriteWaveData(hFile, pReader, cbMaxAudioData, &cbAudioData);
	//}

	hr = pWriter->SetInputMediaType(sink_stream, pReaderType, NULL);
	if(FAILED(hr)) {
		ShowMessage(hr, _T("SetInputMediaType failed"));
		goto DONE;
	}

	hr = pWriter->BeginWriting();
	if(FAILED(hr)) {
		ShowMessage(hr, _T("BeginWriting failed"));
		goto DONE;
	}

DONE:
	SafeRelease(&pReaderType);
	return hr;
}
