//////////////////////////////////////////////////////////////////////////
//
// winmain.cpp. Application entry-point.
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
#include "resource.h"

// Include the v6 common controls in the manifest
#pragma comment(linker, \
	"\"/manifestdependency:type='Win32' "\
	"name='Microsoft.Windows.Common-Controls' "\
	"version='6.0.0.0' "\
	"processorArchitecture='*' "\
	"publicKeyToken='6595b64144ccf1df' "\
	"language='*'\"")

enum FileContainer {
	FileContainer_TOP = IDC_CAPTURE_TOP,
	FileContainer_BOTTOM = IDC_CAPTURE_BOTTOM
};

DeviceList  g_devices;
CCapture    *g_pCapture = NULL;
HDEVNOTIFY  g_hdevnotify = NULL;
BOOL g_useAudio = 1;
FileContainer g_file = FileContainer_TOP;
int g_lastAudioDevice = 0;
int g_lastVideoDevice = 0;

const UINT32 TARGET_BIT_RATE = 240 * 1000;

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void    OnInitDialog(HWND hDlg);
void    OnCloseDialog();

void    UpdateUI(HWND hDlg);
void    StopCapture(HWND hDlg);
void    StartCapture(HWND hDlg);
void    OnSelectEncodingType(HWND hDlg);

HRESULT GetSelectedDevice(HWND hDlg, IMFActivate **ppActivate);
HRESULT UpdateDeviceList(HWND hDlg);
void    OnDeviceChange(HWND hwnd, WPARAM reason, DEV_BROADCAST_HDR *pHdr);

void    EnableDialogControl(HWND hDlg, int nIDDlgItem, BOOL bEnable);

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
					LPWSTR /*lpCmdLine*/, INT /*nCmdShow*/)
{
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	INT_PTR ret = DialogBox(
		hInstance,
		MAKEINTRESOURCE(IDD_DIALOG1),
		NULL,
		DialogProc
		);

	if (ret == 0 || ret == -1) {
		MessageBox( NULL, L"Could not create dialog", L"Error",
			MB_OK | MB_ICONERROR );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Dialog procedure
//-----------------------------------------------------------------------------

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_INITDIALOG:
			OnInitDialog(hDlg);
			break;
		case WM_DEVICECHANGE:
			OnDeviceChange(hDlg, wParam, (PDEV_BROADCAST_HDR)lParam);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_CAPTURE_TOP:
					g_file = FileContainer_TOP;
					OnSelectEncodingType(hDlg);
					return TRUE;
				case IDC_CAPTURE_BOTTOM:
					g_file = FileContainer_BOTTOM;
					OnSelectEncodingType(hDlg);
					return TRUE;
				case IDC_AUDIO:
					g_useAudio = true;
					OnSelectEncodingType(hDlg);
					return TRUE;
				case IDC_VIDEO:
					g_useAudio = false;
					OnSelectEncodingType(hDlg);
					return TRUE;
				case IDC_CAPTURE:
					if (g_pCapture && g_pCapture->IsCapturing()) {
						StopCapture(hDlg);
					} else {
						StartCapture(hDlg);
					}
					return TRUE;
				case IDCANCEL:
					OnCloseDialog();
					::EndDialog(hDlg, IDCANCEL);
					return TRUE;
			}
			break;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// OnInitDialog
// Handler for WM_INITDIALOG message.
//-----------------------------------------------------------------------------

void OnInitDialog(HWND hDlg) {
	HRESULT hr = S_OK;

	// Initialize the COM library
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// Set the default name for capture
	HWND hEdit = GetDlgItem(hDlg, IDC_OUTPUT_FILE);
    SetWindowText(hEdit, TEXT("capture.mp4"));

	// Initialize the button and file name
	OnSelectEncodingType(hDlg);

	// Initialize Media Foundation
	if (SUCCEEDED(hr)) {
		hr = MFStartup(MF_VERSION);
	}

	// Register for device notifications
	if (SUCCEEDED(hr)) {
		DEV_BROADCAST_DEVICEINTERFACE di = { 0 };

		di.dbcc_size = sizeof(di);
		di.dbcc_devicetype  = DBT_DEVTYP_DEVICEINTERFACE;
		di.dbcc_classguid  = KSCATEGORY_CAPTURE;

		g_hdevnotify = RegisterDeviceNotification(
			hDlg,
			&di,
			DEVICE_NOTIFY_WINDOW_HANDLE
			);

		if (g_hdevnotify == NULL) {
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	// Enumerate the capture devices.
	if (SUCCEEDED(hr)) {
		hr = UpdateDeviceList(hDlg);
	}

	if (SUCCEEDED(hr)) {
		UpdateUI(hDlg);
		if (g_devices.Count() == 0)	{
			::MessageBox(
				hDlg,
				TEXT("Could not find any capture devices."),
				TEXT("MFCaptureToFile"),
				MB_OK
				);
		}
	} else {
		OnCloseDialog();
		::EndDialog(hDlg, 0);
	}
}



//-----------------------------------------------------------------------------
// OnCloseDialog
//
// Frees resources before closing the dialog.
//-----------------------------------------------------------------------------

void OnCloseDialog()
{
	if (g_pCapture)
	{
		g_pCapture->EndCaptureSession();
	}

	SafeRelease(&g_pCapture);

	g_devices.Clear();

	if (g_hdevnotify)
	{
		UnregisterDeviceNotification(g_hdevnotify);
	}

	MFShutdown();
	CoUninitialize();
}


//-----------------------------------------------------------------------------
// StartCapture
//
// Starts the capture.
//-----------------------------------------------------------------------------

void StartCapture(HWND hDlg) {
	EncodingParameters params;

	if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_CAPTURE_TOP)) {
		// Note: Top Button
		if(g_useAudio) {
			params.subtype = MFAudioFormat_WMAudioV8;
		} else {
			params.subtype = MFVideoFormat_H264;
		}
	} else {
		// Note: Bottom button
		if(g_useAudio) {
			params.subtype = MFAudioFormat_MP3;
		} else {
			params.subtype = MFVideoFormat_WMV3;
		}
	}

	params.bitrate = TARGET_BIT_RATE;

	HRESULT hr = S_OK;
	WCHAR   pszFile[MAX_PATH] = { 0 };
	HWND    hEdit = GetDlgItem(hDlg, IDC_OUTPUT_FILE);

	IMFActivate *pActivate = NULL;

	// Get the name of the target file.

	if (0 == GetWindowText(hEdit, pszFile, MAX_PATH)) {
		hr = HRESULT_FROM_WIN32(GetLastError());
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"Failed to get value of capture file");
	}

	// Create the media source for the capture device.
	if (SUCCEEDED(hr)) {
		hr = GetSelectedDevice(hDlg, &pActivate);
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"Failed to get selected device");
	}
	// Start capturing.
	if (SUCCEEDED(hr)) {
		hr = CCapture::CreateInstance(hDlg, g_useAudio, &g_pCapture);
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"Failed to create capture instance");
	}

	if (SUCCEEDED(hr)) {
		hr = g_pCapture->StartCapture(pActivate, pszFile, params);
	}
	if(FAILED(hr)) {
		ShowMessage(hr, L"Error starting capture");
	}

	if (SUCCEEDED(hr)) {
		UpdateUI(hDlg);
	}

	SafeRelease(&pActivate);
}


//-----------------------------------------------------------------------------
// StopCapture
//
// Stops the capture.
//-----------------------------------------------------------------------------

void StopCapture(HWND hDlg)
{
	HRESULT hr = S_OK;
	hr = g_pCapture->EndCaptureSession();
	SafeRelease(&g_pCapture);
	UpdateDeviceList(hDlg);

	// NOTE: Updating the device list releases the existing IMFActivate
	// pointers. This ensures that the current instance of the capture
	// source is released.
	UpdateUI(hDlg);

	if (FAILED(hr))	{
		ShowMessage(hr, L"Error stopping capture - File might be corrupt");
	}
}



//-----------------------------------------------------------------------------
// CreateSelectedDevice
//
// Create a media source for the capture device selected by the user.
//-----------------------------------------------------------------------------

HRESULT GetSelectedDevice(HWND hDlg, IMFActivate **ppActivate)
{
	HWND hDeviceList = GetDlgItem(hDlg, IDC_DEVICE_LIST);

	// First get the index of the selected item in the combo box.
	int iListIndex = ComboBox_GetCurSel(hDeviceList);

	if (iListIndex == CB_ERR) {
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// Now find the index of the device within the device list.
	//
	// This index is stored as item data in the combo box, so that
	// the order of the combo box items does not need to match the
	// order of the device list.
	LRESULT iDeviceIndex = ComboBox_GetItemData(hDeviceList, iListIndex);

	if (iDeviceIndex == CB_ERR) {
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// Now create the media source.
	return g_devices.GetDevice((UINT32)iDeviceIndex, ppActivate);
}


//-----------------------------------------------------------------------------
// UpdateDeviceList
//
// Enumerates the capture devices and populates the list of device
// names in the dialog UI.
//-----------------------------------------------------------------------------

HRESULT UpdateDeviceList(HWND hDlg) {
	HRESULT hr = S_OK;
	WCHAR *szFriendlyName = NULL;
	HWND hCombobox = GetDlgItem(hDlg, IDC_DEVICE_LIST);

	ComboBox_ResetContent( hCombobox );
	g_devices.Clear();
	hr = g_devices.EnumerateDevices(g_useAudio);
	if (FAILED(hr)) { goto done; }

	for (UINT32 iDevice = 0; iDevice < g_devices.Count(); iDevice++) {
		// Get the friendly name of the device.
		hr = g_devices.GetDeviceName(iDevice, &szFriendlyName);
		if (FAILED(hr)) { goto done; }

		// Add the string to the combo-box. This message returns the index in the list.
		int iListIndex = ComboBox_AddString(hCombobox, szFriendlyName);
		if (iListIndex == CB_ERR || iListIndex == CB_ERRSPACE) {
			hr = E_FAIL;
			goto done;
		}

		// The list might be sorted, so the list index is not always the same as the
		// array index. Therefore, set the array index as item data.
		int result = ComboBox_SetItemData(hCombobox, iListIndex, iDevice);
		if (result == CB_ERR) {
			hr = E_FAIL;
			goto done;
		}

		CoTaskMemFree(szFriendlyName);
		szFriendlyName = NULL;
	}

	if (g_devices.Count() > 0) {
		// Select the first item.
		ComboBox_SetCurSel(hCombobox, 0);
	}

done:
	return hr;
}


//-----------------------------------------------------------------------------
// OnSelectEncodingType
//
// Called when the user toggles between file-format types.
//-----------------------------------------------------------------------------

void OnSelectEncodingType(HWND hDlg) {
	// Set whether we are using audio or video
	HWND hTop = GetDlgItem(hDlg, IDC_CAPTURE_TOP);
	HWND hBottom = GetDlgItem(hDlg, IDC_CAPTURE_BOTTOM);
	WCHAR pszFile[MAX_PATH] = { 0 };
	HWND hEdit = GetDlgItem(hDlg, IDC_OUTPUT_FILE);
	GetWindowText(hEdit, pszFile, MAX_PATH);

	// Set UI elements
	if(g_useAudio) {
		SetWindowText(hTop, L"WMA");
		SetWindowText(hBottom, L"MP3");
		switch (g_file) {
		  case FileContainer_TOP:
			  PathRenameExtension(pszFile, L".wma");
			  break;
		  case FileContainer_BOTTOM:
			  PathRenameExtension(pszFile, L".mp3");
			  break;
		  default:
			  assert(false);
			  break;
		}
	} else {
		SetWindowText(hTop, L"MP4");
		SetWindowText(hBottom, L"WMV");
		switch (g_file) {
		  case FileContainer_TOP:
			  PathRenameExtension(pszFile, L".mp4");
			  break;
		  case FileContainer_BOTTOM:
			  PathRenameExtension(pszFile, L".wmv");
			  break;
		  default:
			  assert(false);
			  break;
		}
	}

	SetWindowText(hEdit, pszFile);
	CheckRadioButton(hDlg, IDC_AUDIO, IDC_VIDEO,
		g_useAudio?IDC_AUDIO:IDC_VIDEO);
	CheckRadioButton(hDlg, IDC_CAPTURE_TOP, IDC_CAPTURE_BOTTOM,
		g_file);

	// Update the device list
	HWND hDeviceList = GetDlgItem(hDlg, IDC_DEVICE_LIST);
	if(g_devices.IsAudio()) {
		g_lastAudioDevice = ComboBox_GetCurSel(hDeviceList);
		// Don't let it be unselected
		if(g_lastAudioDevice < 0) g_lastAudioDevice = 0;
	} else {
		g_lastVideoDevice = ComboBox_GetCurSel(hDeviceList);
		// Don't let it be unselected
		if(g_lastVideoDevice < 0) g_lastVideoDevice = 0;
	}
	HRESULT hr = UpdateDeviceList(hDlg);
	if (FAILED(hr)) {
		ShowMessage(hr, L"Failed to reset the device list");
	}
	ComboBox_SetCurSel(hDeviceList,
		g_devices.IsAudio() ? g_lastAudioDevice : g_lastVideoDevice);
}


//-----------------------------------------------------------------------------
// UpdateUI
//
// Updates the dialog UI for the current state.
//-----------------------------------------------------------------------------

void UpdateUI(HWND hDlg) {
	BOOL bEnable = (g_devices.Count() > 0);     // Are there any capture devices?
	BOOL bCapturing = (g_pCapture != NULL);     // Is capture in progress now?

	HWND hButton = GetDlgItem(hDlg, IDC_CAPTURE);

	if (bCapturing) {
		SetWindowText(hButton, L"Stop Capture");
	} else {
		SetWindowText(hButton, L"Start Capture");
	}

	EnableDialogControl(hDlg, IDC_CAPTURE, bCapturing || bEnable);
	EnableDialogControl(hDlg, IDC_DEVICE_LIST, !bCapturing && bEnable);

	// The following cannot be changed while capture is in progress,
	// but are OK to change when there are no capture devices.
	EnableDialogControl(hDlg, IDC_CAPTURE_TOP, !bCapturing);
	EnableDialogControl(hDlg, IDC_CAPTURE_BOTTOM, !bCapturing);
	EnableDialogControl(hDlg, IDC_AUDIO, !bCapturing);
	EnableDialogControl(hDlg, IDC_VIDEO, !bCapturing);
	EnableDialogControl(hDlg, IDC_OUTPUT_FILE, !bCapturing);
}


//-----------------------------------------------------------------------------
// OnDeviceChange
//
// Handles WM_DEVICECHANGE messages.
//-----------------------------------------------------------------------------

void OnDeviceChange(HWND hDlg, WPARAM reason, DEV_BROADCAST_HDR *pHdr)
{
	if (reason == DBT_DEVNODES_CHANGED || reason == DBT_DEVICEARRIVAL) {
		// Check for added/removed devices, regardless of whether
		// the application is capturing at this time.

		UpdateDeviceList(hDlg);
		UpdateUI(hDlg);
	}

	// Now check if the current capture device was lost.
	if (pHdr == NULL) {
		return;
	}
	if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) {
		return;
	}

	HRESULT hr = S_OK;
	BOOL bDeviceLost = FALSE;

	if (g_pCapture && g_pCapture->IsCapturing()) {
		hr = g_pCapture->CheckDeviceLost(pHdr, &bDeviceLost);

		if (FAILED(hr) || bDeviceLost) {
			StopCapture(hDlg);
			MessageBox(hDlg, L"The capture device was removed or lost.",
				L"Lost Device", MB_OK);
		}
	}
}

void EnableDialogControl(HWND hDlg, int nIDDlgItem, BOOL bEnable) {
	HWND hwnd = GetDlgItem(hDlg, nIDDlgItem);

	if (!bEnable &&  hwnd == GetFocus()) {
		// When disabling a control that has focus, set the
		// focus to the next control.
		::SendMessage(GetParent(hwnd), WM_NEXTDLGCTL, 0, FALSE);
	}
	EnableWindow(hwnd, bEnable);
}

