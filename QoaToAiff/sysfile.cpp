#include "sysfile.h"
#include "errors.h"


BOOL SysFile::FileProblem()
{
	static char faultBuffer[128];
	char *description;
	LONG error = IoErr();

	Printf("File \"%s\"", filename);
	return Problem(E_APP_FILE | IOERR | FEOF);
}


SysFile::SysFile(STRPTR path, LONG mode)
{
	filename = path;
	ready = FALSE;

	D("SysFile $%08lx, opening \"%s\" for %s.\n", this, filename, (mode == MODE_OLDFILE) ? "reading" : "writing");

	if (handle = Open(filename, mode))
	{
		ready = TRUE;
		D("SysFile $%08lx (\"%s\") ready, handle is $%08lx.\n", this, filename, handle);
	}
	else FileProblem();
}


SysFile::~SysFile()
{
	if (handle)
	{
		if (!(Close(handle))) FileProblem();
	}

	D("SysFile $%08lx deleted, handle $%08lx closed.\n", this, handle);
}


LONG SysFile::size()
{
	LONG result;

	if (Seek(handle, 0, OFFSET_END) < 0) { FileProblem(); return 0; }
	result = Seek(handle, 0, OFFSET_BEGINNING);
	if (result < 0) { FileProblem(); return 0; }
	return result;
}