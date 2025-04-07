#include <proto/dos.h>

extern struct Library *DOSBase;


class SysFile
{
	BOOL fileProblem();

	protected:

	BPTR handle;
	STRPTR filename;

	public:

	BOOL ready;
	SysFile(STRPTR path, LONG mode);
	~SysFile();
	LONG read(APTR buffer, LONG bytes);
	BOOL mustRead(APTR buffer, LONG bytes);
	BOOL write(APTR buffer, LONG bytes);
	BOOL seek(LONG offset, LONG mode);
};
