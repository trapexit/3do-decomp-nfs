	AREA	| fill_bytes_code |, CODE, READONLY
	; void fill_bytes(uint8 *destination, int32 byte_count, uint32 value);
	EXPORT	fill_bytes

fill_bytes
	tst	r0, #3
	bne	fill_bytes_tail
	and	r2, r2, #0xff
	orr	r2, r2, r2, lsl #8
	orr	r2, r2, r2, lsl #16
	cmp	r1, #0x100
	blo	fill_bytes_words
	stmdb	sp!, {r4-r11, lr}
	mov	r3, r2
	mov	r4, r2
	mov	r5, r2
	mov	r6, r2
	mov	r7, r2
	mov	r8, r2
	mov	r9, r2
	mov	r10, r2
	mov	r11, r2
	mov	r12, r2
	mov	lr, r2
	b	fill_bytes_blocks_test
fill_bytes_blocks
	stmia	r0!, {r2-r12, lr}
	stmia	r0!, {r2-r12, lr}
	stmia	r0!, {r2-r12, lr}
	stmia	r0!, {r2-r12, lr}
	stmia	r0!, {r2-r12, lr}
	stmia	r0!, {r2-r5}
fill_bytes_blocks_test
	subs	r1, r1, #0x100
	bpl	fill_bytes_blocks
	ldmia	sp!, {r4-r11, lr}
	add	r1, r1, #0x100
	b	fill_bytes_words
fill_bytes_word
	str	r2, [r0], #4
fill_bytes_words
	subs	r1, r1, #4
	bpl	fill_bytes_word
	add	r1, r1, #4
	b	fill_bytes_tail
fill_bytes_byte
	strb	r2, [r0], #1
fill_bytes_tail
	subs	r1, r1, #1
	bpl	fill_bytes_byte
	mov	pc, lr

	END
