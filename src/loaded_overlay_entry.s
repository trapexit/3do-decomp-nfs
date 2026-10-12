	AREA	| loaded_overlay_entry_code |, CODE, READONLY
	; extern void *execute_loaded_overlay_and_resolve_entry(void *image);

overlay_return_instruction
	DCD	0xE1A0F007

	EXPORT	execute_loaded_overlay_and_resolve_entry
execute_loaded_overlay_and_resolve_entry
	ldr	r1, [r0, #12]
	ldr	r2, overlay_return_instruction
	str	r2, [r0, #12]
	stmfd	sp!, {r0-r12, lr}
	mov	r7, pc
	mov	pc, r0
	ldmfd	sp!, {r0-r12, lr}
	str	r1, [r0, #12]
	mov	r1, r1, lsl #8
	add	r1, r0, r1, asr #6
	add	r1, r1, #20
overlay_branch_scan
	ldrb	r2, [r1], #4
	cmp	r2, #0xEB
	bne	overlay_branch_scan
	ldr	r2, [r1, #-4]
	mov	r2, r2, lsl #8
	add	r2, r1, r2, asr #6
	add	r0, r2, #4
	mov	pc, lr

	END
