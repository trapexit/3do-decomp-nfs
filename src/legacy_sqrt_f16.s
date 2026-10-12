	AREA	| legacy_sqrt_f16_code |, CODE
	; extern unsigned int SqrtF16(unsigned int value);
	EXPORT	SqrtF16

	MACRO
$label	SQRT_HIGH_STEP $shift
$label	add	r1, r12, r2, LSL #2
	cmp	r0, r1, LSL #$shift
	sbccs	r0, r0, r1, LSL #$shift
	adc	r2, r2, r2
	MEND

	MACRO
$label	SQRT_LOW_STEP
$label	mov	r0, r0, LSL #2
	add	r1, r12, r2, LSL #2
	cmp	r0, r1
	sbccs	r0, r0, r1
	adc	r2, r2, r2
	MEND

SqrtF16
	mov	r2, #0
	mov	r12, #1
	cmp	r0, #0x40000000
	sbccs	r0, r0, #0x40000000
	adc	r2, r2, r2

	SQRT_HIGH_STEP 28
	SQRT_HIGH_STEP 26
	SQRT_HIGH_STEP 24
	SQRT_HIGH_STEP 22
	SQRT_HIGH_STEP 20
	SQRT_HIGH_STEP 18
	SQRT_HIGH_STEP 16
	SQRT_HIGH_STEP 14
	SQRT_HIGH_STEP 12
	SQRT_HIGH_STEP 10
	SQRT_HIGH_STEP 8
	SQRT_HIGH_STEP 6
	SQRT_HIGH_STEP 4
	SQRT_HIGH_STEP 2
	SQRT_HIGH_STEP 0

	SQRT_LOW_STEP
	SQRT_LOW_STEP
	SQRT_LOW_STEP
	SQRT_LOW_STEP
	SQRT_LOW_STEP
	SQRT_LOW_STEP
	SQRT_LOW_STEP

	mov	r0, r0, LSL #2
	add	r1, r12, r2, LSL #2
	cmp	r0, r1
	adc	r0, r2, r2
	mov	pc, lr

	END
