#include "sysfile.h"

#define QOA_FRAMES_PER_BUFFER     16

class QoaBuffer
{
	UBYTE *buffer;
	UBYTE *dataPtr;
	SysFile *dataSource;
	LONG frameSize;
	LONG frameIndex;

	public:

	BOOL ready;
	QoaBuffer(SysFile *source, LONG framesize);
	~QoaBuffer();
	ULONG* GetFrame();
	BOOL Fill();
};


class QoaInput : public SysFile
{
	LONG fullFrameSize;
	QoaBuffer *buffer;
	LONG ExpectedFileSize();
	BOOL FileSizeCheck(LONG realFileSize);
	BOOL HeaderCheck();
	BOOL FirstFrameCheck();
	BOOL ProbeFirstFrame();
	void PrintAudioInfo();
	BOOL Problem(LONG error);

	public:

	BOOL ready;
	ULONG samples;
	ULONG channels;
	ULONG sampleRate;
	FLOAT playTime;              /* seconds */
	QoaInput(STRPTR filename);
	~QoaInput();
	ULONG* GetFrame() { return buffer->GetFrame(); }
	LONG QoaFrameSize(LONG samples, LONG channels);
};
