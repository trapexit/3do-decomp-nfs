	AREA	| debug_monitor_math_swi_trace_code |, CODE, READONLY
	IMPORT	g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	; extern void notify_monitor_before_math_swi(void);

	EXPORT	notify_monitor_before_math_swi
notify_monitor_before_math_swi
	stmfd	sp!, {r0, ip, lr}
	ldr	r0, g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	ldr	r0, [r0]
	tst	r0, #2
	swine	0x103
	ldmfd	sp!, {r0, ip, pc}

	stmfd	sp!, {r0, ip, lr}
	ldr	r0, g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	ldr	r0, [r0]
	tst	r0, #2
	swine	0x104
	ldmfd	sp!, {r0, ip, pc}

	stmfd	sp!, {r0, ip, lr}
	ldr	r0, g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	ldr	r0, [r0]
	tst	r0, #4
	swine	0x103
	ldmfd	sp!, {r0, ip, pc}

	stmfd	sp!, {r0, ip, lr}
	ldr	r0, g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	ldr	r0, [r0]
	tst	r0, #4
	swine	0x104
	ldmfd	sp!, {r0, ip, pc}

	stmfd	sp!, {r1, lr}
	ldr	r1, g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	str	r0, [r1]
	ldmfd	sp!, {r1, pc}

	END
