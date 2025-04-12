#include "main.h"
#include "errors.h"
#include "qoainput.h"

#include <proto/exec.h>


QoaInput::QoaInput(STRPTR filename) : SysFile(filename, MODE_OLDFILE)
{
	ULONG header[2];
	LONG realFileSize, expectedFileSize;

	ready = FALSE;

	D("Construction of QoaInput $%08lx starts.\n");

	if (SysFile::ready)
	{
		LONG realFileSize;

		if ((realFileSize = size()) >= 40)
		{
			D("File ready, real size is %ld bytes.\n", realFileSize);

			if (HeaderCheck() && FirstFrameCheck() && FileSizeCheck(realFileSize))
			{
				if (buffer = new QoaBuffer(this, fullFrameSize))
				{
					if (buffer->ready)
					{
						playTime = ULongToFloat(samples) / (FLOAT)sampleRate;
						PrintAudioInfo();
						ready = TRUE;
						D("QoaInput $%08lx (\"%s\") ready.\n", this, filename);
						return;
					}
				}
				else Problem(E_QOA_NO_BUFFER);
			}
		}
		else Problem(E_QOA_LESS_THAN_40_BYTES);
	}
}


QoaInput::~QoaInput()
{
	if (buffer) delete buffer;
}


LONG QoaInput::QoaFrameSize(LONG samples, LONG channels)
{
	return (1 + ((2 + divu16(samples + 19, 20)) << (channels - 1))) << 3;
}


LONG QoaInput::ExpectedFileSize()
{
	return 8 + (samples / 5120) * fullFrameSize + QoaFrameSize(samples % 5120, channels);
}


BOOL QoaInput::FileSizeCheck(LONG realFileSize)
{
	LONG expectedFileSize;

	expectedFileSize = ExpectedFileSize();

	if (realFileSize >= expectedFileSize)
	{
		if (realFileSize == expectedFileSize) return TRUE;
		else return Problem(E_QOA_FILE_EXTRA_DATA);
	}
	else return Problem(E_QOA_FILE_TOO_SHORT);
}


BOOL QoaInput::HeaderCheck()
{
	BOOL result = TRUE;
	ULONG header[2];

	if (read(header, 8) == 8)
	{
		if (header[0] == MAKE_ID('q','o','a','f'))
		{
			samples = header[1];
			if (samples == 0) result = Problem(E_QOA_ZERO_SAMPLES);
		}
		else result = Problem(E_QOA_NO_QOAF_MARKER);
	}
	else result = FALSE;

	return result;
}


BOOL QoaInput::FirstFrameCheck()
{
	if (ProbeFirstFrame())
	{
		if (channels > 0)
		{
			if (channels <= 2)
			{
				if (samples <= (2147483594 >> channels))
				{
					if (sampleRate > 0)
					{
						return TRUE;
					}
					else return Problem(E_QOA_ZERO_SAMPLING_RATE);
				}
				else return Problem(E_QOA_FILE_TOO_BIG);
			}
			else return Problem(E_QOA_TOO_MANY_CHANNELS);
		}
		else return Problem(E_QOA_ZERO_CHANNELS);
	}
	else return FALSE;
}


BOOL QoaInput::Problem(LONG error)
{
	Printf("QOA problem %ld.\n", error);
	return FALSE;
}


BOOL QoaInput::ProbeFirstFrame()
{
	ULONG probe;

	if (read(&probe, 4) == 4)
	{
		channels = probe >> 24;
		sampleRate = probe & 0x00FFFFFF;
		fullFrameSize = QoaFrameSize(5120, channels);
		if (seek(-4, OFFSET_CURRENT) >= 0) return TRUE;
		else return FileProblem();
	}
	else return FileProblem();
}


void QoaInput::PrintAudioInfo()
{
	FLOAT seconds, ticks;

	ticks = fract(playTime) * 100.0f;
	Printf("QOA stream: %lu %s samples at %lu Hz (%lu.%02lu seconds).\n", samples,
		(channels == 1) ? "mono" : "stereo", sampleRate, (LONG)playTime, (LONG)ticks);
}

//=============================================================================================

QoaBuffer::QoaBuffer(SysFile *source, LONG framesize)
{
	ready = FALSE;
	frameSize = framesize;
	dataSource = source;
	frameIndex = QOA_FRAMES_PER_BUFFER;    /* exhausted, so Fill() at first GetFrame() */

	D("QoaBuffer $%08lx constructor, frame size = %ld.\n", this, frameSize);

	if (buffer = (UBYTE*)AllocMem(frameSize * QOA_FRAMES_PER_BUFFER, MEMF_ANY))
	{
		ready = TRUE;
		D("QoaBuffer $%08lx ready, %ld bytes of buffer at $%08lx.\n", this, framesize *
			QOA_FRAMES_PER_BUFFER, buffer);
	}
}


QoaBuffer::~QoaBuffer()
{
	if (buffer) FreeMem(buffer, frameSize * QOA_FRAMES_PER_BUFFER);
	D("QoaBuffer $%08lx deleted, buffer at $%08lx freed.\n", this, buffer);
}


ULONG* QoaBuffer::GetFrame()
{
	ULONG *result = NULL;

	if ((frameIndex < QOA_FRAMES_PER_BUFFER) || Fill())
	{
		result = (ULONG*)dataPtr;
		D("QoaBuffer $%08lx, getting frame %ld ($%08lx).\n", this, frameIndex, dataPtr);
		dataPtr += frameSize;
		frameIndex++;
	}
	
	return result;
}


BOOL QoaBuffer::Fill()
{
	if (dataSource->read(buffer, frameSize * QOA_FRAMES_PER_BUFFER) > 0)
	{
		frameIndex = 0;
		dataPtr = buffer;
		D("QoaBuffer $%08lx refilled.\n", this);
	}
	else dataSource->FileProblem();
}