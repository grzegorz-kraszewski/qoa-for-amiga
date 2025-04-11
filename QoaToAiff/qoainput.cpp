#include "main.h"
#include "qoainput.h"

#include <proto/exec.h>


QoaInput::QoaInput(STRPTR filename) : SysFile(filename, MODE_OLDFILE)
{
	ULONG header[2];
	LONG realFileSize, expectedFileSize;

	ready = FALSE;
	if (!SysFile::ready) return;
	if (!read(header, 8)) return;
	if (header[0] != MAKE_ID('q','o','a','f')) { Problem(E_QOA_NO_QOAF_MARKER); return; }
	samples = header[1];
	if (samples == 0) { Problem(E_QOA_ZERO_SAMPLES); return; }
	if (!ProbeFirstFrame()) return;
	if (channels == 0) { Problem(E_QOA_ZERO_CHANNELS); return; }
	if (channels > 2) { Problem(E_QOA_TOO_MANY_CHANNELS); return; }
	if (samples > (268435449 << channels)) { Problem(E_QOA_FILE_TOO_BIG); return; }
	if (sampleRate == 0) { Problem(E_QOA_ZERO_SAMPLING_RATE); return; }
	realFileSize = size();
	expectedFileSize = ExpectedFileSize();
	if (realFileSize < expectedFileSize) { Problem(E_QOA_FILE_TOO_SHORT); return; }
	if (realFileSize > expectedFileSize) Problem(E_QOA_FILE_EXTRA_DATA);
	playTime = ULongToFloat(samples) / (FLOAT)sampleRate;
	PrintAudioInfo();
	buffer = (UBYTE*)AllocMem(frameSize << 4, MEMF_ANY);
	if (!buffer) return;
	ready = TRUE;
}


QoaInput::~QoaInput()
{
	if (buffer) FreeMem(buffer, frameSize << 4);
}


LONG QoaInput::QoaFrameSize(LONG samples, LONG channels)
{
	return (1 + (channels << 1) + ((samples + 19) / 20) << (channels - 1)) << 3;
}


LONG QoaInput::ExpectedFileSize()
{
	return 8 + (samples / 5120) * frameSize + QoaFrameSize(samples % 5120, channels);
}


BOOL QoaInput::Problem(LONG error)
{
	Printf("QOA problem %ld.\n", error);
}


BOOL QoaInput::ProbeFirstFrame()
{
	ULONG probe;

	if (!read(&probe, 4)) return FALSE;
	channels = probe >> 24;
	sampleRate = probe & 0x00FFFFFF;
	frameSize = QoaFrameSize(256, channels);
	if (!seek(-4, OFFSET_CURRENT)) return FALSE;
	return TRUE;
}

void QoaInput::PrintAudioInfo()
{
	FLOAT seconds, ticks;

	ticks = fract(playTime) * 100.0f;
	Printf("QOA stream: %lu %s samples at %lu Hz (%lu.%02lu seconds).\n", samples,
		(channels == 1) ? "mono" : "stereo", sampleRate, (LONG)playTime, (LONG)ticks);
}
