// IFEONpp.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "IFEONpp.h"

LPTSTR FindNextArg(LPTSTR lpCmdLine)
{
	///解析命令行
	bool bMarks(false);

	for (int ix = 0; lpCmdLine[ix] != 0; ++ix)
	{
		switch (lpCmdLine[ix])
		{
		case '"':
			bMarks = !bMarks;
			break;
		case ' ':
			if (!bMarks)
			{
				return lpCmdLine + ix;
			}
			break;
		default:
			break;
		}
	}

	return lpCmdLine;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	TCHAR szPath[MAX_PATH];
	GetModuleFileName(NULL, szPath, sizeof(szPath));

	if (lpCmdLine[0])
	{
		///打开notepad++
		LPTSTR lpCmdTxt = FindNextArg(lpCmdLine);

		STARTUPINFO si = { 0 };
		PROCESS_INFORMATION pi;
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = TRUE;

		(_tcsrchr(szPath, _T('\\')))[1] = 0;
		_tcscat(szPath, TEXT("notepad++.exe"));

		BOOL bRet = ::CreateProcess(
			szPath,
			lpCmdTxt,   
			NULL,
			NULL,
			FALSE,
			CREATE_NEW_CONSOLE | CREATE_SHARED_WOW_VDM | CREATE_UNICODE_ENVIRONMENT,
			NULL,
			NULL,
			&si,
			&pi);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else
	{
		///进行镜像劫持
		LPTSTR lpIFEO = TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options");
		HKEY hKeyIFEO;
		if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpIFEO, 0, KEY_WRITE, &hKeyIFEO))
		{
			LPTSTR lpNotepad = TEXT("notepad.exe");
			HKEY hKeyNotepad;
			if (ERROR_SUCCESS == ::RegOpenKeyEx(hKeyIFEO, lpNotepad, 0, KEY_READ, &hKeyNotepad))
			{
				///删除镜像劫持
				::RegCloseKey(hKeyNotepad);
				::RegDeleteKeyEx(hKeyIFEO, lpNotepad, KEY_WOW64_32KEY, 0);
			}
			else
			{
				///创建镜像劫持
				if (ERROR_SUCCESS == ::RegCreateKeyEx(hKeyIFEO, lpNotepad, 0, REG_NONE,
					REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, NULL, &hKeyNotepad, 0))
				{
					::RegSetValueEx(hKeyNotepad, TEXT("Debugger"), 0, REG_SZ, (const BYTE*)szPath, _tcslen(szPath) * sizeof(TCHAR));
				}
			}
		}
	}


	return 0;
}


