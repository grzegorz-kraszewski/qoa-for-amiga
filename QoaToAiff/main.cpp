#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <workbench/startup.h>
#include <dos/rdargs.h>

#include "main.h"
#include "timing.h"
#include "qoainput.h"
#include "aiffoutput.h"

Library *LocaleBase, *TimerBase, *MathIeeeSingBasBase, *UtilityBase;
Catalog *Cat;

#define LS()
char FaultBuffer[128];

LONG QoaFrameSizes[2] = {2064, 4128};


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

FLOAT ULongToFloat(ULONG x)
{
	FLOAT y;

	y = (FLOAT)(x & 0x7FFFFFFF);
	if (x & 0x80000000) y += 2147483648.0f;
	return y;
}

FLOAT EClockValToFloat(EClockVal *ev)
{
	FLOAT result;

	result = ULongToFloat(ev->ev_lo);
	if (ev->ev_hi) result += (FLOAT)ev->ev_hi * 4294967296.0f;
	return result;
}

FLOAT fract(FLOAT x) { return x - floorf(x); }


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
		if (args = ReadArgs("FROM/A,TO/A", vals, NULL))
		{
			ready = TRUE;
			D("CallArgs $%08lx ready, RDArgs at $%08lx.\n", this, args);
		}
		else IoErrProblem("Program arguments");
	}

	~CallArgs()
	{
		if (args) FreeArgs(args);
		D("CallArgs $%08lx deleted, RDArgs at $%08lx freed.\n", this, args);
	}

	STRPTR getString(LONG index) { return (STRPTR)vals[index]; }
};

/*-------------------------------------------------------------------------------------------*/

class App
{
	QoaInput *inFile;
	AiffOutput *outFile;
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
	outFile = new AiffOutput(args.getString(1), inFile->samples, inFile->channels,
		inFile->sampleRate);

	if (inFile->ready && outFile->ready)
	{
		if (inFile->channels == 1) decoder = DecodeMonoFrame;
		else decoder = DecodeStereoFrame;
		ready = TRUE;
		D("App $%08lx ready.\n", this);
	}
}

App::~App()
{
	if (inFile) delete inFile;
	if (outFile) delete outFile;
	D("App $%08lx deleted.\n");
}


ULONG App::convertFrame()
{
	ULONG *frame;
	UWORD channels;
	ULONG samprate;
	UWORD fsamples;
	UWORD fbytes;
	UWORD slicesPerChannel;
	ULONG expectedFrameSize;

	diskTime.start();
	frame = inFile->GetFrame();
	diskTime.stop();
	if (!frame) return 0;
	D("frame: %08lx %08lx %08lx %08lx\n", frame[0], frame[1], frame[2], frame[3]);
	channels = frame[0] >> 24;
	samprate = frame[0] & 0x00FFFFFF;
	fsamples = frame[1] >> 16;
	fbytes = frame[1] & 0x0000FFFF;
	if (channels != inFile->channels) { Problem("Variable number of channels detected."); return 0; }
	if (samprate != inFile->sampleRate) { Problem("Variable sampling rate detected."); return 0; }
	if (fsamples == 0) { Problem("Zero samples specified in frame."); return 0; }
	if (fsamples > 5120) { Problem("More than 5120 samples in frame specified."); return 0; }
	slicesPerChannel = divu16(fsamples + 19, 20);
	expectedFrameSize = 8 + (8 << channels) + (slicesPerChannel << (channels + 2));
	if (expectedFrameSize != fbytes) { Problem("Expected and specified frame size differs."); return 0; }
	decodeTime.start();
	decoder(&frame[2], outBuf, slicesPerChannel);
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
		if (UtilityBase = OpenLibrary("utility.library", 39))
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

			CloseLibrary(UtilityBase);
		}
		else Problem("Can't open utility.library v39+.\n");

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