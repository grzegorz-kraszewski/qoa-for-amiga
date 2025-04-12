#ifndef QOATOAIFF_SYSFILE_H
#define QOATOAIFF_SYSFILE_H

#include "main.h"

#include <proto/dos.h>


class SysFile
{
	BPTR handle;
	STRPTR filename;

	public:

	BOOL ready;
	SysFile(STRPTR path, LONG mode);
	~SysFile();
	LONG read(APTR buffer, LONG bytes) { return Read(handle, buffer, bytes); }
	LONG write(APTR buffer, LONG bytes) { return Write(handle, buffer, bytes); }
	LONG seek(LONG offset, LONG mode) { return Seek(handle, offset, mode); }
	LONG size();
	BOOL FileProblem();
};

#endif    // QOATOAIFF_SYSFILE_H
