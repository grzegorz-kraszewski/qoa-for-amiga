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

//---------------------------------------------------------------------------------------------
// Wrapper on Read(), which only succeeds if all requested bytes have been read. If Read()
// returns positive value, but less than requested, "unexpected end of file" is reported.

BOOL SysFile::mustRead(APTR buffer, LONG bytes)
{
	if (Read(handle, buffer, bytes) == bytes) return TRUE;
	else return fileProblem();
}

//---------------------------------------------------------------------------------------------
// Wrapper on Read(), which allows for reading less than requested bytes. It returns Read()
// result, but in case of error it is reported.

LONG SysFile::read(APTR buffer, LONG bytes)
{
	LONG result = Read(handle, buffer, bytes);
	
	if (result < 0) fileProblem();
	return result;
}


BOOL SysFile::write(APTR buffer, LONG bytes)
{
	if (FWrite(handle, buffer, bytes, 1) >= 0) return TRUE;
	else return fileProblem();
}


BOOL SysFile::seek(LONG offset, LONG mode)
{
	if (Seek(handle, offset, mode) >= 0) return TRUE;
	else return fileProblem();
}
