#pragma once

#include "base_types.h"
#include "control_input_poll_state.h"
#include "task_stack_allocation.h"

#include "event.h"
#include "kernel.h"
#include "msgport.h"

typedef struct ControlInputThreadState
{
  TaskStackAllocation stack_allocation;
  Item                thread_item;
  uint32              signal_mask;
  volatile uint8      run_requested;
  uint8               reserved_15[0x03];
  volatile uint8      start_complete;
  uint8               reserved_19[0x03];
  volatile uint8      enabled;
  uint8               reserved_1_d[0x0B];
  int32               output_enabled;
  uint32              raw_buttons;
  uint8               reserved_30[0x50];
  uint32              suppressed_buttons;
  Item                event_port_item;
  MsgPort            *event_port;
  Item                broker_message_item;
  Item                timer_io_request;
} ControlInputThreadState;

typedef int32 (*ControlPadReader)(int32                control_pad_number_,
                                  int32                wait_,
                                  ControlPadEventData *event_data_);

#define G_FORCED_CONTROL_PAD_BUTTONS (g_CONTROL_INPUT_POLL_STATE.status_08)
#define G_CONTROL_INPUT_BUTTONS (g_CONTROL_INPUT_POLL_STATE.status_0c)
extern int32                   g_CONTROL_PAD_COUNT;
extern int32                   g_CONTROL_INPUT_SECRET_ENABLED;
extern int32                   g_CONTROL_INPUT_SEQUENCE_STAGE;
extern int32                   g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT;
extern ControlPadReader        g_CONTROL_PAD_READER;
extern ControlInputThreadState g_CONTROL_INPUT_THREAD_STATE;

int32
store_polled_control_input(uint32 *control_input_);
int32
initialize_joystick_event_broker(void);
int32
receive_joystick_event_broker_messages(void);
void
calibrate_joystick_input(void);
void
poll_joystick_control_input(void);
void
run_control_input_worker(void);
int32
initialize_control_input_system(void);
void
disable_control_input_thread(void);
void
enable_control_input_thread(void);
Err
send_control_input_timer_offset_request(int32 offset_);
uint32
translate_control_pad_buttons(uint32 portfolio_buttons_);
void
initialize_control_pads(int32 control_pad_count_);
uint32
read_control_pad_buttons(int32 control_pad_index_);
uint32
read_primary_control_pad_buttons(void);
uint32
poll_control_input(void);
int32
read_forced_control_pad(int32 control_pad_number_, int32 wait_, ControlPadEventData *event_data_);
