#ifndef QOATOAIFF_SYSFILE_H
#define QOATOAIFF_SYSFILE_H

#include "main.h"

#include <proto/dos.h>


class SysFile
{
	BPTR handle;
	STRPTR filename;

	protected:

	BOOL fileProblem();

	public:

	BOOL ready;
	SysFile(STRPTR path, LONG mode);
	~SysFile();
	LONG read(APTR buffer, LONG bytes) { return Read(handle, buffer, bytes); }
	LONG write(APTR buffer, LONG bytes) { return Write(handle, buffer, bytes); }
	LONG seek(LONG offset, LONG mode) { return Seek(handle, mode, offset); }
	LONG size();
};

#endif    // QOATOAIFF_SYSFILE_H
