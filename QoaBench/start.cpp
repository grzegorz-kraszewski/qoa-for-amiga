/*-------------------------------------*/
/* Minimal C++ startup for AmigaOS 3.x */
/* RastPort, 2024                      */
/*-------------------------------------*/

#include <proto/exec.h>
#include <proto/dos.h>

#include <workbench/startup.h>

#ifdef DEBUG
#define D(args...) Printf(args)
#else
#define D(args...)
#endif


Library *SysBase;
Library *DOSBase;
APTR TaskPool = NULL;

extern ULONG Main(WBStartup *wbmsg);


__saveds ULONG Start(void)
{
	Process *myproc = NULL;
	WBStartup *wbmsg = NULL;
	BOOL have_shell = FALSE;
	ULONG result = RETURN_OK;

	SysBase = *(Library**)4L;
	myproc = (Process*)FindTask(NULL);

	if (myproc->pr_CLI) have_shell = TRUE;

	if (!have_shell)
	{
		WaitPort(&myproc->pr_MsgPort);
		wbmsg = (WBStartup*)GetMsg(&myproc->pr_MsgPort);
	}

	result = RETURN_FAIL;

	if (DOSBase = OpenLibrary("dos.library", 39))
	{
		result = Main(wbmsg);
		CloseLibrary(DOSBase);
	}

	if (wbmsg)
	{
		Forbid();
		ReplyMsg(&wbmsg->sm_Message);
	}

	return (ULONG)result;
}


__attribute__((section(".text"))) UBYTE VString[] = "$VER: Unimore 0.1 (11.04.2024)\r\n";


APTR operator new(ULONG size) throw()
{
	APTR p = AllocVec(size, MEMF_ANY);
	D("new(%ld) = $%08lx.\n", size, p);
	return p;
}


APTR operator new[](ULONG size)
{
	return operator new(size);
}


void operator delete(APTR memory)
{
	D("delete($%08lx).\n", memory);
	FreeVec(memory);
}


void operator delete[](APTR memory)
{
	operator delete(memory);
}
