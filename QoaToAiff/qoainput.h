#include "sysfile.h"

/* error codes */

#define E_QOA_FILE_TOO_BIG        0    /* won't fit to 2 GB AIFF file */
#define E_QOA_NO_QOAF_MARKER      1    /* file does not start with 'qoaf' */
#define E_QOA_ZERO_SAMPLES        2    /* zero samples specified in header */
#define E_QOA_ZERO_CHANNELS       3    /* zero audio channels specifed in the first frame */
#define E_QOA_TOO_MANY_CHANNELS   4    /* QoaToAiff only supports 1/2 channels */
#define E_QOA_ZERO_SAMPLING_RATE  5    /* 0 Hz sampling rate specified */
#define E_QOA_FILE_TOO_SHORT      6    /* real file length lower than calculated */
#define E_QOA_FILE_EXTRA_DATA     7    /* real file length higher than calculated */

class QoaInput : public SysFile
{
	LONG fullFrames;
	LONG frameSize;
	UBYTE *buffer;
	LONG QoaFrameSize(LONG samples, LONG channels);
	LONG ExpectedFileSize();
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
};
