#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <proto/locale.h>

#include <workbench/startup.h>
#include <dos/rdargs.h>

extern "C"
{
	void DecodeMonoFrame(ULONG *inbuf, WORD *outbuf, WORD slices);
	void DecodeStereoFrame(ULONG *inbuf, WORD *outbuf, WORD slices);
}

extern Library *SysBase, *DOSBase;

Library *IFFParseBase, *LocaleBase;
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

#define ID_AIFF MAKE_ID('A','I','F','F')
#define ID_COMM MAKE_ID('C','O','M','M')
#define ID_SSND MAKE_ID('S','S','N','D')

#define ID_QOA MAKE_ID('q','o','a','f')

/* maximum sizes of QOA frames in bytes WITHOUT 8-byte frame header */

#define QOA_FRAME_MONO      2064   /* 16 (LMS) + 256 * 8 (slices)     */
#define QOA_FRAME_STEREO    4128   /* 32 (LMS) + 256 * 2 * 8 (slices) */

ULONG QoaFrameSizes[2] = { QOA_FRAME_MONO, QOA_FRAME_STEREO };

char FaultBuffer[128];

struct AiffComm
{
	WORD NumChannels;
	ULONG NumSampleFrames;
	WORD SampleSize;
	UWORD SampRateMant;
	ULONG SampRateChar1;
	ULONG SampRateChar2;
};


BOOL Problem(STRPTR text)
{
	Printf("%s.\n", text);
	return FALSE;
}

BOOL IoErrProblem(STRPTR text)
{
	Fault(IoErr(), "", FaultBuffer, 128);
	Printf("%s: %s.\n", text, &FaultBuffer[2]);
	return FALSE;
}

BOOL FileProblem(STRPTR filename)
{
	Fault(IoErr(), "", FaultBuffer, 128);
	PutStr("Plik");
	Printf(" \"%s\": %s.\n", filename, &FaultBuffer[2]);
	return FALSE;
}

BOOL IFFProblem(STRPTR filename, LONG errcode)
{
	Printf("\"%s\": IFF error %ld.\n", filename, errcode);
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
	} 

	~CallArgs()
	{
		if (args)
		{
			FreeArgs(args);
			D("CallArgs: RDArgs [$%08lx] freed.\n", args);
		}
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
	BPTR getHandle() { return handle; }
	STRPTR getName() { return filename; }
};

SysFile::SysFile(STRPTR path, LONG mode)
{
	filename = path;
	ready = FALSE;

	if (!(handle = Open(filename, mode))) { FileProblem(filename); return; }
	D("SysFile: file '%s' opened [$%08lx].\n", filename, handle);
	ready = TRUE;
}

SysFile::~SysFile()
{
	if (!handle) return;
	if (!(Close(handle))) FileProblem(filename);
	D("SysFile: file '%s' [$%08lx] closed.\n", filename, handle);
}

/*-------------------------------------------------------------------------------------------*/

class IffChunk
{
	IFFHandle *iff;

	#ifdef DEBUG
	char b1[6];
	char b2[6];
	#endif

	public:

	BOOL ready;

	IffChunk(IFFHandle *iff, ULONG type, ULONG id, LONG size);
	~IffChunk();
	BOOL write(APTR data, LONG bytes);
};

IffChunk::IffChunk(IFFHandle *h, ULONG type, ULONG id, LONG size)
{
	LONG ifferr;

	iff = h;
	ready = FALSE;
	ifferr = PushChunk(iff, type, id, size);
	D("Chunk '%s'|'%s' pushed to [$%08lx].\n", IDtoStr(type, b1), IDtoStr(id, b2), iff);
	if (ifferr) IFFProblem("", ifferr);
	else ready = TRUE;
}

IffChunk::~IffChunk()
{
	LONG ifferr = 0;

	if (ready)
	{
		ifferr = PopChunk(iff);
		D("Chunk popped from [$%08lx].\n", iff);
	}
	if (ifferr) IFFProblem("", ifferr);
}

BOOL IffChunk::write(APTR data, LONG bytes)
{
	LONG ifferr;

	ifferr = WriteChunkRecords(iff, data, bytes, 1);
	if (ifferr == 1) return TRUE;
	else
	{
		IFFProblem("", ifferr);
		return FALSE;
	}
}

/*-------------------------------------------------------------------------------------------*/

class IffOutput : public SysFile
{
	BOOL opened;

	protected:

	IFFHandle *iff;

	public:

	BOOL ready;

	IffOutput(STRPTR filename);
	~IffOutput();
};

IffOutput::IffOutput(STRPTR filename) : SysFile(filename, MODE_NEWFILE)
{
	LONG ifferr;

	ready = FALSE;
	iff = NULL;
	if (!SysFile::ready) return;
	if (!(iff = AllocIFF())) { Problem("Out of memory"); return; }
	D("IffOutput: IFFHandle allocated [$%08lx].\n", iff);
	iff->iff_Stream = getHandle();
	InitIFFasDOS(iff);
	if (ifferr = OpenIFF(iff, IFFF_WRITE)) { IFFProblem(getName(), ifferr); return; }
	D("IffOutput: IFFHandle [$%08lx] opened.\n", iff);
	ready = TRUE;
}

IffOutput::~IffOutput()
{
	if (ready)
	{
		CloseIFF(iff);
		D("IffOutput: IFFHandle [$%08lx] closed.\n", iff);
	}

	if (iff)
	{
		FreeIFF(iff);
		D("IffOutput: IFFHandle [$%08lx] freed.\n", iff);
	}
}

/*-------------------------------------------------------------------------------------------*/

class AiffOutput : public IffOutput
{
	IffChunk *form;
	IffChunk *ssnd;

	void convertSampRate(struct AiffComm &acom, ULONG samprate);
	BOOL writeComm(ULONG samples, UWORD channels, ULONG samprate);

	public:

	BOOL ready;
	AiffOutput(STRPTR filename, ULONG samples, UWORD channels, ULONG samprate);
	~AiffOutput();

	void pushAudioBlock(WORD *block, LONG samples)
	{
		WriteChunkBytes(iff, block, samples << 1);
	}
};

AiffOutput::AiffOutput(STRPTR filename, ULONG samples, UWORD channels, ULONG samprate) : IffOutput(filename)
{
	ready = FALSE;
	form = NULL;
	ssnd = NULL;

	if (!IffOutput::ready) return;
	form = new IffChunk(iff, ID_AIFF, ID_FORM, IFFSIZE_UNKNOWN);
	if (!form->ready) return;
	if (!writeComm(samples, channels, samprate)) return;
	ssnd = new IffChunk(iff, ID_AIFF, ID_SSND, IFFSIZE_UNKNOWN);
	if (!ssnd->ready) return;
	ready = TRUE;
}

AiffOutput::~AiffOutput()
{
	if (ssnd) delete ssnd;
	if (form) delete form;
}

BOOL AiffOutput::writeComm(ULONG samples, UWORD channels, ULONG samprate)
{
	BOOL result;
	AiffComm acm;
	IffChunk comm(iff, ID_AIFF, ID_COMM, sizeof(AiffComm));

	if (!comm.ready) return FALSE;
	acm.NumChannels = channels;
	acm.NumSampleFrames = samples;
	acm.SampleSize = 16;
	convertSampRate(acm, samprate);
	return comm.write(&acm, sizeof(AiffComm));
}

/*----------------------------------------------------------------------------*/
/* Converts integer 24-bit sampling rate to 80-bit floating point (IEEE 754). */
/*----------------------------------------------------------------------------*/

void AiffOutput::convertSampRate(AiffComm &acom, ULONG samprate)
{
	UWORD mant = 16414;

	while ((samprate & 0x80000000) == 0)
	{
		mant--;
		samprate <<= 1;
	}

	acom.SampRateMant = mant;
	acom.SampRateChar1 = samprate;
	acom.SampRateChar2 = 0;
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
	if (FRead(handle, header, 8, 1) != 1) { FileProblem(filename); return; }
	if (header[0] != ID_QOA) { Problem("Not a QOA file"); return; }
	if (header[1] == 0) { Problem("Zero samples in QOA file."); return; }
	samples = header[1];
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

	if (FRead(handle, &probe, 4, 1) != 1) return FileProblem(filename);
	channels = probe >> 24;
	sampleRate = probe & 0x00FFFFFF;
	if (Seek(handle, -4, OFFSET_CURRENT) == -1) return FileProblem(filename);
	return TRUE;
}

/*-------------------------------------------------------------------------------------------*/

class App
{
	QoaInput *inFile;
	AiffOutput *outFile;
	ULONG *inBuf;
	WORD *outBuf;
	ULONG convertFrame();
	public:

	BOOL ready;
	App(CallArgs &args);
	~App();
	BOOL App::convertAudio();
};

App::App(CallArgs &args)
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

	if (FRead(inFile->getHandle(), header, 8, 1) != 1) { FileProblem(inFile->getName()); return 0; }
	channels = header[0] >> 24;
	samprate = header[0] & 0x00FFFFFF;
	fsamples = header[1] >> 16;
	fbytes = header[1] & 0x0000FFFF;
	D("QOA frame, %lu channels @ %lu Hz, %lu samples, %lu bytes to read.\n", channels, samprate,
		fsamples, fbytes);
	if (channels != inFile->channels) { Problem("Variable number of channels detected."); return 0; }
	if (samprate != inFile->sampleRate) { Problem("Variable sampling rate detected."); return 0; }
	if (fsamples == 0) { Problem("Zero samples specified in frame."); return 0; }
	if (fsamples > 5120) { Problem("More than 5120 samples in frame specified."); return 0; }
	slicesPerChannel = divu16(fsamples + 19, 20);
	expectedFrameSize = 8 + (8 << channels) + (slicesPerChannel << (channels + 2));
	if (expectedFrameSize != fbytes) { Problem("Expected and specified frame size differs."); return 0; }
	D("expected frame size %ld bytes.\n", expectedFrameSize);
	if (FRead(inFile->getHandle(), inBuf, fbytes - 8, 1) != 1) { FileProblem(inFile->getName()); return 0; }
	if (channels == 1) DecodeMonoFrame(inBuf, outBuf, slicesPerChannel);
	else DecodeStereoFrame(inBuf, outBuf, slicesPerChannel);
	outFile->pushAudioBlock(outBuf, fsamples << (channels - 1));
	return fsamples;
}

BOOL App::convertAudio()
{
	ULONG decoded = 0;

	if (outBuf = (WORD*) new UBYTE[5120 << inFile->channels])
	{
		if (inBuf = (ULONG*) new UBYTE[QoaFrameSizes[inFile->channels - 1]])
		{
			ULONG fsamples = 0;

			while ((decoded < inFile->samples) && (fsamples = convertFrame()))
			{
				decoded += fsamples;
			}

			delete inBuf;
		}

		delete outBuf;
	}

	PutStr("audio converted.\n");
}

/*-------------------------------------------------------------------------------------------*/

LONG Main(WBStartup *wbmsg)
{
	App *app;

	/* Locale are optional. */

	if (LocaleBase = OpenLibrary("locale.library", 39))
	{
		Cat = OpenCatalogA(NULL, "QoaToAiff.catalog", NULL);
	}

	if (IFFParseBase = OpenLibrary("iffparse.library", 39))
	{
		CallArgs args;

		if (args.ready)
		{
			App app(args);

			if (app.ready)
			{
				app.convertAudio();
			}
		}

		CloseLibrary(IFFParseBase);
	}

	if (LocaleBase)
	{
		CloseCatalog(Cat);                 /* NULL-safe */
		CloseLibrary(LocaleBase);
	}

	return 0;
}