#include "sysfile.h"

/* error codes */

#define E_QOA_LESS_THAN_40_BYTES   0    /* QOA file can't be shorter */
#define E_QOA_FILE_TOO_BIG         1    /* won't fit to 2 GB AIFF file */
#define E_QOA_NO_QOAF_MARKER       2    /* file does not start with 'qoaf' */
#define E_QOA_ZERO_SAMPLES         3    /* zero samples specified in header */
#define E_QOA_ZERO_CHANNELS        4    /* zero audio channels specifed in the first frame */
#define E_QOA_TOO_MANY_CHANNELS    5    /* QoaToAiff only supports 1/2 channels */
#define E_QOA_ZERO_SAMPLING_RATE   6    /* 0 Hz sampling rate specified */
#define E_QOA_FILE_TOO_SHORT       7    /* real file length lower than calculated */
#define E_QOA_FILE_EXTRA_DATA      8    /* real file length higher than calculated */
#define E_QOA_NO_BUFFER            9    /* input buffer allocation failed */

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
	LONG QoaFrameSize(LONG samples, LONG channels);
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
};
