#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <workbench/startup.h>
#include <dos/rdargs.h>

extern Library *SysBase, *DOSBase;

Library *LocaleBase;
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

void Problem(STRPTR text)
{
	Printf("%s.\n", text);
}

void IoErrProblem(STRPTR text)
{
	Fault(IoErr(), "", FaultBuffer, 128);
	Printf("%s: %s.\n", text, &FaultBuffer[2]);
}

BOOL FileProblem(STRPTR filename)
{
	Fault(IoErr(), "", FaultBuffer, 128);
	PutStr("Plik");
	Printf(" \"%s\": %s.\n", filename, &FaultBuffer[2]);
	return FALSE;
}

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

class SysFile
{
	protected:

	BPTR handle;
	STRPTR filename;

	public:

	BOOL ready;

	SysFile(STRPTR path, LONG mode);
	~SysFile();
	BOOL read(APTR buffer, LONG bytes);
	BOOL write(APTR buffer, LONG bytes);
	BOOL seek(LONG offset, LONG mode);
};


SysFile::SysFile(STRPTR path, LONG mode)
{
	filename = path;
	ready = FALSE;

	if (handle = Open(filename, mode)) ready = TRUE;
	else FileProblem(filename);
}


SysFile::~SysFile()
{
	if (handle)
	{
		if (!(Close(handle))) FileProblem(filename);
	}
}

BOOL SysFile::read(APTR buffer, LONG bytes)
{
	if (FRead(handle, buffer, bytes, 1) == 1) return TRUE;
	else return FileProblem(filename);
}


BOOL SysFile::write(APTR buffer, LONG bytes)
{
	if (FWrite(handle, buffer, bytes, 1) == 1) return TRUE;
	else return FileProblem(filename);
}


BOOL SysFile::seek(LONG offset, LONG mode)
{
	if (Seek(handle, offset, mode) >= 0) return TRUE;
	else return FileProblem(filename);
}

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

	public:

	BOOL ready;
	ULONG samples;
	ULONG channels;
	ULONG sampleRate;
	QoaInput(STRPTR filename);
};

QoaInput::QoaInput(STRPTR filename) : SysFile(filename, MODE_OLDFILE)
{
	ULONG header[2];

	ready = FALSE;
	if (!SysFile::ready) return;
	if (!read(header, 8)) { FileProblem(filename); return; }
	if (header[0] != MAKE_ID('q','o','a','f')) { Problem("Not a QOA file"); return; }
	samples = header[1];
	if (samples == 0) { Problem("Zero samples in QOA file."); return; }
	if (!probeFirstFrame()) return;
	if (channels == 0) { Problem("Zero audio channels specified in QOA file."); return; }
	if (channels > 2) { Problem("QoaToAiff does not handle more than 2 audio channels."); return; }
	if (sampleRate == 0) { Problem("0 Hz sampling rate specified in QOA file."); return; }
	Printf("QOA stream: %lu %s samples at %lu Hz.\n", samples, (channels == 1) ? "mono" : "stereo",
		sampleRate);
	ready = TRUE;
}

BOOL QoaInput::probeFirstFrame()
{
	ULONG probe;

	if (!read(&probe, 4)) return FileProblem(filename);
	channels = probe >> 24;
	sampleRate = probe & 0x00FFFFFF;
	if (!seek(-4, OFFSET_CURRENT)) return FileProblem(filename);
	return TRUE;
}

/*-------------------------------------------------------------------------------------------*/

class App
{
	QoaInput *inFile;
	AiffOutput *outFile;

	public:

	BOOL ready;

	App(CallArgs &args)
	{
		ready = FALSE;
		inFile = new QoaInput(args.getString(0));

		if (inFile->ready)
		{
			outFile = new AiffOutput(args.getString(1), inFile->samples, inFile->channels,
				inFile->sampleRate);
			if (outFile->ready) ready = TRUE;
		}
	}
	
	~App()
	{
		if (inFile) delete inFile;
		if (outFile) delete outFile;
	}
};

/*-------------------------------------------------------------------------------------------*/

LONG Main(WBStartup *wbmsg)
{
	App *app;
	CallArgs args;

	/* Locale are optional. */

	if (LocaleBase = OpenLibrary("locale.library", 39))
	{
		Cat = OpenCatalogA(NULL, "QoaToAiff.catalog", NULL);
	}

	if (args.ready)
	{
		App app(args);

		if (app.ready)
		{
			Printf("");
		}
	}

	if (LocaleBase)
	{
		CloseCatalog(Cat);                 /* NULL-safe */
		CloseLibrary(LocaleBase);
	}

	return 0;
}