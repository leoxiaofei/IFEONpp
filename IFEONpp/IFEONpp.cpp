// IFEONpp.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "IFEONpp.h"
#include <vector>
#include <Shlwapi.h>
#include <shellapi.h>
#include <ShlObj.h>

#pragma comment(lib,"Shlwapi.lib")
#pragma comment(lib,"Version.lib")

LPTSTR FindNextArg(LPTSTR lpCmdLine)
{
	///解析命令行
	bool bMarks(false);
	bool bSpace(false);

	int ix = 0;
	for (; lpCmdLine[ix] != 0; ++ix)
	{
		if (lpCmdLine[ix] == _T(' '))
		{
			if (!bMarks)
			{
				bSpace = true;
			}
		}
		else
		{
			if (lpCmdLine[ix] == _T('"'))
			{
				bMarks = !bMarks;
			}

			if (bSpace)
			{
				break;
			}
		}
	}

	return lpCmdLine + ix;
}

void OldModel(LPTSTR lpCmdLine, TCHAR* szModuleFPath)
{
	TCHAR szPath[MAX_PATH] = { _T('\0') };

	///打开notepad++
	LPTSTR lpCmdTxt = FindNextArg(lpCmdLine);

	if (lpCmdTxt[0] != 0 && lpCmdTxt[0] != _T('"'))
	{
		_stprintf_s(szPath, MAX_PATH, _T("%s \"%s\""), szModuleFPath, lpCmdTxt);
		lpCmdTxt = szPath;
	}
	else
	{
		_stprintf_s(szPath, MAX_PATH, _T("%s %s"), szModuleFPath, lpCmdTxt);
		lpCmdTxt = szPath;
	}

	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = TRUE;

	BOOL bRet = ::CreateProcess(
		NULL,
		lpCmdTxt,
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE | CREATE_SHARED_WOW_VDM | CREATE_UNICODE_ENVIRONMENT,
		NULL,
		NULL,
		&si,
		&pi);

	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

///查询notepad++版本大于7.5.9，使用新方法。
bool CheckNewModel(TCHAR* szModuleFPath)
{
	bool bRet(false);

	DWORD dwSize = GetFileVersionInfoSize(szModuleFPath, NULL);
	if (dwSize > 0)
	{
		std::vector<char> vecData(dwSize, 0);
		if (GetFileVersionInfo(szModuleFPath, 0, dwSize, vecData.data()))
		{
			VS_FIXEDFILEINFO *vsInfo;
			unsigned int uLen;
			if (VerQueryValue(vecData.data(), _T("\\"), (LPVOID*)&vsInfo, &uLen))
			{
				int64_t nVerCur = vsInfo->dwFileVersionMS;
				nVerCur <<= 32;
				nVerCur |= vsInfo->dwFileVersionLS;

				int64_t nVerOld = ((7 << 16) | 5);
				nVerOld <<= 32;
				nVerOld |= ((9 << 16) | 0);

				bRet = nVerCur >= nVerOld;
			}
		}
	}

	return bRet;
}

bool CheckAdmin(TCHAR* szNpppPath)
{
	bool bRet(true);

	if (!IsUserAnAdmin())
	{
		///权限不够，提权尝试一下。
		GetModuleFileName(NULL, szNpppPath, MAX_PATH);

		SHELLEXECUTEINFO shExecInfo = { 0 };
		shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shExecInfo.hwnd = NULL;
		shExecInfo.lpVerb = _T("runas");
		shExecInfo.lpFile = szNpppPath;
		shExecInfo.lpParameters = L"";
		shExecInfo.lpDirectory = NULL;
		shExecInfo.nShow = SW_SHOW;
		shExecInfo.hInstApp = NULL;

		ShellExecuteEx(&shExecInfo);

		CloseHandle(shExecInfo.hProcess);

		bRet = false;
	}

	return bRet;
}

void Setup(TCHAR* szNpppPath)
{
	///进行镜像劫持
	LPTSTR lpIFEO = TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options");
	HKEY hKeyIFEO;
	if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpIFEO, 0, KEY_WRITE, &hKeyIFEO))
	{
		bool bHasDebugger = false;

		LPTSTR lpNotepad = TEXT("notepad.exe");
		HKEY hKeyNotepad = nullptr;
		if (ERROR_SUCCESS == ::RegOpenKeyEx(hKeyIFEO, lpNotepad, 0, KEY_WRITE | KEY_READ, &hKeyNotepad))
		{
			LONG lResult = ::RegQueryValueEx(hKeyNotepad, TEXT("Debugger"), NULL, NULL, NULL, NULL);
			if (lResult == ERROR_SUCCESS)
			{
				bHasDebugger = true;
			}
		}

		if(bHasDebugger)
		{
			LONG lResult = ::RegQueryValueEx(hKeyNotepad, TEXT("UseFilter"), NULL, NULL, NULL, NULL);
			if (lResult == ERROR_SUCCESS)
			{
				DWORD dwUseFilter = 1;
				::RegSetValueEx(hKeyNotepad, TEXT("UseFilter"), 0,
					REG_DWORD, (const BYTE*)&dwUseFilter, sizeof(dwUseFilter));
			}

			::RegDeleteValue(hKeyNotepad, TEXT("Debugger"));
			///删除镜像劫持
			::RegCloseKey(hKeyNotepad);
			::RegDeleteKeyEx(hKeyIFEO, lpNotepad, KEY_WOW64_32KEY, 0);

			MessageBox(0, _T("取消关联成功。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
		}
		else
		{
			if (CheckNewModel(szNpppPath))
			{
				///"C:\Program Files\Notepad++\notepad++.exe" -notepadStyleCmdline -z
				_tcscat_s(szNpppPath, MAX_PATH, _T("\" -notepadStyleCmdline -z"));
			}
			else
			{
				GetModuleFileName(NULL, szNpppPath, MAX_PATH);
				_tcscat_s(szNpppPath, MAX_PATH, _T("\""));
			}

			szNpppPath = szNpppPath - 1;

			if (hKeyNotepad || ERROR_SUCCESS == ::RegCreateKeyEx(hKeyIFEO, lpNotepad, 0, REG_NONE,
				REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, NULL, &hKeyNotepad, 0))
			{
				::RegSetValueEx(hKeyNotepad, TEXT("Debugger"), 0,
					REG_SZ, (const BYTE*)szNpppPath, _tcslen(szNpppPath) * sizeof(TCHAR));

				DWORD dwUseFilter = 0;
				::RegSetValueEx(hKeyNotepad, TEXT("UseFilter"), 0,
					REG_DWORD, (const BYTE*)&dwUseFilter, sizeof(dwUseFilter));

				::RegCloseKey(hKeyNotepad);

				MessageBox(0, _T("关联成功。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
			}
			else
			{
				MessageBox(0, _T("创建<notepad.exe>键值失败，请检查权限。"), _T("错误"), MB_OK | MB_ICONERROR);
			}
		}
	}
	else
	{
		MessageBox(0, _T("对注册表<Image File Execution Options>键值没有写入权限，请检查权限。"), _T("错误"), MB_OK | MB_ICONERROR);
	}
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	do 
	{
		TCHAR szPath[MAX_PATH + 1] = { _T('\0') };
		szPath[0] = _T('\"');

		TCHAR* szNpppPath = szPath + 1;
		GetModuleFileName(NULL, szNpppPath, MAX_PATH);

		TCHAR* strDst = (_tcsrchr(szNpppPath, _T('-')));
		strDst = strDst != nullptr ? strDst + 1 : TEXT("notepad++.exe");

		(_tcsrchr(szNpppPath, _T('\\')))[1] = 0;
		_tcscat_s(szNpppPath, MAX_PATH, strDst);

		///检查Notepad++是否存在
		if (!PathFileExists(szNpppPath))
		{
			MessageBox(0, _T("没有找到Notepad++，\r\n请将\"IFEONpp.exe\"放到Notepad++安装目录后再运行。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
			break;
		}

		///如果有参数，说明是劫持调用。
		if (lpCmdLine[0])
		{
			///新版Notepad++直接映射到notepad++.exe上，老版本通过映射到本程序代理调用。
			OldModel(lpCmdLine, szNpppPath);
		}
		///如果没有参数，说明是安装/卸载。
		else
		{
			///安装需要写注册表，需要管理员权限。
			if (!CheckAdmin(szNpppPath))
			{
				break;
			}

			///安装/卸载
			Setup(szNpppPath);
		}

	} while (0);

	return 0;
}


