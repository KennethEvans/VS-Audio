// Header file for utility routines

#pragma once

#include "stdafx.h"

#define PRINT_STRING_SIZE 1024

// Function prototypes
DWORD ansiToUnicode(LPCSTR pszA, LPWSTR *ppszW);
DWORD unicodeToAnsi(LPCWSTR pszW, LPSTR *ppszA);
int errMsg(const TCHAR *format, ...);
int infoMsg(const TCHAR *format, ...);
int debugMsg(const TCHAR *format, ...);
void sysErrMsg(LPTSTR lpHead);
int wsaErrMsg(const TCHAR *format, ...);

// Global string for print routines.  Must be defined somewhere.
extern TCHAR szDebugString[PRINT_STRING_SIZE];
