#include "main.h"
#include "aiffoutput.h"


AiffOutput::AiffOutput(STRPTR filename, ULONG frames, UWORD channels, ULONG samplerate) :
	SysFile(filename, MODE_NEWFILE)
{
	ULONG audioSize;

	ready = FALSE;
	audioSize = frames << channels;

	if (SysFile::ready)
	{
		header.formID = MAKE_ID('F','O','R','M');
		header.formSize = audioSize + 46;
		header.type = MAKE_ID('A','I','F','F');
		header.commID = MAKE_ID('C','O','M','M');
		header.commSize = 18;
		header.commChannels = channels;
		header.commSamples = frames;
		header.commSampBits = 16;
		sampleRateConvert(samplerate);
		header.ssndID = MAKE_ID('S','S','N','D');
		header.ssndSize = audioSize + 8;
		header.ssndPad0 = 0;
		header.ssndPad1 = 0;
		if (write(&header, sizeof(AiffHeader)) == sizeof(AiffHeader)) ready = TRUE;
		else FileProblem();
	}
}

void AiffOutput::sampleRateConvert(ULONG rate)
{
	UWORD mant = 16414;

	while ((rate & 0x80000000) == 0)
	{
		mant--;
		rate <<= 1;
	}

	header.commRateExp = mant;
	header.commRateMant0 = rate;
	header.commRateMant1 = 0;
}
