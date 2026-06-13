#include "StdAfx.h"
#include "crashtool.h"

#include <strsafe.h>
#include <DbgHelp.h>
#include <tchar.h>
#include "MessageDlg.h"
#include "Common.h"
#include "TrafficMonitor.h"
#include <sstream>

#pragma comment(lib, "Dbghelp.lib")

class CCrashReport
{
public:
    CCrashReport()
    {
        GetAppPath();
    }
    ~CCrashReport() {}
public:
    // 生成MiniDump文件
    void CreateMiniDump(EXCEPTION_POINTERS* pEP)
    {
        SYSTEMTIME stLocalTime;
        ::GetLocalTime(&stLocalTime);
        TCHAR szDumpFile[MAX_PATH] = {0};
        ::StringCchPrintf(szDumpFile, _countof(szDumpFile), TEXT("%s%04d%02d%02d%02d%02d%02d_%s.dmp"), m_szDumpFilePath,
                          stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, m_szModuleFileName);

        // 安全性改进：
        // 1. FILE_SHARE_READ only（去掉 FILE_SHARE_WRITE），避免其他进程在写入期间并发写同一文件
        // 2. CREATE_NEW 拒绝覆盖已存在文件（CREATE_ALWAYS 会覆盖，配合可预测文件名存在符号链接劫持风险）
        HANDLE hDumpFile;
        hDumpFile = ::CreateFile(szDumpFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW, 0, 0);
        if (INVALID_HANDLE_VALUE == hDumpFile)
        {
            // CREATE_NEW 失败（文件已存在）时，尝试加毫秒后缀避免冲突与覆盖
            ::StringCchPrintf(szDumpFile, _countof(szDumpFile), TEXT("%s%04d%02d%02d%02d%02d%02d%03d_%s.dmp"), m_szDumpFilePath,
                              stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, stLocalTime.wMilliseconds, m_szModuleFileName);
            hDumpFile = ::CreateFile(szDumpFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW, 0, 0);
            if (INVALID_HANDLE_VALUE == hDumpFile)
            {
                return;
            }
        }

		m_dumpFile = szDumpFile;

        MINIDUMP_EXCEPTION_INFORMATION ExpParam;
        ExpParam.ThreadId = ::GetCurrentThreadId();
        ExpParam.ExceptionPointers = pEP;
        ExpParam.ClientPointers = TRUE;

        // 生成minidump文件
        BOOL bResult = ::MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &ExpParam, NULL, NULL);
        ::CloseHandle(hDumpFile);
    }

    //根据地址获取模块路径
    std::wstring GetModulePath(DWORD64 address)
    {
        // 获取模块信息
        HMODULE hModule = NULL;
        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)address, &hModule)) 
        {
            TCHAR moduleName[MAX_PATH];
            if (GetModuleFileName(hModule, moduleName, MAX_PATH))
            {
                return moduleName;
            }
        }
        return L"Unknown Module";
    }

    //获取崩溃堆栈信息
    std::wstring GetStackTrace(EXCEPTION_POINTERS* pExceptionInfo)
    {
        std::wstringstream stream;

        // 初始化符号处理
        if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
        {
            stream << L"Failed to initialize symbol handler.\r\n";
            return stream.str();
        }

        // RAII 确保 SymCleanup 被调用
        struct SymCleanupHelper {
            ~SymCleanupHelper() { SymCleanup(GetCurrentProcess()); }
        } cleanupHelper;

        STACKFRAME64 stackFrame = {};
        CONTEXT context = *pExceptionInfo->ContextRecord;

        // 初始化堆栈帧
#if defined _M_IX86
        DWORD machineType = IMAGE_FILE_MACHINE_I386;
        stackFrame.AddrPC.Offset = context.Eip;    // x86 使用 EIP
        stackFrame.AddrFrame.Offset = context.Ebp; // x86 使用 EBP
        stackFrame.AddrStack.Offset = context.Esp; // x86 使用 ESP
//#elif defined _M_ARM64EC
//        DWORD machineType = IMAGE_FILE_MACHINE_ARM64;
//        stackFrame.AddrPC.Offset = context.Pc;     // ARM64 使用 PC
//        stackFrame.AddrFrame.Offset = context.Fp;  // ARM64 使用 FP
//        stackFrame.AddrStack.Offset = context.Sp;  // ARM64 使用 SP
#else
        DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
        stackFrame.AddrPC.Offset = context.Rip;    // x64 使用 RIP
        stackFrame.AddrFrame.Offset = context.Rbp; // x64 使用 RBP
        stackFrame.AddrStack.Offset = context.Rsp; // x64 使用 RSP
#endif
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;

        // 遍历堆栈帧
        while (StackWalk64(machineType, GetCurrentProcess(), GetCurrentThread(), &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            if (stackFrame.AddrPC.Offset == 0) break;

            // 获取符号信息
            BYTE symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;

            stream << L"--------------------------------------\r\n";

            DWORD64 displacement = 0;
            if (SymFromAddr(GetCurrentProcess(), stackFrame.AddrPC.Offset, &displacement, pSymbol)) {
                stream << L"Function: " << CCommon::AsciiToUnicode(pSymbol->Name) << L" (Displacement: " << displacement << L")\r\n";
            }
            else {
                stream << L"Unknown Function at address: " << (void*)stackFrame.AddrPC.Offset << L"\r\n";
            }

            std::wstring modulePath = GetModulePath(stackFrame.AddrPC.Offset);
            if (!modulePath.empty())
                stream << L"Module Path: " << modulePath << L"\r\n";

            // 获取源代码行信息
            IMAGEHLP_LINE64 line = {};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(GetCurrentProcess(), stackFrame.AddrPC.Offset, &lineDisplacement, &line)) {
                stream << L"File: " << CCommon::AsciiToUnicode(line.FileName) << L" (Line: " << line.LineNumber << L")\r\n";
            }
        }
        return stream.str();
    }

	void ShowCrashInfo(EXCEPTION_POINTERS* pEP)
	{
		CString info = CCommon::LoadTextFormat(IDS_CRASH_INFO, { m_dumpFile });
		info += _T("\r\n");
        //在崩溃信息中调用堆栈
        std::wstring stack_trace = GetStackTrace(pEP);
        if (!stack_trace.empty())
        {
            info += _T("Stack trace:\r\n");
            info += stack_trace.c_str();
            info += _T("\r\n");
        }
		info += theApp.GetSystemInfoString();
        //写入日志
        CString crash_log = info;
        crash_log.Replace(_T("\r\n"), _T("\n"));
        crash_log.Replace(_T("\n\n"), _T("\n"));
        CCommon::WriteLog(crash_log.GetString(), theApp.m_log_path.c_str());
        //显示崩溃对话框
        CMessageDlg dlg;
        dlg.SetWindowTitle(APP_NAME);
        dlg.SetInfoText(CCommon::LoadText(IDS_ERROR_MESSAGE));
        dlg.SetMessageText(info);
        dlg.SetStandarnMessageIcon(CMessageDlg::SI_ERROR);
        dlg.DoModal();
	}

private:
    void GetAppPath()
    {
        ZeroMemory(m_szModuleFileName, MAX_PATH);
        TCHAR szExePath[MAX_PATH] = {0};
        ::GetModuleFileName(NULL, szExePath, _countof(szExePath));
        for (int nIndex = (int)_tcslen(szExePath); nIndex >= 0; --nIndex)
        {
            if (szExePath[nIndex] == TEXT('\\'))
            {
                ::memmove(m_szModuleFileName, szExePath + nIndex + 1, (int)_tcslen(szExePath));
                szExePath[nIndex + 1] = 0;  // 截断为 exe 所在目录
                break;
            }
        }
        // 优先使用 %TEMP%（用户可写、隔离）。GetTempPath 失败时回退到 exe 同目录，
        // 而非 C:\（C:\ 根目录普通用户无写权限，管理员运行时会污染系统分区且全局可读）。
        ZeroMemory(m_szDumpFilePath, MAX_PATH);
        if (!::GetTempPath(MAX_PATH, m_szDumpFilePath))
        {
            // 回退到 exe 同目录
            StringCchCopy(m_szDumpFilePath, _countof(m_szDumpFilePath), szExePath);
        }
    }
private:
    wchar_t m_szDumpFilePath[MAX_PATH];
    wchar_t m_szModuleFileName[MAX_PATH];

	CString m_dumpFile;
};

namespace CRASHREPORT
{
    // 独立函数：把带析构的 CCrashReport 对象限制在此函数栈帧内。
    // 这样外层 __UnhandledExceptionFilter 的 __try 块不持有需要展开的 C++ 对象，
    // 避免 MSVC C2712（"不能在需要对象展开的函数中使用 __try"）。
    static void DoCreateMiniDump(PEXCEPTION_POINTERS pEP)
    {
        CCrashReport cr;
        cr.CreateMiniDump(pEP);
    }

    static LONG WINAPI __UnhandledExceptionFilter(PEXCEPTION_POINTERS pEP)
    {
        ::SetErrorMode(0); //使用默认的
        // 崩溃路径必须做最少的事：进程已处于未定义状态（堆可能已损坏），任何依赖堆/锁/GDI/COM
        // 的操作都可能二次崩溃。原实现在此调用 ShowCrashInfo（内部 StackWalk64+SymInitialize+
        // std::wstringstream 堆分配 + CMessageDlg::DoModal 创建窗口/消息泵），极易在崩溃时再次崩溃。
        // 现在仅写 MiniDump（已含完整堆栈/模块信息，可用 WinDbg 事后分析），然后立即终止进程。
        // 用 __try/__except 兜底，即使 dump 写入本身失败也不会再触发未处理异常。
        __try
        {
            DoCreateMiniDump(pEP);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // dump 写入失败也继续，确保进程能终止
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void StartCrashReport()
    {
        ::SetUnhandledExceptionFilter(__UnhandledExceptionFilter);
    }
}
