#include "sysfile.h"


BOOL SysFile::fileProblem()
{
	static char faultBuffer[128];

	Fault(IoErr(), "", faultBuffer, 128);
	PutStr("File");
	Printf(" \"%s\": %s.\n", filename, &faultBuffer[2]);
	return FALSE;
}


SysFile::SysFile(STRPTR path, LONG mode)
{
	filename = path;
	ready = FALSE;

	if (handle = Open(filename, mode)) ready = TRUE;
	else fileProblem();
}


SysFile::~SysFile()
{
	if (handle)
	{
		if (!(Close(handle))) fileProblem();
	}
}


BOOL SysFile::read(APTR buffer, LONG bytes)
{
	if (FRead(handle, buffer, bytes, 1) == 1) return TRUE;
	else return fileProblem();
}


BOOL SysFile::write(APTR buffer, LONG bytes)
{
	if (FWrite(handle, buffer, bytes, 1) == 1) return TRUE;
	else return fileProblem();
}


BOOL SysFile::seek(LONG offset, LONG mode)
{
	if (Seek(handle, offset, mode) >= 0) return TRUE;
	else return fileProblem();
}
