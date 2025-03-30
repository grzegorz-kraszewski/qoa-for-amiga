;
; vasm -Faout -no-opt -o slice.o slice.s

OpenLibrary	= -552
CloseLibrary	= -414
Open		= -30
Close		= -36
Read		= -42
Write		= -48

MODE_OLDFILE	= 1005
MODE_NEWFILE	= 1006

; test code: loading one full frame (256 mono slices) from a hardcoded file.

		MOVEM.L	d2-d3/a2-a3/a6,-(sp)
		MOVEA.L	$4,a6
		LEA	DosName(pc),a1
		MOVEQ	#39,d0
		JSR	OpenLibrary(a6)
		TST.L	d0
		BEQ.S	NoDos
		MOVEA.L	a6,a3
		MOVEA.L	d0,a6
		LEA	InFileName(pc),a0
		MOVE.L	a0,d1
		MOVE.L	#MODE_OLDFILE,d2
		JSR	Open(a6)
		MOVE.L	d0,d1
		BEQ.S	NoFile
		MOVEA.L	d0,a2
		LEA	FullFrame(pc),a0
		MOVE.L	a0,d2
		MOVE.L	#2080,d3
		JSR	Read(a6)
		MOVE.L	a2,d1
		JSR	Close(a6)
		
		MOVE.W	#255,d0
		LEA	FullFrame(pc),a0
		LEA	8(a0),a0               ; skip QOA file header
		LEA	FullOutput(pc),a1
		BSR.W	monoframe

		LEA	OutFileName(pc),a0
		MOVE.L	a0,d1
		MOVE.L	#MODE_NEWFILE,d2
		JSR	Open(a6)
		MOVE.L	d0,d1
		BEQ.S	NoFile
		MOVEA.L	d0,a2
		LEA	FullOutput(pc),a0
		MOVE.L	a0,d2
		MOVE.L	#10240,d3
		JSR	Write(a6)
		MOVE.L	a2,d1
		JSR	Close(a6)		
NoFile:		MOVEA.L	a6,a1
		MOVEA.L	a3,a6
		JSR	CloseLibrary(a6)		
NoDos:		MOVEM.L	(sp)+,d2-d3/a2-a3/a6
		RTS

InFileName	DC.B	"mono-5120-encoded.qoa", 0
OutFileName	DC.B	"mono-5120-test.pcm", 0
DosName:	DC.B	"dos.library", 0
FullFrame:	DS.B	2080
		ALIGN	1
FullOutput: 	DS.W	5120

;==============================================================================
; decodes complete QOA frame with one audio channel to a buffer (5120 WORD
; samples)
; INPUTS:
;   d0.w - number of slices available in the frame buffer, minus 1
;          (0 to 255 including)
;   a0.l - frame buffer
;   a1.l - output buffer (at least 10240 bytes)
;==============================================================================

monoframe:	MOVEM.L	d2-d7/a2-a6,-(sp)
		MOVE.W	d0,d7                  ; slice counter
		LEA	8(a0),a0               ; skip frame header
		MOVEA.W	(a0)+,a2               ; loading LMS history
		MOVEA.W	(a0)+,a3
		MOVEA.W	(a0)+,a4
		MOVEA.W	(a0)+,a5
		MOVE.L	(a0)+,d2               ; loading LMS weights
		MOVE.L	(a0)+,d3
		LEA	dequant(pc),a6         ; pointer to lookup table
nextslice:	MOVE.L	(a0)+,d0               ; the first half of slice
		MOVE.L	(a0)+,d1               ; the second half of slice
		MOVE.L	d7,-(sp)               ; lame, but I'm out of registers
		BSR.S	monoslice              ; decode slice
		MOVE.L	(sp)+,d7
		DBF	d7,nextslice
		MOVEM.L	(sp)+,d2-d7/a2-a6
		RTS

;==============================================================================
; Decodes QOA slice of mono/stereo stream.
; Registers usage:
;   d0,d1 - slice (in)
;   d2,d3 - LMS weights (in/out)
;   d4 - residual sample, quantized, dequantized, scaled (internal)
;   d5 - predicted sample (internal)
;   d6 - keeps extracted and shifted scalefactor(internal)
;   d7 - scratch register (internal)
;   a2,a3,a4,a5 - LMS history (in/out)
;   a1 - output buffer (in/out)
;   a6 - pointer to 'dequant' lookup table (in/out)
;==============================================================================

monoslice:	MOVEQ	#0,d4
		ROL.L	#8,d0
		MOVE.B	d0,d6
		ANDI.B	#$F0,d6                ; scale factor in bits 7:4 of d6

		;extract 8 residuals from d0
		
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r1 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r2 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r3 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r4 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r5 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r6 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r7 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r8 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		
		; now the first bit of r9 is in d0:0, pull two bits from d1
		
		LSL.L	#1,d1
		ROXL.L	#1,d0
		LSL.L	#1,d1
		ROXL.L	#1,d0
		LSL.B   #1,d0                  ; r9 in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		
		; extract 10 residuals from d1
		
		ROL.L	#4,d1                  ; rA in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rB in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rC in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rD in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rE in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rF in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rG in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rH in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rI in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; rJ in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		RTS
	
;==============================================================================	
; Decodes a single sample. 3-bit encoded sample is in bits 3:1 of register d4
;==============================================================================	

		; decode residual sample using lookup table, store in d4

DecSamp:	ANDI.W	#$0E,d4                ; extract encoded sample in d4 
		OR.B	d6,d4                  ; merge with encoded scale factor
		MOVE.W	(a6,d4.w),d4           ; decode with lookup table

		; calculate predicted sample, store in d5

		MOVE.W	a5,d7                  ; history[-1]
		MULS.W	d3,d7                  ; *= weights[-1]
		MOVE.L	d7,d5
		SWAP	d3
		MOVE.W	a4,d7                  ; history[-2]
		MULS.W	d3,d7                  ; *= weights[-2]
		ADD.L	d7,d5
		MOVE.W	a3,d7                  ; history[-3]
		MULS.W	d2,d7                  ; *= weights[-3]
		ADD.L	d7,d5
		SWAP	d2
		MOVE.W	a2,d7                  ; history[-4]
		MULS.W	d2,d7                  ; *= weights[-4]
		ADD.L	d7,d5
		ASR.L	#6,d5
		ASR.L	#7,d5                  ; predicted sample in d5

		; add predicted sample to reconstructed residual with clamp to
		; 16-bit signed range, store in d5, code by meynaf from EAB

		ADD.W	d4,d5
		BVC.S	clamped
		SPL 	d5
		EXT.W	d5
		EORI.W	#$7FFF,d5

		; update LMS weights, reconstructed sample in d5, decoded
		; residual in d4

clamped:	ASR.W	#4,d4                  ; scale residual signal down

		MOVE.W	a2,d7
		BMI.S	h4neg
		ADD.W	d4,d2
		BRA.S	h3
h4neg:  	SUB.W	d4,d2
h3:		SWAP	d2
		MOVE.W	a3,d7
		BMI.S	h3neg
		ADD.W	d4,d2
		BRA.S	h2
h3neg:		SUB.W	d4,d2
h2:		MOVE.W	a4,d7
		BMI.S	h2neg
		ADD.W	d4,d3
		BRA.S	h1
h2neg:		SUB.W	d4,d3
h1:		SWAP	d3
		MOVE.W	a5,d7
		BMI.S	h1neg
		ADD.W	d4,d3
		BRA.S	update
h1neg:  	SUB.W	d4,d3

		; update history vector
		
update: 	MOVEA.W	a3,a2
		MOVEA.W	a4,a3
		MOVEA.W	a5,a4
		MOVEA.W	d5,a5

		; store output sample

		MOVE.W 	d5,(a1)+
		RTS

dequant:	DC.W	   1,    -1,    3,    -3,    5,    -5,     7,     -7
		DC.W	   5,    -5,   18,   -18,   32,   -32,    49,    -49
		DC.W	  16,   -16,   53,   -53,   95,   -95,   147,   -147
		DC.W	  34,   -34,  113,  -113,  203,  -203,   315,   -315
		DC.W	  63,   -63,  210,  -210,  378,  -378,   588,   -588
		DC.W	 104,  -104,  345,  -345,  621,  -621,   966,   -966
		DC.W	 158,  -158,  528,  -528,  950,  -950,  1477,  -1477
		DC.W	 228,  -228,  760,  -760, 1368, -1368,  2128,  -2128
		DC.W	 316,  -316, 1053, -1053, 1895, -1895,  2947,  -2947
		DC.W	 422,  -422, 1405, -1405, 2529, -2529,  3934,  -3934
		DC.W	 548,  -548, 1828, -1828, 3290, -3290,  5117,  -5117
		DC.W	 696,  -696, 2320, -2320, 4176, -4176,  6496,  -6496
		DC.W	 868,  -868, 2893, -2893, 5207, -5207,  8099,  -8099
		DC.W	1064, -1064, 3548, -3548, 6386, -6386,  9933,  -9933
		DC.W	1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005
		DC.W	1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336
