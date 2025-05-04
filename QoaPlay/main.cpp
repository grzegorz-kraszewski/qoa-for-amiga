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
#include "player-paula-mono8.h"


Library *LocaleBase, *UtilityBase;
Catalog *Cat;


extern "C"
{
	void DecodeMonoFrame(ULONG *in, BYTE *out, WORD slices);
	void DecodeStereoFrame(ULONG *in, BYTE *out, WORD slices);
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
	"Out of memory",
	"Can't open audio.device",
	"No free audio channels"
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
	void (*decoder)(ULONG*, BYTE*, WORD);
	ULONG DecodeFrame(BYTE *buffer);
	public:

	BOOL ready;
	App(CallArgs &args);
	~App();
	BOOL Play();
};


App::App(CallArgs &args)
{
	ready = FALSE;
	inFile = new QoaInput(args.getString(0));

	if (inFile->ready)
	{
		if (inFile->channels == 1) decoder = DecodeMonoFrame;
		else decoder = DecodeStereoFrame;

		ready = TRUE;
	}
}

App::~App()
{
	if (inFile) delete inFile;
	D("App $%08lx deleted.\n");
}


ULONG App::DecodeFrame(BYTE *buffer)
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
	decoder(&frame[2], buffer, slicesPerChannel);
	return fsamples;
}


BOOL App::Play()
{
	BOOL go = FALSE;
	PlayerPaula *player;
	LONG played = 0;
	BYTE *buffer;

	if (inFile->channels == 1)
	{
		decoder = DecodeMonoFrame;
		player = new PlayerPaulaMono8(inFile->sampleRate);
	}
	else
	{
		decoder = DecodeStereoFrame;
		player = NULL; // new PlayerPaulaStereo8(inFile->sampleRate);
	}

	if (player->ready)
	{
		go = TRUE;

		while (go && (played < inFile->samples))
		{
			WORD fsamples;

			fsamples = DecodeFrame(player->GetBuffer());

			if (fsamples > 0) go = player->BufferFilled(fsamples);
			else go = FALSE;
		}
	}

	return go;
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

	if (LocaleBase)
	{
		CloseCatalog(Cat);                 /* NULL-safe */
		CloseLibrary(LocaleBase);
	}

	return result;
}