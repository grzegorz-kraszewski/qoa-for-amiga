#include "sysfile.h"


struct AiffHeader
{
	ULONG formID;             // 'FORM'
	ULONG formSize;           //  audio size + 46
	ULONG type;               // 'AIFF'
	ULONG commID;             // 'COMM'
	ULONG commSize;           // 18
	UWORD commChannels;       // number of audio channels
	ULONG commSamples;        // total audio frames in file
	UWORD commSampBits;       // bits per sample
	UWORD commRateExp;        // sampling rate 80-bit floating point
	ULONG commRateMant0;
	ULONG commRateMant1;
	ULONG ssndID;              // 'SSND'
	ULONG ssndSize;            // audio size + 8
	ULONG ssndPad0;            // 0
	ULONG ssndPad1;            // 0
};


class AiffOutput : public SysFile
{
	AiffHeader header;
	void sampleRateConvert(ULONG rate);

	public:

	BOOL ready;

	AiffOutput(STRPTR filename, ULONG frames, UWORD channels, ULONG samplerate);
};
