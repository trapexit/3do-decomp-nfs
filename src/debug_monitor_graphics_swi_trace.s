	AREA	| debug_monitor_graphics_swi_trace_code |, CODE, READONLY
	IMPORT	g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	; extern void notify_monitor_before_graphics_swi(void);

	EXPORT	notify_monitor_before_graphics_swi
notify_monitor_before_graphics_swi
	stmfd	sp!, {r0, ip, lr}
	ldr	r0, g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	ldr	r0, [r0]
	tst	r0, #1
	swine	0x103
	ldmfd	sp!, {r0, ip, pc}

	stmfd	sp!, {r0, ip, lr}
	ldr	r0, g_DEBUG_MONITOR_SWI_TRACE_MASK_REFERENCE
	ldr	r0, [r0]
	tst	r0, #1
	swine	0x104
	ldmfd	sp!, {r0, ip, pc}

	END
