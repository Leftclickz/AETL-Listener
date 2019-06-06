#ifndef __KGUITHREAD__
#define __KGUITHREAD__

#include <windows.h>
#include <string>


/* call a program and send input or grab it's output */

enum
{
CALLTHREAD_READ,
CALLTHREAD_WRITE
};

class kGUICallThread
{
public:
	kGUICallThread();
	~kGUICallThread();
	bool Start(const char *line,int mode);
	void Stop(void);
	void SetString(std::string *s) {m_string = (*s);}
	std::string *GetString(void) {return &m_string;}
	volatile bool GetActive(void) {return m_active;}
private:
	std::string m_string;
	PROCESS_INFORMATION m_pi; 

	volatile bool m_active:1;
	volatile bool m_closing:1;
};

#endif

