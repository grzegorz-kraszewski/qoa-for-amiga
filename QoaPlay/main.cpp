#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <workbench/startup.h>
#include <dos/rdargs.h>
#include <exec/execbase.h>

#include "main.h"
#include "errors.h"
#include "qoainput.h"
#include "locale.h"


Library *LocaleBase, *TimerBase, *MathIeeeSingBasBase, *UtilityBase;
Catalog *Cat;


extern "C"
{
	void DecodeMonoFrame(ULONG *in, WORD *out, WORD slices);
	void DecodeStereoFrame(ULONG *in, WORD *out, WORD slices);
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


/*-------------------------------------------------------------------------------------------*/

class CallArgs
{
	LONG vals[1];
	RDArgs *args;

	public:

	BOOL ready;

	CallArgs()
	{
		vals[0] = 0;
		ready = FALSE;
		if (args = ReadArgs("FILE/A", vals, NULL))
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
	UBYTE *outBuf, *outPtr;
	LONG outSize;
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

	if (inFile->ready)
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

	frame = inFile->GetFrame();
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
	decoder(&frame[2], (WORD*)outPtr, slicesPerChannel);
	outPtr += fsamples << inFile->channels;
	return fsamples;
}


BOOL App::Play()
{
	BOOL run = TRUE;
	LONG fsamples;
	ULONG decoded = 0;

	do
	{
		fsamples = ConvertFrame();
		D("%ld samples decoded, bufptr advanced to $%08lx.\n", fsamples, outPtr);
		decoded += fsamples;
		if (BufferFull()) run = FlushOutputBuffer();
		Printf(LS(MSG_DECODING_PROGRESS_INDICATOR, "%9ld/%9ld samples converted.\r"),
			decoded, inFile->samples);

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


/*-------------------------------------------------------------------------------------------*/

LONG Main(WBStartup *wbmsg)
{
	LONG result = RETURN_ERROR;

	PutStr("QoaPlay " QOAPLAY_VERSION " (" QOAPLAY_DATE "), Grzegorz Kraszewski.\n");

	/* Locale are optional. */

	if (LocaleBase = OpenLibrary("locale.library", 39))
	{
		Cat = OpenCatalogA(NULL, "QoaPlay.catalog", NULL);
		if (Cat) LocalizeErrorMessages();
	}

	CallArgs args;

	if (MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 0))
	{
		if (UtilityBase = OpenLibrary("utility.library", 39))
		{
			if (args.ready)
			{
				App app(args);

				if (app.ready)
				{
					if (app.Play()) result = RETURN_OK;
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