;
; QOA decoder

;==============================================================================
; STEREO DECODING STRATEGY
;
; Stereo QOA files have interleaved slices in LR order. Decoding them in order
; means LMS state has to be swapped for each slice. To avoid this, there are 
; two passes over a frame. The first pass loads L channel LMS state then
; decodes only even slices and stores audio samples at output buffer offset 0,
; advancing 4 bytes after each sample. The second pass loads R channel LMS
; state, then decodes only odd slices and stores audio samples at output buffer
; offset 2, advancing 4 bytes after each sample.
;==============================================================================

;==============================================================================
; Decodes QOA mono frame to a buffer.
; INPUTS:
;   d0 - number of slices available in the frame buffer (1 to 256 including)
;   a0 - frame buffer
;   a1 - output buffer
;==============================================================================

		XDEF	_DecodeMonoFrame

_DecodeMonoFrame:
		MOVEM.L	d2-d7/a2-a6,-(sp)
		SUBQ.L	#1,d0
		LEA	sampoff,a2
		MOVE.W	#0,(a2)
		MOVE.W	d0,d7                  ; slice counter
		MOVEA.W	(a0)+,a2               ; loading LMS history
		MOVEA.W	(a0)+,a3
		MOVEA.W	(a0)+,a4
		MOVEA.W	(a0)+,a5
		MOVE.L	(a0)+,d2               ; loading LMS weights
		MOVE.L	(a0)+,d3
nextslice: 	LEA	dequant(pc),a6         ; pointer to lookup table
		MOVE.L	(a0)+,d0               ; the first half of slice
		MOVE.L	(a0)+,d1               ; the second half of slice
		BSR.S	slice                  ; decode slice
		DBF	d7,nextslice
		MOVEM.L	(sp)+,d2-d7/a2-a6
		RTS

;==============================================================================
; Decodes QOA stereo frame to a buffer.
; INPUTS:
;   d0.w - number of slices per channel available in the frame buffer (1 to 256
;          including)
;   a0.l - frame buffer
;   a1.l - output buffer
;==============================================================================

		XDEF	_DecodeStereoFrame

_DecodeStereoFrame:
		MOVEM.L	d2-d7/a2-a6,-(sp)
		SUBQ.L	#1,d0
		LEA	sampoff,a2
		MOVE.W	#2,(a2)
		MOVE.L	a0,-(sp)
		MOVE.L	a1,-(sp)
		MOVE.W	d0,-(sp)

		; L channel pass

		MOVE.W	d0,d7                  ; slice counter
		MOVEA.W	(a0)+,a2               ; loading LMS history
		MOVEA.W	(a0)+,a3
		MOVEA.W	(a0)+,a4
		MOVEA.W	(a0)+,a5
		MOVE.L	(a0)+,d2               ; loading LMS weights
		MOVE.L	(a0)+,d3
		LEA	16(a0),a0              ; skip R channel LMS state
nextleft: 	LEA	dequant(pc),a6         ; pointer to lookup table
		MOVE.L	(a0)+,d0               ; the first half of slice
		MOVE.L	(a0)+,d1               ; the second half of slice
		BSR.S	slice                  ; decode slice
		ADDQ.L	#8,a0                  ; skip R channel slice
		DBF	d7,nextleft

		; R channel pass

		MOVE.W	(sp)+,d7               ; slice counter
		MOVEA.L	(sp)+,a1               ; output buffer
		MOVEA.L	(sp)+,a0               ; input buffer
		ADDQ.L	#2,a1                  ; R channel samples
		LEA	16(a0),a0              ; skip L channel LMS state
		MOVEA.W	(a0)+,a2               ; loading LMS history
		MOVEA.W	(a0)+,a3
		MOVEA.W	(a0)+,a4
		MOVEA.W	(a0)+,a5
		MOVE.L	(a0)+,d2               ; loading LMS weights
		MOVE.L	(a0)+,d3
nextright: 	LEA	dequant(pc),a6         ; pointer to lookup table
		ADDQ	#8,a0                  ; skip L channel slice
		MOVE.L	(a0)+,d0               ; the first half of slice
		MOVE.L	(a0)+,d1               ; the second half of slice
		BSR.S	slice                  ; decode slice
		DBF	d7,nextright

		MOVEM.L	(sp)+,d2-d7/a2-a6
		RTS

;==============================================================================
; Decodes QOA slice of mono/stereo stream.
; Registers usage:
;   d0,d1 - slice (input, modified)
;   d2,d3 - LMS weights (input, updated)
;   d4 - residual sample, quantized, dequantized, scaled (modified)
;   d5 - predicted sample (modified)
;   d6 - scratch register (modified)
;   d7 - unused
;   a0 - unused
;   a1 - output buffer (input, updated)
;   a2,a3,a4,a5 - LMS history (input, updated)
;   a6 - pointer to 'dequant' lookup table (input, modified)
;==============================================================================

slice:		ROL.L	#8,d0
		MOVE.B	d0,d4
		ANDI.W	#$00F0,d4              ; scale factor in bits 7:4 of d4
		ADDA.W	d4,a6                  ; select lookup table row

		;extract 9 residuals from d0, r[0] is in position already

		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[1] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[2] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[3] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[4] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[5] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[6] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[7] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp
		ROL.L	#3,d0                  ; r[8] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp

		; now the first bit of r[9] is in d0:0, pull two bits from d1

		LSL.L	#1,d1
		ROXL.L	#1,d0
		LSL.L	#1,d1
		ROXL.L	#1,d0
		LSL.B	#1,d0                  ; r[9] in bits 3:1 of d0
		MOVE.B	d0,d4
		BSR.S	DecSamp

		; extract 10 residuals from d1

		ROL.L	#4,d1                  ; r[10] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[11] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[12] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[13] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[14] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[15] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[16] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[17] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[18] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		ROL.L	#3,d1                  ; r[19] in bits 3:1 of d1
		MOVE.B	d1,d4
		BSR.S   DecSamp
		RTS

;==============================================================================
; Decodes a single sample. 3-bit encoded sample is in bits 3:1 of register d4
;==============================================================================

		; decode residual sample using lookup table, store in d4

DecSamp:	ANDI.W	#$0E,d4                ; extract encoded sample in d4
		MOVE.W	(a6,d4.w),d4           ; decode with lookup table

		; calculate predicted sample, store in d5

		MOVE.W	a5,d6                  ; history[-1]
		MULS.W	d3,d6                  ; *= weights[-1]
		MOVE.L	d6,d5
		SWAP	d3
		MOVE.W	a4,d6                  ; history[-2]
		MULS.W	d3,d6                  ; *= weights[-2]
		ADD.L	d6,d5
		MOVE.W	a3,d6                  ; history[-3]
		MULS.W	d2,d6                  ; *= weights[-3]
		ADD.L	d6,d5
		SWAP	d2
		MOVE.W	a2,d6                  ; history[-4]
		MULS.W	d2,d6                  ; *= weights[-4]
		ADD.L	d6,d5
		ASR.L	#6,d5
		ASR.L	#7,d5                  ; predicted sample in d5

		; add predicted sample to reconstructed residual with clamp to
		; 16-bit signed range, store in d5, code by meynaf from EAB

		EXT.L	d4
		ADD.L	d4,d5
		CMPI.L	#32767,d5
		BLE.S	noupper
		MOVE.W	#32767,d5
		BRA.S	clamped
noupper:	CMPI.L	#-32768,d5
		BGE.S	clamped
		MOVE.W	#-32768,d5

		; update LMS weights, reconstructed sample in d5, decoded
		; residual in d4

clamped:	ASR.W	#4,d4                  ; scale residual signal down

		MOVE.W	a2,d6
		BMI.S	h4neg
		ADD.W	d4,d2
		BRA.S	h3
h4neg:  	SUB.W	d4,d2
h3:		SWAP	d2
		MOVE.W	a3,d6
		BMI.S	h3neg
		ADD.W	d4,d2
		BRA.S	h2
h3neg:		SUB.W	d4,d2
h2:		MOVE.W	a4,d6
		BMI.S	h2neg
		ADD.W	d4,d3
		BRA.S	h1
h2neg:		SUB.W	d4,d3
h1:		SWAP	d3
		MOVE.W	a5,d6
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
		ADDA.W	sampoff(pc),a1

		RTS

; not very effective, should be stored in some register once registers
; usage is optimized

sampoff:	DC.W	0

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
