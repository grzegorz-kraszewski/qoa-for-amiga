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

		if (args = ReadArgs("FROM/A,TO/A", vals, NULL))
		{
			D("CallArgs: RDArgs allocated [$%08lx].\n", args);
			ready = TRUE;
		}
		else IoErrProblem("Program arguments");

		D("CallArgs: created at $%08lx.\n", this);  
	} 

	~CallArgs()
	{
		if (args)
		{
			FreeArgs(args);
			D("CallArgs: RDArgs [$%08lx] freed.\n", args);
		}

		D("CallArgs: object $%08lx disposed.\n", this);
	}	

	STRPTR getString(LONG index) { return (STRPTR)vals[index]; }
};

/*-------------------------------------------------------------------------------------------*/

class SysFile
{
	BPTR handle;
	STRPTR filename;

	public:

	BOOL ready;

	SysFile(STRPTR path, LONG mode);
	~SysFile();
	BOOL write(APTR buffer, LONG bytes);
	BPTR getHandle() { return handle; }
	STRPTR getName() { return filename; }
};


SysFile::SysFile(STRPTR path, LONG mode)
{
	filename = path;
	ready = FALSE;

	if (handle = Open(filename, mode))
	{
		D("SysFile: file '%s' opened [$%08lx].\n", filename, handle);
		ready = TRUE;
	}
	else FileProblem(filename);

	D("SysFile: created at $%08lx.\n", this);
}


SysFile::~SysFile()
{
	if (handle)
	{
		if (!(Close(handle))) FileProblem(filename);
		D("SysFile: file '%s' [$%08lx] closed.\n", filename, handle);
	}

	D("SysFile: object $%08lx disposed.\n", this);
}


BOOL SysFile::write(APTR buffer, LONG bytes)
{
	if (FWrite(handle, buffer, bytes, 1) == 1) return TRUE;
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

class App
{
	SysFile *inFile;
	AiffOutput *outFile;

	public:

	BOOL ready;

	App(CallArgs &args)
	{
		ready = FALSE;
		inFile = new SysFile(args.getString(0), MODE_OLDFILE);

		if (inFile->ready)
		{
			outFile = new AiffOutput(args.getString(1), 0x1000, 1, 44100);
			if (outFile->ready) ready = TRUE;
		}

		D("App: created at $%08lx.\n", this);
	}
	
	~App()
	{
		if (inFile) delete inFile;
		if (outFile) delete outFile;
		D("App: deleted object $%08lx.\n", this);
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