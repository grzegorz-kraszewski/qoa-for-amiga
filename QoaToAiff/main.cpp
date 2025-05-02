#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <workbench/startup.h>
#include <dos/rdargs.h>
#include <exec/execbase.h>

#include "main.h"
#include "errors.h"
#include "timing.h"
#include "qoainput.h"
#include "aiffoutput.h"
#include "locale.h"
#include "nicenumber.h"


Library *LocaleBase, *TimerBase, *MathIeeeSingBasBase, *UtilityBase;
Catalog *Cat;
Locale *Loc;


extern "C"
{
	void DecodeMonoFrame(ULONG *in, WORD *out, WORD slices);
	void DecodeStereoFrame(ULONG *in, WORD *out, WORD slices);
	void DecodeMonoFrame020(ULONG *in, WORD *out, WORD slices);
	void DecodeStereoFrame020(ULONG *in, WORD *out, WORD slices);
}

                                                                                                    
char *ErrorMessages[E_ENTRY_COUNT] = {
	"QOA file too short, 40 bytes the minimum size",
	"QOA file too big, resulting AIFF will be larger than 2 GB",
	"Not QOA file",
	"Zero samples specified in the QOA header",
	"Zero audio channels specified in the first frame",
	"QoaToAiff does not support multichannel files",
	"0 Hz sampling rate specified in the first frame",
	"QOA file is too short for specified number of samples",
	"QOA file is too long, extra data will be ignored",
	"Variable channels stream is not supported",
	"Variable sampling rate is not supported",
	"QOA frame with zero samples specified",
	"QOA frame with more than 5120 samples specified",
	"Specified frame size is different than calculated one",
	"Unexpected partial QOA frame not at the end of stream",
	"Can't open utility.library v39+",
	"Can't open mathieeesingbas.library",
	"Commandline arguments",
	"Out of memory"
};


void LocalizeErrorMessages()
{
	for (WORD i = 0; i < E_ENTRY_COUNT; i++)
	{
		ErrorMessages[i] = LS(i, ErrorMessages[i]);
	}
}


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
		else if (error & FEOF) description = LS(MSG_UNEXPECTED_END_OF_FILE, "unexpected end of file");

		error &= 0xFFFF;
		if (error < E_ENTRY_COUNT) Printf("%s: %s.\n", ErrorMessages[error], description);
		else Printf("[filename missing]: %s.\n", description);
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

#define OUTPUT_BUFFER_SIZE   5120 * QOA_FRAMES_PER_BUFFER  /* in audio timepoints */


class App
{
	QoaInput *inFile;
	AiffOutput *outFile;
	UBYTE *outBuf, *outPtr;
	LONG outSize;
	StopWatch diskTime;
	StopWatch decodeTime;
	void (*decoder)(ULONG*, WORD*, WORD);
	ULONG ConvertFrame();
	BOOL FlushOutputBuffer();
	BOOL BufferFull() { return (outPtr - outBuf == outSize); }
	BOOL BufferNotEmpty() { return (outPtr > outBuf); }
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
	outBuf = NULL;
	inFile = new QoaInput(args.getString(0));
	outFile = new AiffOutput(args.getString(1), inFile->samples, inFile->channels,
		inFile->sampleRate);

	if (inFile->ready && outFile->ready)
	{
		outSize = OUTPUT_BUFFER_SIZE << inFile->channels;

		if (outBuf = (UBYTE*)AllocVec(outSize, MEMF_ANY))
		{
			outPtr = outBuf;

			if (inFile->channels == 1) decoder = DecodeMonoFrame;
			else decoder = DecodeStereoFrame;

			ready = TRUE;
			D("App $%08lx ready, inFile $%08lx, outFile $%08lx, outBuf[$%08lx, %ld].\n",
				this, inFile, outFile, outBuf, outSize);
		}
		else Problem(E_APP_OUT_OF_MEMORY);
	}
}

App::~App()
{
	if (outBuf) FreeVec(outBuf);
	if (inFile) delete inFile;
	if (outFile) delete outFile;
	D("App $%08lx deleted.\n");
}


ULONG App::ConvertFrame()
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
	decoder(&frame[2], (WORD*)outPtr, slicesPerChannel);
	outPtr += fsamples << inFile->channels;
	decodeTime.stop();
	return fsamples;
}


BOOL App::FlushOutputBuffer()
{
	BOOL result = TRUE;
	LONG bytes = outPtr - outBuf;

	diskTime.start();

	if (outFile->write(outBuf, bytes) == bytes)
	{
		D("output buffer flush, %ld bytes written.\n", bytes);
		outPtr = outBuf;
		result = TRUE;
	}
	else outFile->FileProblem();

	diskTime.stop();
	return result;
}


BOOL App::convertAudio()
{
	BOOL run = TRUE;
	LONG fsamples;
	ULONG decoded = 0;
	NiceInt ni0, ni1;

	do
	{
		fsamples = ConvertFrame();
		D("%ld samples decoded, bufptr advanced to $%08lx.\n", fsamples, outPtr);
		decoded += fsamples;
		if (BufferFull()) run = FlushOutputBuffer();
		Printf(LS(MSG_DECODING_PROGRESS_INDICATOR, "%s / %s samples converted.\r"),
			ni0.GetString(decoded), ni1.GetString(inFile->samples));

		if (fsamples == 0)
		{
			run = FALSE;
		}
		else if ((fsamples < 5120) && (decoded < inFile->samples))
		{
			Problem(E_QOA_UNEXP_PARTIAL_FRAME);
			run = FALSE;
		}

		// keyboard break check

		if (CheckSignal(SIGBREAKF_CTRL_C))
		{
			PutStr(LS(MSG_CONVERSION_ABORTED, "\nConversion aborted."));
			run = FALSE;
		}

	}
	while (run && (decoded < inFile->samples));

	if (BufferNotEmpty()) run = FlushOutputBuffer();
	PutStr("\n");
	if (run) reportTimes();
	return run;
}


void App::reportTimes()
{
	FLOAT speed, diskSeconds, decodeSeconds;
	NiceTime iot, dect;

	diskSeconds = EClockValToFloat(&diskTime.total) / (FLOAT)TimerDevice::eClock;
	decodeSeconds = EClockValToFloat(&decodeTime.total) / (FLOAT)TimerDevice::eClock;

	Printf(LS(MSG_TIMING_REPORT, "disk I/O time: %s seconds.\ndecoding time: %s seconds.\n"),
		iot.GetString(diskSeconds), dect.GetString(decodeSeconds));

	speed = inFile->playTime / decodeSeconds;
	Printf(LS(MSG_DECODING_SPEED_REPORT, "decoding speed to realtime: \xD7%s.\n"),
		iot.GetString(speed));
}


/*-------------------------------------------------------------------------------------------*/

LONG Main(WBStartup *wbmsg)
{
	TimerDevice timer;
	LONG result = RETURN_ERROR;

	PutStr("QoaToAiff " QOATOAIFF_VERSION " (" QOATOAIFF_DATE "), Grzegorz Kraszewski.\n");

	/* Locale are optional. */

	if (LocaleBase = OpenLibrary("locale.library", 39))
	{
		Loc = OpenLocale(NULL);
		Cat = OpenCatalogA(NULL, "QoaToAiff.catalog", NULL);
		if (Cat) LocalizeErrorMessages();
	}

	CallArgs args;

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
		CloseLocale(Loc);                  /* NULL-safe */
		CloseLibrary(LocaleBase);
	}

	return result;
}