#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <workbench/startup.h>
#include <dos/rdargs.h>

#include "main.h"
#include "errors.h"
#include "timing.h"
#include "qoainput.h"
#include "aiffoutput.h"


Library *LocaleBase, *TimerBase, *MathIeeeSingBasBase, *UtilityBase;
Catalog *Cat;


extern "C"
{
	void DecodeMonoFrame(ULONG *in, WORD *out, WORD slices);
	void DecodeStereoFrame(ULONG *in, WORD *out, WORD slices);
}


const char *ErrorMessages[] = {
	"QOA file too short, 40 bytes is the minimum size",
	"QOA file too big, resulting AIFF will be larger than 2 GB",
	"Not QOA file",
	"Zero samples specified in the QOA header",
	"Zero audio channels specified in the first frame",
	"QoaToAiff does not support multichannel files",
	"0 Hz sampling rate specified in the first frame",
	"QOA file is too short for specified number of samples",
	"QOA file is too long, extra data will be ignored",
	"Input buffer allocation failed, out of memory?",
	"Variable channels stream is not supported",
	"Variable sampling rate is not supported",
	"QOA frame with zero samples specified",
	"QOA frame with more than 5120 samples specified",
	"Specified frame size is different than calculated one",
	"Can't open utility.library v39+",
	"Can't open mathieeesingbas.library",
	"Commandline arguments",
	""
};


BOOL Problem(LONG error)
{
	static char faultbuf[128];
	STRPTR description = "";

	if (error & IOERR)
	{
		LONG doserr = IoErr();

		if (doserr)
		{
			Fault(doserr, "", faultbuf, 128);
			description = &faultbuf[2];
		}
		else if (error & FEOF) description = "unexpected end of file";

		Printf("%s: %s.\n", ErrorMessages[error & 0xFFFF], description);
	}
	else Printf("%s.\n", ErrorMessages[error]);

	return FALSE;
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
		else Problem(E_APP_COMMANDLINE_ARGS | IOERR);
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
	StopWatch diskTime;
	StopWatch decodeTime;
	void (*decoder)(ULONG*, WORD*, WORD);
	ULONG convertFrame(WORD *dest);

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


ULONG App::convertFrame(WORD *destBuffer)
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
	channels = frame[0] >> 24;
	samprate = frame[0] & 0x00FFFFFF;
	fsamples = frame[1] >> 16;
	fbytes = frame[1] & 0x0000FFFF;
	if (channels != inFile->channels) { Problem(E_QOA_VARIABLE_CHANNELS); return 0; }
	if (samprate != inFile->sampleRate) { Problem(E_QOA_VARIABLE_SAMPLING); return 0; }
	if (fsamples == 0) { Problem(E_QOA_ZERO_SAMPLES_FRAME); return 0; }
	if (fsamples > 5120) { Problem(E_QOA_TOO_MANY_SAMPLES); return 0; }
	slicesPerChannel = divu16(fsamples + 19, 20);
	expectedFrameSize = inFile->QoaFrameSize(fsamples, channels);
	if (expectedFrameSize != fbytes) { Problem(E_QOA_WRONG_FRAME_SIZE); return 0; }
	decodeTime.start();
	decoder(&frame[2], destBuffer, slicesPerChannel);
	decodeTime.stop();

/*
	diskTime.start();

	if (outFile->write(outBuf, fsamples << channels) != (fsamples << channels))
	{
		outFile->FileProblem();
		return 0;
	}

	diskTime.stop();
*/

	return fsamples;
}


BOOL App::convertAudio()
{
	WORD *outBuf, *bufPtr;
	BOOL stop = FALSE;
	ULONG decoded = 0;

	if (outBuf = (WORD*)AllocVec((5120 << inFile->channels) * QOA_FRAMES_PER_BUFFER, MEMF_ANY))
	{
		bufPtr = outBuf;

		/* Partial frame which is not the final one is considered an error. */
		
		do
		{
			if (CheckSignal(SIGBREAKF_CTRL_C))
			{
				PutStr("\nConversion aborted.");
				stop = TRUE;
			}
		}
		while (!stop && (decoded < inFile->samples));
		

/*

		ULONG fsamples = 0;

		while ((decoded < inFile->samples) && (fsamples = convertFrame()))
		{
			decoded += fsamples;
			Printf("%9ld/%9ld samples converted.\r", decoded, inFile->samples);


		PutStr("\n");
		reportTimes();
		FreeVec(outBuf);

*/

	}
	else return Problem(E_APP_OUT_OF_MEMORY);
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
		else Problem(E_APP_NO_UTILITY_LIBRARY);

		CloseLibrary(MathIeeeSingBasBase);
	}
	else Problem(E_APP_NO_MATHIEEE_LIBRARY);

	if (LocaleBase)
	{
		CloseCatalog(Cat);                 /* NULL-safe */
		CloseLibrary(LocaleBase);
	}

	return result;
}