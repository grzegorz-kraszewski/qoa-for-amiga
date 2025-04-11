#include "sysfile.h"


BOOL SysFile::fileProblem()
{
	static char faultBuffer[128];
	char *description;
	LONG error = IoErr();

	Printf("File \"%s\": ", filename);

	if (error)
	{
		Fault(IoErr(), "", faultBuffer, 128);
		description = &faultBuffer[2];
	}
	else description = "unexpected end of file";

	Printf("%s.\n", description);
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


LONG SysFile::size()
{
	LONG result;

	if (Seek(handle, OFFSET_END, 0) < 0) { fileProblem(); return 0; }
	result = Seek(handle, OFFSET_BEGINNING, 0);
	if (result < 0) { fileProblem(); return 0; }
	return result;
}