//////////////////////////////////////////////////////////////////////////
//
// capture.cpp: Manages media capture.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "utils.h"
#include "mfUtils.h"

#include "capture.h"

HRESULT CopyAttribute(IMFAttributes *pSrc, IMFAttributes *pDest, const GUID& key);


void DeviceList::Clear()
{
	for (UINT32 i = 0; i < m_cDevices; i++)
	{
		SafeRelease(&m_ppDevices[i]);
	}
	CoTaskMemFree(m_ppDevices);
	m_ppDevices = NULL;

	m_cDevices = 0;
}

BOOL DeviceList::IsAudio()
{
	return m_useAudio;
}

HRESULT DeviceList::EnumerateDevices(BOOL useAudio)
{
	HRESULT hr = S_OK;
	IMFAttributes *pAttributes = NULL;

	m_useAudio = useAudio;

	Clear();

	// Initialize an attribute store. We will use this to
	// specify the enumeration parameters.

	hr = MFCreateAttributes(&pAttributes, 1);

	// Ask for source type = media capture devices
	if (SUCCEEDED(hr)) {
		hr = pAttributes->SetGUID(
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			useAudio ? MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID :
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
			);
	}

	// Enumerate devices.
	if (SUCCEEDED(hr)) {
		hr = MFEnumDeviceSources(pAttributes, &m_ppDevices, &m_cDevices);
	}

	if(FAILED(hr)) {
		ShowMessage(hr, L"Failed to enumerate devices");
	}

	SafeRelease(&pAttributes);
	return hr;
}


HRESULT DeviceList::GetDevice(UINT32 index, IMFActivate **ppActivate)
{
	if (index >= Count())
	{
		return E_INVALIDARG;
	}

	*ppActivate = m_ppDevices[index];
	(*ppActivate)->AddRef();

	return S_OK;
}

HRESULT DeviceList::GetDeviceName(UINT32 index, WCHAR **ppszName)
{
	if (index >= Count())
	{
		return E_INVALIDARG;
	}

	HRESULT hr = S_OK;

	hr = m_ppDevices[index]->GetAllocatedString(
		MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
		ppszName,
		NULL
		);

	return hr;
}



//-------------------------------------------------------------------
//  CreateInstance
//
//  Static class method to create the CCapture object.
//-------------------------------------------------------------------

HRESULT CCapture::CreateInstance(
								 HWND     hwnd,       // Handle to the window to receive events
								 BOOL useAudio,        // Whether to use audio or video
								 CCapture **ppCapture // Receives a pointer to the CCapture object.
								 )
{
	if (ppCapture == NULL) {
		return E_POINTER;
	}

	CCapture *pCapture = new (std::nothrow) CCapture(hwnd, useAudio);
	if (pCapture == NULL) {
		return E_OUTOFMEMORY;
	}

	// The CCapture constructor sets the ref count to 1.
	*ppCapture = pCapture;

	return S_OK;
}


//-------------------------------------------------------------------
//  constructor
//-------------------------------------------------------------------

CCapture::CCapture(HWND hwnd, BOOL useAudio) :
m_pReader(NULL),
m_pWriter(NULL),
m_hwndEvent(hwnd),
m_nRefCount(1),
m_bFirstSample(FALSE),
m_llBaseTime(0),
m_pwszSymbolicLink(NULL),
m_useAudio(useAudio)
{
	InitializeCriticalSection(&m_critsec);
}

//-------------------------------------------------------------------
//  destructor
//-------------------------------------------------------------------

CCapture::~CCapture()
{
	assert(m_pReader == NULL);
	assert(m_pWriter == NULL);
	DeleteCriticalSection(&m_critsec);
}


/////////////// IUnknown methods ///////////////

//-------------------------------------------------------------------
//  AddRef
//-------------------------------------------------------------------

ULONG CCapture::AddRef()
{
	return InterlockedIncrement(&m_nRefCount);
}


//-------------------------------------------------------------------
//  Release
//-------------------------------------------------------------------

ULONG CCapture::Release()
{
	ULONG uCount = InterlockedDecrement(&m_nRefCount);
	if (uCount == 0)
	{
		delete this;
	}
	return uCount;
}



//-------------------------------------------------------------------
//  QueryInterface
//-------------------------------------------------------------------

HRESULT CCapture::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(CCapture, IMFSourceReaderCallback),
		{ 0 },
	};
	return QISearch(this, qit, riid, ppv);
}


/////////////// IMFSourceReaderCallback methods ///////////////

//-------------------------------------------------------------------
// OnReadSample
//
// Called when the IMFMediaSource::ReadSample method completes.
//-------------------------------------------------------------------

HRESULT CCapture::OnReadSample(
							   HRESULT hrStatus,
							   DWORD /*dwStreamIndex*/,
							   DWORD /*dwStreamFlags*/,
							   LONGLONG llTimeStamp,
							   IMFSample *pSample      // Can be NULL
							   )
{
	EnterCriticalSection(&m_critsec);
	if (!IsCapturing())	{
		LeaveCriticalSection(&m_critsec);
		return S_OK;
	}

	HRESULT hr = S_OK;

	if (FAILED(hrStatus)) {
		hr = hrStatus;
		goto DONE;
	}

	if (pSample) {
		if (m_bFirstSample) {
			m_llBaseTime = llTimeStamp;
			m_bFirstSample = FALSE;
		}

		// rebase the time stamp
		llTimeStamp -= m_llBaseTime;

		hr = pSample->SetSampleTime(llTimeStamp);
		if (FAILED(hr)) { goto DONE; }

		hr = m_pWriter->WriteSample(0, pSample);
		if (FAILED(hr)) { goto DONE; }
	}

	// Read another sample.
	hr = m_pReader->ReadSample(
		m_useAudio ? (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM :
		(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		0,
		NULL,   // actual
		NULL,   // flags
		NULL,   // timestamp
		NULL    // sample
		);

DONE:
	if (FAILED(hr))	{
		NotifyError(hr);
	}

	LeaveCriticalSection(&m_critsec);
	return hr;
}


//-------------------------------------------------------------------
// OpenMediaSource
//
// Set up preview for a specified media source.
//-------------------------------------------------------------------

HRESULT CCapture::OpenMediaSource(IMFMediaSource *pSource)
{
	HRESULT hr = S_OK;

	IMFAttributes *pAttributes = NULL;

	hr = MFCreateAttributes(&pAttributes, 2);

	if (SUCCEEDED(hr))
	{
		hr = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFCreateSourceReaderFromMediaSource(
			pSource,
			pAttributes,
			&m_pReader
			);
	}

	SafeRelease(&pAttributes);
	return hr;
}


//-------------------------------------------------------------------
// StartCapture
//
// Start capturing.
//-------------------------------------------------------------------

HRESULT CCapture::StartCapture(
							   IMFActivate *pActivate,
							   const WCHAR *pwszFileName,
							   const EncodingParameters &param
							   )
{
#if 0
	// DEBUG
	WCHAR *subtype;
	if(param.subtype == MFAudioFormat_MPEG) {
		subtype = L"MFAudioFormat_MPEG";
	} else if(param.subtype == MFAudioFormat_MP3) {
		subtype = L"MFAudioFormat_MP3";
	} else if(param.subtype == MFAudioFormat_AAC) {
		subtype = L"MFAudioFormat_AAC";
	} else if(param.subtype == MFAudioFormat_WMAudioV8) {
		subtype = L"MFAudioFormat_WMAudioV8";
	} else if(param.subtype == MFAudioFormat_WMAudioV9) {
		subtype = L"MFAudioFormat_WMAudioV9";
	} else if(param.subtype == MFAudioFormat_WMAudio_Lossless) {
		subtype = L"MFAudioFormat_WMAudio_Lossless";
	} else if(param.subtype == MFVideoFormat_WMV3) {
		subtype = L"MFVideoFormat_WMV3";
	} else if(param.subtype == MFVideoFormat_H264) {
		subtype = L"MFVideoFormat_H264";
	} else {
		subtype = L"Not Regognized";
	}
	errMsg(L"StartCapture\n\n"
		L"FileName: %s\nparam.subtype: %s\nparam.bitrate: %d\n", pwszFileName,
		subtype, param.bitrate);
#endif

	HRESULT hr = S_OK;
	IMFMediaSource *pSource = NULL;
	EnterCriticalSection(&m_critsec);

	// Create the media source for the device.
	hr = pActivate->ActivateObject(
		__uuidof(IMFMediaSource),
		(void**)&pSource
		);
	if(FAILED(hr)) {
		ShowMessage(hr, L"StartCapture: ActivateObject failed");
	}

	// Get the symbolic link. This is needed to handle device-
	// loss notifications. (See CheckDeviceLost.)

	if (SUCCEEDED(hr)) {
		hr = pActivate->GetAllocatedString(
			m_useAudio ? MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID :
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
			&m_pwszSymbolicLink,
			NULL
			);
	}
	// Debug
	if (SUCCEEDED(hr)) {
		debugMsg(L"StartCapture: "
			L"m_pwszSymbolicLink=%s\n", m_pwszSymbolicLink);
	}

	if (SUCCEEDED(hr)) {
		hr = OpenMediaSource(pSource);
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"StartCapture: OpenMediaSource failed");
	}

	// Create the sink writer
	if (SUCCEEDED(hr)) {
		hr = MFCreateSinkWriterFromURL(
			pwszFileName,
			NULL,
			NULL,
			&m_pWriter
			);
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"StartCapture: MFCreateSinkWriterFromURL failed");
#if 0
		// Debug
		ShowMessage(hr, L"pwszFileName=%s", pwszFileName);
#endif
	}

	// Set up the encoding parameters.
	if (SUCCEEDED(hr)) {
		hr = ConfigureCapture(param, m_useAudio);
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"StartCapture: ConfigureCapture failed");
	}

	if (SUCCEEDED(hr)) {
		m_bFirstSample = TRUE;
		m_llBaseTime = 0;

		// Request the first frame.
		hr = m_pReader->ReadSample(
			m_useAudio ? (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM :
			(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,
			NULL,
			NULL,
			NULL,
			NULL
			);
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"StartCapture: ReadSample failed");
	}

	SafeRelease(&pSource);
	LeaveCriticalSection(&m_critsec);
	return hr;
}


//-------------------------------------------------------------------
// EndCaptureSession
//
// Stop the capture session.
//
// NOTE: This method resets the object's state to State_NotReady.
// To start another capture session, call SetCaptureFile.
//-------------------------------------------------------------------

HRESULT CCapture::EndCaptureSession()
{
	EnterCriticalSection(&m_critsec);

	HRESULT hr = S_OK;

	if (m_pWriter)
	{
		hr = m_pWriter->Finalize();
	}

	SafeRelease(&m_pWriter);
	SafeRelease(&m_pReader);

	LeaveCriticalSection(&m_critsec);

	return hr;
}


BOOL CCapture::IsCapturing()
{
	EnterCriticalSection(&m_critsec);
	BOOL bIsCapturing = (m_pWriter != NULL);
	LeaveCriticalSection(&m_critsec);

	return bIsCapturing;
}



//-------------------------------------------------------------------
//  CheckDeviceLost
//  Checks whether the media capture device was removed.
//
//  The application calls this method when is receives a
//  WM_DEVICECHANGE message.
//-------------------------------------------------------------------

HRESULT CCapture::CheckDeviceLost(DEV_BROADCAST_HDR *pHdr, BOOL *pbDeviceLost)
{
	if (pbDeviceLost == NULL)
	{
		return E_POINTER;
	}

	EnterCriticalSection(&m_critsec);

	DEV_BROADCAST_DEVICEINTERFACE *pDi = NULL;

	*pbDeviceLost = FALSE;

	if (!IsCapturing())
	{
		goto DONE;
	}
	if (pHdr == NULL)
	{
		goto DONE;
	}
	if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
	{
		goto DONE;
	}

	// Compare the device name with the symbolic link.

	pDi = (DEV_BROADCAST_DEVICEINTERFACE*)pHdr;

	if (m_pwszSymbolicLink)
	{
		if (_wcsicmp(m_pwszSymbolicLink, pDi->dbcc_name) == 0)
		{
			*pbDeviceLost = TRUE;
		}
	}

DONE:
	LeaveCriticalSection(&m_critsec);
	return S_OK;
}


/////////////// Private/protected class methods ///////////////



//-------------------------------------------------------------------
//  ConfigureSourceReader
//
//  Sets the media type on the source reader.
//-------------------------------------------------------------------

HRESULT ConfigureSourceReader(IMFSourceReader *pReader, BOOL useAudio)
{
	// The list of acceptable types.
	GUID audioSubtypes[] = {
		MFAudioFormat_Float,
		MFAudioFormat_WMAudioV8, MFAudioFormat_WMAudioV9, 
		MFAudioFormat_WMAudio_Lossless, MFAudioFormat_MP3,
		MFAudioFormat_MPEG, MFAudioFormat_AAC,
		MFAudioFormat_PCM
	};
	UINT32 nAudioSubtypes = ARRAYSIZE(audioSubtypes);
	GUID videoSubtypes[] = {
		MFVideoFormat_NV12, MFVideoFormat_YUY2, MFVideoFormat_UYVY,
		MFVideoFormat_RGB32, MFVideoFormat_RGB24, MFVideoFormat_IYUV
	};
	UINT32 nVideoSubtypes = ARRAYSIZE(audioSubtypes);
	GUID *subtypes;
	UINT32 nSubtypes;
	if(useAudio) {
		subtypes = audioSubtypes;
		nSubtypes = nAudioSubtypes;
	} else {
		subtypes = videoSubtypes;
		nSubtypes = nVideoSubtypes;
	}

	HRESULT hr = S_OK;
	BOOL    bUseNativeType = FALSE;
	GUID subtype = { 0 };
	IMFMediaType *pType = NULL;

	// If the source's native format matches any of the formats in
	// the list, prefer the native format.

	// Note: The source might support multiple output formats,
	// including a range of frame dimensions. The application could
	// provide a list to the user and have the user select the
	// camera's output format. That is outside the scope of this
	// sample, however.
	hr = pReader->GetNativeMediaType(
		useAudio ? (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM :
		(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		0,  // Type index
		&pType
		);

	if (FAILED(hr)) {
		ShowMessage(hr, L"ConfigureSourceReader: GetNativeMediaType failed");
		goto DONE;
	}

	hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
	if (FAILED(hr)) {
		ShowMessage(hr, L"ConfigureSourceReader: GetGUID MF_MT_SUBTYPE failed");
		goto DONE;
	}

	// See if subtype is one of ours and use it if so
	for (UINT32 i = 0; i < nSubtypes; i++) {
		if (subtype == subtypes[i]) {
			hr = pReader->SetCurrentMediaType(
				useAudio ? (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM :
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				NULL,
				pType
				);

			bUseNativeType = TRUE;
			break;
		}
	}

	// If not found found, try each of ours in turn
	if (!bUseNativeType) {
		// None of the native types worked. The camera might offer
		// output a compressed type such as MJPEG or DV.

		// Try adding a decoder (MF_MT_SUBTYPE).
		for (UINT32 i = 0; i < nSubtypes; i++) {
			hr = pType->SetGUID(MF_MT_SUBTYPE, subtypes[i]);
			if (FAILED(hr)) {
				debugMsg(L"ConfigureSourceReader: "
					L"SetGUID MF_MT_SUBTYPE failed for subtype %d (0x%08X)\n",
					i);
				goto DONE;
			}

			hr = pReader->SetCurrentMediaType(
				useAudio ? (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM :
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				NULL,
				pType
				);
			if (SUCCEEDED(hr)) {
				break;
			} else {
				// Debug
				debugMsg(L"ConfigureSourceReader: "
					L"Failed for subtype %d (0x%08X)\n", i);
			}
		}
		if (FAILED(hr)) {
			ShowMessage(hr, L"ConfigureSourceReader: "
				L"Failed to set any of our subtypes (0x%08X)");
			goto DONE;
		}
	}

DONE:
	SafeRelease(&pType);
	return hr;
}

HRESULT ConfigureEncoder(
						 const EncodingParameters &params,
						 BOOL useAudio,
						 IMFMediaType *pType,
						 IMFSinkWriter *pWriter,
						 DWORD *sink_stream
						 )
{
	HRESULT hr = S_OK;
	IMFMediaType *pType2 = NULL;

	hr = MFCreateMediaType(&pType2);
	if(FAILED(hr)) {
		ShowMessage(hr, L"ConfigureEncoder: MFCreateMediaType failed");
		goto DONE;
	}

	if(useAudio) {
		hr = pType2->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	} else {
		hr = pType2->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"ConfigureEncoder: SetGUID MF_MT_MAJOR_TYPE failed");
		goto DONE;
	}

	hr = pType2->SetGUID(MF_MT_SUBTYPE, params.subtype);
	if(FAILED(hr)) {
		ShowMessage(hr, L"ConfigureEncoder: SetGUID MF_MT_SUBTYPE failed");
		goto DONE;
	}

	hr = pType2->SetUINT32(MF_MT_AVG_BITRATE, params.bitrate);
	if(FAILED(hr)) {
		ShowMessage(hr, L"ConfigureEncoder: SetUINT32 MF_MT_AVG_BITRATE failed");
		goto DONE;
	}

	UINT32 count = 0xFFFF;
	UINT32 count2 = 0xFFFF;
	hr = pType->GetCount(&count);
	hr = pType->GetCount(&count2);
	if(useAudio) {
		// Copy all the parameters
		GUID guid;
		PROPVARIANT propVariant;
		for(UINT32 i=0; i < count; i++) {
			hr = pType->GetItemByIndex(i, &guid, &propVariant);
			if(FAILED(hr)) {
				debugMsg(L"ConfigureEncoder: "
					L"GetItemByIndex failed for index %d (0x%08X)\n", i);
				ShowMessage(hr, szDebugString);
				goto DONE;
			}
			hr = CopyAttribute(pType, pType2, guid);
			if(FAILED(hr)) {
				debugMsg(L"ConfigureEncoder: "
					L"CopyAttribute failed for index %d (0x%08X)\n", i);
				ShowMessage(hr, szDebugString);
				goto DONE;
			}
		}
	} else {
		hr = CopyAttribute(pType, pType2, MF_MT_FRAME_SIZE);
		if(FAILED(hr)) {
			ShowMessage(hr, NULL,
				L"ConfigureEncoder: CopyAttribute MF_MT_FRAME_SIZE failed");
			goto DONE;
		}

		hr = CopyAttribute(pType, pType2, MF_MT_FRAME_RATE);
		if(FAILED(hr)) {
			ShowMessage(hr, NULL,
				L"ConfigureEncoder: CopyAttribute MF_MT_FRAME_RATE failed");
			goto DONE;
		}

		hr = CopyAttribute(pType, pType2, MF_MT_PIXEL_ASPECT_RATIO);
		if(FAILED(hr)) {
			ShowMessage(hr, NULL,
				L"ConfigureEncoder: CopyAttribute MF_MT_PIXEL_ASPECT_RATIO failed");
			goto DONE;
		}

		hr = CopyAttribute(pType, pType2, MF_MT_INTERLACE_MODE);
		if(FAILED(hr)) {
			ShowMessage(hr, NULL,
				L"ConfigureEncoder: CopyAttribute MF_MT_INTERLACE_MODE failed");
			goto DONE;
		}
	}

	// Debug
	debugMsg(L"ConfigureEncoder: "
		L"pType->GetCount=%d pType2->GetCount=%d\n",
		count, count2);

	hr = pWriter->AddStream(pType2, sink_stream);
	if(FAILED(hr)) {
		ShowMessage(hr, L"ConfigureEncoder: AddStream failed");
		goto DONE;
	}

DONE:
	SafeRelease(&pType2);
	return hr;
}

//-------------------------------------------------------------------
// ConfigureCapture
//
// Configures the capture session.
//
//-------------------------------------------------------------------

HRESULT CCapture::ConfigureCapture(const EncodingParameters &param,
								   BOOL useAudio) {
									   HRESULT hr = S_OK;
									   DWORD sink_stream = 0;
									   IMFMediaType *pType = NULL;

									   hr = ConfigureSourceReader(m_pReader, m_useAudio);
									   if(FAILED(hr)) {
										   ShowMessage(hr, L"ConfigureCapture: ConfigureSourceReader failed");
										   goto DONE;
									   }

									   hr = m_pReader->GetCurrentMediaType(
										   useAudio ? (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM :
										   (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
										   &pType
										   );
									   if(FAILED(hr)) {
										   ShowMessage(hr, L"ConfigureCapture: GetCurrentMediaType failed");
										   goto DONE;
									   }

									   hr = ConfigureEncoder(param, m_useAudio, pType, m_pWriter, &sink_stream);
									   if(FAILED(hr)) {
										   ShowMessage(hr, L"ConfigureCapture: ConfigureEncoder failed");
										   goto DONE;
									   }

									   if(!useAudio) {
										   // Register the color converter DSP for this process, in the video
										   // processor category. This will enable the sink writer to enumerate
										   // the color converter when the sink writer attempts to match the
										   // media types.

										   hr = MFTRegisterLocalByCLSID(
											   __uuidof(CColorConvertDMO),
											   MFT_CATEGORY_VIDEO_PROCESSOR,
											   L"",
											   MFT_ENUM_FLAG_SYNCMFT,
											   0,
											   NULL,
											   0,
											   NULL
											   );
										   if(FAILED(hr)) {
											   ShowMessage(hr, L"ConfigureCapture: MFTRegisterLocalByCLSID failed");
											   goto DONE;
										   }
									   }

									   hr = m_pWriter->SetInputMediaType(sink_stream, pType, NULL);
									   if(FAILED(hr)) {
										   ShowMessage(hr, L"ConfigureCapture: SetInputMediaType failed");
										   goto DONE;
									   }

									   hr = m_pWriter->BeginWriting();
									   if(FAILED(hr)) {
										   ShowMessage(hr, L"ConfigureCapture: BeginWriting failed");
										   goto DONE;
									   }

DONE:
									   SafeRelease(&pType);
									   return hr;
}


//-------------------------------------------------------------------
// EndCaptureInternal
//
// Stops capture.
//-------------------------------------------------------------------

HRESULT CCapture::EndCaptureInternal()
{
	HRESULT hr = S_OK;

	if (m_pWriter)
	{
		hr = m_pWriter->Finalize();
	}

	SafeRelease(&m_pWriter);
	SafeRelease(&m_pReader);

	CoTaskMemFree(m_pwszSymbolicLink);
	m_pwszSymbolicLink = NULL;

	return hr;
}




//-------------------------------------------------------------------
// CopyAttribute
//
// Copy an attribute value from one attribute store to another.
//-------------------------------------------------------------------

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
