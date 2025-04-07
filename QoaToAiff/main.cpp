#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <workbench/startup.h>
#include <dos/rdargs.h>

#include "main.h"
#include "timing.h"
#include "sysfile.h"

Library *LocaleBase, *TimerBase, *MathIeeeSingBasBase;
Catalog *Cat;

#ifdef DEBUG
#define D(args...) Printf(args)
#else
#define D(args...)
#endif

#define divu16(a,b) ({ \
UWORD _r, _b = (b); \
ULONG _a = (a); \
asm("DIVU.W %2,%0": "=d" (_r): "0" (_a), "dmi" (_b): "cc"); \
_r;})

#define LS()
#define MAKE_ID(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
char FaultBuffer[128];

/* maximum sizes of QOA frames in bytes WITHOUT 8-byte frame header */

#define QOA_FRAME_MONO      2064   /* 16 (LMS) + 256 * 8 (slices)     */
#define QOA_FRAME_STEREO    4128   /* 32 (LMS) + 256 * 2 * 8 (slices) */

ULONG QoaFrameSizes[2] = { QOA_FRAME_MONO, QOA_FRAME_STEREO };

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

extern "C"
{
	void DecodeMonoFrame(ULONG *in, WORD *out, WORD slices);
	void DecodeStereoFrame(ULONG *in, WORD *out, WORD slices);
}

void Problem(STRPTR text)
{
	Printf("%s.\n", text);
}

void IoErrProblem(STRPTR text)
{
	Fault(IoErr(), "", FaultBuffer, 128);
	Printf("%s: %s.\n", text, &FaultBuffer[2]);
}

FLOAT EClockValToFloat(EClockVal *ev)
{
	FLOAT result;

	result = (FLOAT)(ev->ev_lo & 0x7FFFFFFF);
	if (ev->ev_lo & 0x80000000) result += 2147483648.0f;
	result += (FLOAT)ev->ev_hi * 4294967296.0f;
	return result;
}

FLOAT ULongToFloat(ULONG x)
{
	FLOAT y;

	y = (FLOAT)(x & 0x7FFFFFFF);
	if (x & 0x80000000) y += 2147483648.0f;
	return y;
}

static FLOAT fract(FLOAT x) { return x - floorf(x); }

/*-------------------------------------------------------------------------------------------*/

class CallArgs
{
	LONG vals[2];
	RDArgs *args;

	public:

	BOOL ready;

	CallArgs()
	{
		vals[0] = 0;
		vals[1] = 0;
		ready = FALSE;
		if (args = ReadArgs("FROM/A,TO/A", vals, NULL)) ready = TRUE;
		else IoErrProblem("Program arguments");
	}

	~CallArgs()
	{
		if (args) FreeArgs(args);
	}

	STRPTR getString(LONG index) { return (STRPTR)vals[index]; }
};

/*-------------------------------------------------------------------------------------------*/

class AiffOutput : public SysFile
{
	AiffHeader header;
	ULONG audioFrames;
	UWORD audioChannels;

	public:

	BOOL ready;

	AiffOutput(STRPTR filename, ULONG frames, UWORD channels, ULONG samplerate);
	void sampleRateConvert(ULONG rate);
};

AiffOutput::AiffOutput(STRPTR filename, ULONG frames, UWORD channels, ULONG samplerate) :
	SysFile(filename, MODE_NEWFILE)
{
	ULONG audioSize;

	ready = FALSE;
	audioFrames = frames;
	audioChannels = channels;
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
		if (write(&header, sizeof(AiffHeader))) ready = TRUE;
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

/*-------------------------------------------------------------------------------------------*/

class QoaInput : public SysFile
{
	BOOL probeFirstFrame();
	void printAudioInfo();

	public:

	BOOL ready;
	ULONG samples;
	ULONG channels;
	ULONG sampleRate;
	FLOAT playTime;              /* seconds */
	QoaInput(STRPTR filename);
};

QoaInput::QoaInput(STRPTR filename) : SysFile(filename, MODE_OLDFILE)
{
	ULONG header[2];

	ready = FALSE;
	if (!SysFile::ready) return;
	if (!read(header, 8)) return;
	if (header[0] != MAKE_ID('q','o','a','f')) { Problem("Not a QOA file"); return; }
	samples = header[1];
	if (samples == 0) { Problem("Zero samples in QOA file."); return; }
	if (!probeFirstFrame()) return;
	if (channels == 0) { Problem("Zero audio channels specified in QOA file."); return; }
	if (channels > 2) { Problem("QoaToAiff does not handle more than 2 audio channels."); return; }
	if (sampleRate == 0) { Problem("0 Hz sampling rate specified in QOA file."); return; }
	playTime = ULongToFloat(samples) / (FLOAT)sampleRate;
	printAudioInfo();
	ready = TRUE;
}

BOOL QoaInput::probeFirstFrame()
{
	ULONG probe;

	if (!read(&probe, 4)) return FALSE;
	channels = probe >> 24;
	sampleRate = probe & 0x00FFFFFF;
	if (!seek(-4, OFFSET_CURRENT)) return FALSE;
	return TRUE;
}

void QoaInput::printAudioInfo()
{
	FLOAT seconds, ticks;

	ticks = fract(playTime) * 100.0f;
	Printf("QOA stream: %lu %s samples at %lu Hz (%lu.%02lu seconds).\n", samples,
		(channels == 1) ? "mono" : "stereo", sampleRate, (LONG)playTime, (LONG)ticks);
}

/*-------------------------------------------------------------------------------------------*/

class App
{
	QoaInput *inFile;
	AiffOutput *outFile;
	ULONG *inBuf;
	WORD *outBuf;
	StopWatch diskTime;
	StopWatch decodeTime;
	void (*decoder)(ULONG*, WORD*, WORD);
	ULONG convertFrame();

	public:

	BOOL ready;
	App(CallArgs &args);
	~App();
	BOOL convertAudio();
	void reportTimes();
};

App::App(CallArgs &args)
{
	ready = FALSE;
	inFile = new QoaInput(args.getString(0));
	if (!inFile->ready) return;
	outFile = new AiffOutput(args.getString(1), inFile->samples, inFile->channels,
		inFile->sampleRate);
	if (!outFile->ready) return;
	if (inFile->channels == 1) decoder = DecodeMonoFrame;
	else decoder = DecodeStereoFrame;
	ready = TRUE;
}

App::~App()
{
	if (inFile) delete inFile;
	if (outFile) delete outFile;
}


ULONG App::convertFrame()
{
	ULONG header[2];
	UWORD channels;
	ULONG samprate;
	UWORD fsamples;
	UWORD fbytes;
	UWORD slicesPerChannel;
	ULONG expectedFrameSize;

	diskTime.start();
	if (!inFile->read(header, 8)) return 0;
	diskTime.stop();
	channels = header[0] >> 24;
	samprate = header[0] & 0x00FFFFFF;
	fsamples = header[1] >> 16;
	fbytes = header[1] & 0x0000FFFF;
	if (channels != inFile->channels) { Problem("Variable number of channels detected."); return 0; }
	if (samprate != inFile->sampleRate) { Problem("Variable sampling rate detected."); return 0; }
	if (fsamples == 0) { Problem("Zero samples specified in frame."); return 0; }
	if (fsamples > 5120) { Problem("More than 5120 samples in frame specified."); return 0; }
	slicesPerChannel = divu16(fsamples + 19, 20);
	expectedFrameSize = 8 + (8 << channels) + (slicesPerChannel << (channels + 2));
	if (expectedFrameSize != fbytes) { Problem("Expected and specified frame size differs."); return 0; }
	diskTime.start();
	if (!inFile->read(inBuf, fbytes - 8)) return 0;
	diskTime.stop();
	decodeTime.start();
	decoder(inBuf, outBuf, slicesPerChannel);
	decodeTime.stop();
	diskTime.start();
	if (!outFile->write(outBuf, fsamples << channels)) return 0;
	diskTime.stop();
	return fsamples;
}


BOOL App::convertAudio()
{
	ULONG decoded = 0;

	if (outBuf = (WORD*)AllocVec(5120 << inFile->channels, MEMF_ANY))
	{
		if (inBuf = (ULONG*)AllocVec(QoaFrameSizes[inFile->channels - 1], MEMF_ANY))
		{
			ULONG fsamples = 0;

			while ((decoded < inFile->samples) && (fsamples = convertFrame()))
			{
				decoded += fsamples;
				Printf("%9ld/%9ld samples converted.\r", decoded, inFile->samples);

				if (CheckSignal(SIGBREAKF_CTRL_C))
				{
					PutStr("\nConversion aborted.");
					break;
				}
			}

			PutStr("\n");
			reportTimes();
			FreeVec(inBuf);
		}

		FreeVec(outBuf);
	}
}

void App::reportTimes()
{
	FLOAT speed, speedfrac;

	FLOAT diskSeconds = EClockValToFloat(&diskTime.total) / (FLOAT)TimerDevice::eClock;
	FLOAT decodeSeconds = EClockValToFloat(&decodeTime.total) / (FLOAT)TimerDevice::eClock;
	FLOAT diskTicks = fract(diskSeconds) * 100.0f;
	FLOAT decodeTicks = fract(decodeSeconds) * 100.0f;
	Printf("disk I/O time: %ld.%02ld seconds.\ndecoding time: %ld.%02ld seconds.\n", (LONG)diskSeconds, 
		(LONG)diskTicks, (LONG)decodeSeconds, (LONG)decodeTicks);
	speed = inFile->playTime / decodeSeconds;
	speedfrac = fract(speed) * 100.0f;
	Printf("decoding speed to realtime: \xD7%ld.%02ld.\n", (LONG)speed, (LONG)speedfrac);
}

/*-------------------------------------------------------------------------------------------*/

LONG Main(WBStartup *wbmsg)
{
	CallArgs args;
	TimerDevice timer;
	LONG result = RETURN_ERROR;

	/* Locale are optional. */

	if (LocaleBase = OpenLibrary("locale.library", 39))
	{
		Cat = OpenCatalogA(NULL, "QoaToAiff.catalog", NULL);
	}

	if (MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0))
	{
		if (args.ready && timer.ready)
		{
			App app(args);
			EClockVal dummy;

			TimerDevice::eClock = ReadEClock(&dummy);

			if (app.ready)
			{
				if (app.convertAudio()) result = RETURN_OK;
			}
		}

		CloseLibrary(MathIeeeSingBasBase);
	}
	else Problem("Can't open mathieeesingbas.library.\n");

	if (LocaleBase)
	{
		CloseCatalog(Cat);                 /* NULL-safe */
		CloseLibrary(LocaleBase);
	}

	return result;
}