#include "kguithread.h"
#include <wtypesbase.h>

kGUICallThread::kGUICallThread()
{
	m_active = false;
}

kGUICallThread::~kGUICallThread()
{

}

bool kGUICallThread::Start(const char* line, int mode)
{
	HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
	SECURITY_ATTRIBUTES saAttr;
	STARTUPINFO siStartInfo;
	char c;
	DWORD cl;

	// Set the bInheritHandle flag so pipe handles are inherited. 

	m_closing = false;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Get the handle to the current STDOUT. 

	// Create a pipe for the child process's STDOUT. 
	if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
		return(false);

	// Ensure that the read handle to the child process's pipe for STDOUT is not inherited.
	SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

	// Create a pipe for the child process's STDIN. 
	if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0))
		return(false);

	// Ensure that the write handle to the child process's pipe for STDIN is not inherited. 
	SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

	// Now create the child process. 

	// Set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory(&m_pi, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = hChildStdoutWr;
	siStartInfo.hStdOutput = hChildStdoutWr;
	siStartInfo.hStdInput = hChildStdinRd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 
	if (!CreateProcess(NULL,
		(LPSTR)line,     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,   // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&m_pi))  // receives PROCESS_INFORMATION 
		return(false);


	switch (mode)
	{
	case CALLTHREAD_READ:
		m_string = "";
		if (!CloseHandle(hChildStdoutWr))
			return(false);

		// Read output from the child process, and write to parent's STDOUT. 
		m_active = true;
		while (ReadFile(hChildStdoutRd, &c, sizeof(c), &cl, NULL))
		{
			if (!cl)
				break;
			m_string.append(1u, c);
		}
		// Wait until child process exits.
		m_active = false;
		WaitForSingleObject(m_pi.hProcess, INFINITE);
		CloseHandle(m_pi.hProcess);
		CloseHandle(m_pi.hThread);
		return(true);
		break;
	case CALLTHREAD_WRITE:
		return(true);
		break;
	}
	return(false);	/* should never get here! */
}

void kGUICallThread::Stop(void)
{
	if ((m_closing == false) && (m_active == true))
	{
		AttachConsole(m_pi.dwProcessId);
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_pi.dwProcessId);
		GenerateConsoleCtrlEvent(CTRL_C_EVENT, m_pi.dwProcessId);
		FreeConsole();
		m_closing = true;
	}
}

