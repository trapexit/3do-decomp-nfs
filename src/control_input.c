#include "control_input.h"

#include "control_input_poll_state.h"
#include "display_coordinate_state.h"
#include "driving_warning_display.h"
#include "game_bootstrap.h"
#include "game_frame_count.h"
#include "game_loop.h"
#include "io_offset_request.h"
#include "record_value_copy.h"
#include "simulation_thread_signal.h"
#include "vehicle_dynamics_state.h"
#include "vehicle_render_resource_selection.h"

#include "event.h"
#include "graphics.h"
#include "operror.h"
#include "task.h"

#include "stddef.h"
#include "string.h"

#define CONTROL_INPUT_WORKER_STACK_BYTES 2000
#define CONTROL_INPUT_WORKER_PRIORITY 12
#define CONTROL_INPUT_WORKER_NAME "Joy"

#define JOYSTICK_EVENT_PORT_NAME "JoyPort"
#define JOYSTICK_EVENT_QUEUE_MAX 10
#define JOYSTICK_RECORD_BUTTONS_INDEX 0
#define JOYSTICK_RECORD_HORIZONTAL_INDEX 1
#define JOYSTICK_RECORD_VERTICAL_INDEX 2

#define JOYSTICK_AXIS_FIXED_SHIFT 16
#define JOYSTICK_AXIS_SCALE 0x23
#define JOYSTICK_AXIS_NEGATIVE_OFFSET 3
#define JOYSTICK_AXIS_POSITIVE_OFFSET 4
#define JOYSTICK_AXIS_DIGITAL_THRESHOLD 12
#define JOYSTICK_X_MINIMUM (-0x20)
#define JOYSTICK_X_MAXIMUM 0x1F
#define JOYSTICK_Y_MINIMUM (-0x1E)
#define JOYSTICK_Y_MAXIMUM 0x1E
#define JOYSTICK_CALIBRATION_SENTINEL 500
#define JOYSTICK_CONTROL_BUTTON_MASK 0xFFF00000

#define GAME_CONTROL_LEFT 0x008
#define GAME_CONTROL_RIGHT 0x004
#define GAME_CONTROL_UP 0x001
#define GAME_CONTROL_DOWN 0x002
#define GAME_CONTROL_A 0x010
#define GAME_CONTROL_B 0x040
#define GAME_CONTROL_C 0x020
#define GAME_CONTROL_START 0x080
#define GAME_CONTROL_X 0x100
#define GAME_CONTROL_RIGHT_SHIFT 0x400
#define GAME_CONTROL_LEFT_SHIFT 0x200

#define CONTROL_INPUT_JOYSTICK_PRESET_INDEX 5
#define CONTROL_INPUT_SECONDARY_PAD 2
#define CONTROL_INPUT_TERTIARY_PAD 3
#define CONTROL_INPUT_EARLY_FRAME_LIMIT 600
#define CONTROL_INPUT_DISPLAY_TIMER 30
#define CONTROL_INPUT_SEQUENCE_STAGE_SECOND 2
#define CONTROL_INPUT_SEQUENCE_STAGE_THIRD 3
#define CONTROL_INPUT_SEQUENCE_STAGE_COMPLETE 4
#define CONTROL_INPUT_SECRET_SECONDARY_BUTTONS 0x40600000U
#define CONTROL_INPUT_SECRET_TERTIARY_BUTTONS 0x0A600000U
#define CONTROL_INPUT_SEQUENCE_FIRST_BUTTONS 0x10600000U
#define CONTROL_INPUT_SEQUENCE_SECOND_BUTTONS 0x40600000U
#define CONTROL_INPUT_SEQUENCE_THIRD_BUTTONS 0x20600000U
#define CONTROL_INPUT_SEQUENCE_FOURTH_BUTTONS 0x80600000U
#define CONTROL_INPUT_OPPONENT_OVERRIDE_BUTTONS 0x84400000U
#define CONTROL_INPUT_TUNING_SECONDARY_BUTTONS 0x03800000U
#define CONTROL_INPUT_TUNING_PRIMARY_BUTTONS 0x04000000U

/* Retains the broker's 32-bit wire enum slots across compiler enum ABI changes. */
typedef struct JoystickEventConfigurationRequest
{
  int32  flavor;
  int32  category;
  uint32 trigger_mask[8];
  uint32 capture_mask[8];
  int32  queue_max;
  uint32 reserved[8];
} JoystickEventConfigurationRequest;

typedef char SizeAssertControlInputThreadItemOffset
  [(offsetof(ControlInputThreadState, thread_item) == 0x0C) ? 1 : -1];
typedef char SizeAssertControlInputSignalMaskOffset
  [(offsetof(ControlInputThreadState, signal_mask) == 0x10) ? 1 : -1];
typedef char SizeAssertControlInputRunRequestedOffset
  [(offsetof(ControlInputThreadState, run_requested) == 0x14) ? 1 : -1];
typedef char SizeAssertControlInputStartCompleteOffset
  [(offsetof(ControlInputThreadState, start_complete) == 0x18) ? 1 : -1];
typedef char
  SizeAssertControlInputEnabledOffset[(offsetof(ControlInputThreadState, enabled) == 0x1C) ? 1 :
                                                                                             -1];
typedef char SizeAssertControlInputOutputEnabledOffset
  [(offsetof(ControlInputThreadState, output_enabled) == 0x28) ? 1 : -1];
typedef char SizeAssertControlInputRawButtonsOffset
  [(offsetof(ControlInputThreadState, raw_buttons) == 0x2C) ? 1 : -1];
typedef char SizeAssertControlInputSuppressedButtonsOffset
  [(offsetof(ControlInputThreadState, suppressed_buttons) == 0x80) ? 1 : -1];
typedef char SizeAssertControlInputEventPortItemOffset
  [(offsetof(ControlInputThreadState, event_port_item) == 0x84) ? 1 : -1];
typedef char SizeAssertControlInputEventPortOffset
  [(offsetof(ControlInputThreadState, event_port) == 0x88) ? 1 : -1];
typedef char SizeAssertControlInputBrokerMessageItemOffset
  [(offsetof(ControlInputThreadState, broker_message_item) == 0x8C) ? 1 : -1];
typedef char SizeAssertControlInputTimerIoRequestOffset
  [(offsetof(ControlInputThreadState, timer_io_request) == 0x90) ? 1 : -1];
typedef char SizeAssertJoystickEventConfigurationRequest
  [(sizeof(JoystickEventConfigurationRequest) == 0x6C) ? 1 : -1];
typedef char
  SizeAssertJoystickAxisCalibrationSize[(sizeof(JoystickAxisCalibration) == 0x14) ? 1 : -1];
typedef char SizeAssertJoystickCalibrationCurrentXOffset
  [(offsetof(JoystickCalibrationState, current_x) == 0x28) ? 1 : -1];
typedef char SizeAssertJoystickCalibrationCurrentYOffset
  [(offsetof(JoystickCalibrationState, current_y) == 0x2C) ? 1 : -1];
typedef char SizeAssertJoystickCalibrationButtonsOffset
  [(offsetof(JoystickCalibrationState, buttons) == 0x30) ? 1 : -1];
typedef char SizeAssertJoystickCalibrationRequestOffset
  [(offsetof(JoystickCalibrationState, recalibration_requested) == 0x34) ? 1 : -1];
typedef char
  SizeAssertJoystickCalibrationStateSize[(sizeof(JoystickCalibrationState) == 0x40) ? 1 : -1];
typedef char SizeAssertControlInputForcedButtonsOffset
  [(offsetof(ControlInputPollState, status_08) == 0x08) ? 1 : -1];
typedef char
  SizeAssertControlInputButtonsOffset[(offsetof(ControlInputPollState, status_0c) == 0x0C) ? 1 :
                                                                                             -1];

ControlPadReader               g_CONTROL_PAD_READER = NULL;
int32                          g_CONTROL_PAD_COUNT  = 0;
ControlInputThreadState        g_CONTROL_INPUT_THREAD_STATE;
extern ControlInputThreadState g_CONTROL_INPUT_THREAD_STATE_FLAGS;
extern ControlInputThreadState g_CONTROL_INPUT_THREAD_STATE_SERVICES;
int32                          g_CONTROL_INPUT_SECRET_ENABLED;
int32                          g_CONTROL_INPUT_SEQUENCE_STAGE;
uint32                         g_CONTROL_INPUT_SEQUENCE_RESERVED_STATE;
int32                          g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT = 3;

/* Converts one calibrated analog sample to its bounded signed axis value. */
static int32
_scale_joystick_axis(const JoystickAxisCalibration *calibration_,
                     int32                          current_,
                     int32                          negative_limit_,
                     int32                          positive_limit_);
/* Seeds all calibration extrema with the oracle's acquisition sentinel. */
static void
_reset_joystick_calibration_extrema(void);
/* Expands both axis extrema with the current broker sample. */
static void
_record_joystick_calibration_extrema(void);
/* Pumps broker events until every relevant joystick button is released. */
static void
_wait_for_joystick_button_release(void);

int32
store_polled_control_input(uint32 *control_input_)
{
  *control_input_ = poll_control_input();
  return 0;
}

uint32
poll_control_input(void)
{
  ControlPadEventData event_data;
  uint32              primary_buttons;
  uint32              secondary_buttons;
  uint32              tertiary_buttons;

  (void) GetControlPad(1, 0, &event_data);
  primary_buttons                           = event_data.cped_ButtonBits;
  g_CONTROL_INPUT_POLL_STATE.status_00      = primary_buttons;
  g_CONTROL_INPUT_POLL_STATE.primary_status = primary_buttons;
  if(g_STARTUP_CONFIGURATION.control_preset_index == CONTROL_INPUT_JOYSTICK_PRESET_INDEX)
    {
      primary_buttons |= g_CONTROL_INPUT_POLL_STATE.status_0c;
      g_CONTROL_INPUT_POLL_STATE.status_00 = primary_buttons;
    }

  (void) GetControlPad(CONTROL_INPUT_SECONDARY_PAD, 0, &event_data);
  secondary_buttons                    = event_data.cped_ButtonBits;
  g_CONTROL_INPUT_POLL_STATE.status_04 = secondary_buttons;
  (void) GetControlPad(CONTROL_INPUT_TERTIARY_PAD, 0, &event_data);
  tertiary_buttons                     = event_data.cped_ButtonBits;
  g_CONTROL_INPUT_POLL_STATE.status_08 = tertiary_buttons;

  if(G_GAME_FRAME_COUNTER >= CONTROL_INPUT_EARLY_FRAME_LIMIT)
    return primary_buttons;

  if(G_FRONTEND_EXIT_REQUESTED == 1)
    {
      if((g_CONTROL_INPUT_POLL_STATE.status_04 == CONTROL_INPUT_SECRET_SECONDARY_BUTTONS) &&
         (tertiary_buttons == CONTROL_INPUT_SECRET_TERTIARY_BUTTONS))
        {
          g_CONTROL_INPUT_SECRET_ENABLED                              = 1;
          g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer = CONTROL_INPUT_DISPLAY_TIMER;
        }

      if(g_CONTROL_INPUT_SEQUENCE_STAGE == 0)
        {
          if((g_CONTROL_INPUT_POLL_STATE.status_00 == CONTROL_INPUT_SEQUENCE_FIRST_BUTTONS) &&
             (G_GAME_FRAME_COUNTER < g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT))
            {
              g_CONTROL_INPUT_SEQUENCE_STAGE       = 1;
              g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT = G_GAME_FRAME_COUNTER;
            }
        }
      else if(g_CONTROL_INPUT_SEQUENCE_STAGE == 1)
        {
          if((g_CONTROL_INPUT_POLL_STATE.status_00 == CONTROL_INPUT_SEQUENCE_SECOND_BUTTONS) &&
             (G_GAME_FRAME_COUNTER < g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT))
            {
              g_CONTROL_INPUT_SEQUENCE_STAGE       = CONTROL_INPUT_SEQUENCE_STAGE_SECOND;
              g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT = G_GAME_FRAME_COUNTER;
            }
        }
      else if(g_CONTROL_INPUT_SEQUENCE_STAGE == CONTROL_INPUT_SEQUENCE_STAGE_SECOND)
        {
          if((g_CONTROL_INPUT_POLL_STATE.status_00 == CONTROL_INPUT_SEQUENCE_THIRD_BUTTONS) &&
             (G_GAME_FRAME_COUNTER < g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT))
            {
              g_CONTROL_INPUT_SEQUENCE_STAGE       = CONTROL_INPUT_SEQUENCE_STAGE_THIRD;
              g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT = G_GAME_FRAME_COUNTER;
            }
        }
      else if((g_CONTROL_INPUT_SEQUENCE_STAGE == CONTROL_INPUT_SEQUENCE_STAGE_THIRD) &&
              (g_CONTROL_INPUT_POLL_STATE.status_00 == CONTROL_INPUT_SEQUENCE_FOURTH_BUTTONS) &&
              (G_GAME_FRAME_COUNTER < g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT))
        {
          g_CONTROL_INPUT_SEQUENCE_STAGE       = CONTROL_INPUT_SEQUENCE_STAGE_COMPLETE;
          g_CONTROL_INPUT_SEQUENCE_FRAME_LIMIT = G_GAME_FRAME_COUNTER;
        }
    }
  else if(g_CONTROL_INPUT_POLL_STATE.status_04 == CONTROL_INPUT_OPPONENT_OVERRIDE_BUTTONS)
    {
      G_OPPONENT_VEHICLE_OVERRIDE = 1;
    }
  else if((g_CONTROL_INPUT_POLL_STATE.status_04 == CONTROL_INPUT_TUNING_SECONDARY_BUTTONS) &&
          (g_CONTROL_INPUT_POLL_STATE.status_00 == CONTROL_INPUT_TUNING_PRIMARY_BUTTONS))
    {
      g_VEHICLE_SPECIAL_SURFACE_TUNING_ENABLED                    = 1;
      g_DRIVING_WARNING_DISPLAY_STATE.control_input_display_timer = CONTROL_INPUT_DISPLAY_TIMER;
    }

  return g_CONTROL_INPUT_POLL_STATE.status_00;
}

int32
initialize_joystick_event_broker(void)
{
  JoystickEventConfigurationRequest configuration_request;
  Item                              event_broker_port;
  Err                               status;

  g_CONTROL_INPUT_THREAD_STATE.enabled = 0;
  event_broker_port                    = FindMsgPort(EventPortName);
  if(event_broker_port < 0)
    {
      PrintfSysErr(event_broker_port);
      return 0;
    }

  g_CONTROL_INPUT_THREAD_STATE.event_port_item = CreateMsgPort(JOYSTICK_EVENT_PORT_NAME, 0, 0);
  if(g_CONTROL_INPUT_THREAD_STATE.event_port_item < 0)
    {
      PrintfSysErr(g_CONTROL_INPUT_THREAD_STATE.event_port_item);
      return 0;
    }

  g_CONTROL_INPUT_THREAD_STATE.event_port =
    (MsgPort *) LookupItem(g_CONTROL_INPUT_THREAD_STATE.event_port_item);
  g_CONTROL_INPUT_THREAD_STATE.broker_message_item =
    CreateMsg(NULL, 0, g_CONTROL_INPUT_THREAD_STATE.event_port_item);
  if(g_CONTROL_INPUT_THREAD_STATE.broker_message_item < 0)
    {
      PrintfSysErr(g_CONTROL_INPUT_THREAD_STATE.broker_message_item);
      return 0;
    }

  configuration_request.flavor   = EB_Configure;
  configuration_request.category = LC_Observer;
  memset(configuration_request.trigger_mask, 0, sizeof(configuration_request.trigger_mask));
  memset(configuration_request.capture_mask, 0, sizeof(configuration_request.capture_mask));
  configuration_request.queue_max = JOYSTICK_EVENT_QUEUE_MAX;
  /* The oracle transmits the untouched reserved tail of this stack object. */
  status = SendMsg(event_broker_port,
                   g_CONTROL_INPUT_THREAD_STATE.broker_message_item,
                   &configuration_request,
                   sizeof(configuration_request));
  if(status < 0)
    {
      PrintfSysErr(status);
      return 0;
    }

  return 1;
}

int32
receive_joystick_event_broker_messages(void)
{
  Message *message;
  Item     message_item;
  uint32   filtered_buttons;
  uint32   received_signals;

  if(g_CONTROL_INPUT_THREAD_STATE.enabled != 0)
    {
      received_signals = WaitSignal(g_CONTROL_INPUT_THREAD_STATE.event_port->mp_Signal);
      if((received_signals & g_CONTROL_INPUT_THREAD_STATE.event_port->mp_Signal) == 0)
        return 0;
    }

  while(1)
    {
      message_item = GetMsg(g_CONTROL_INPUT_THREAD_STATE.event_port_item);
      if(message_item < 0)
        {
          PrintfSysErr(message_item);
          return 0;
        }
      if(message_item == 0)
        break;

      message = (Message *) LookupItem(message_item);
      if(message_item == g_CONTROL_INPUT_THREAD_STATE.broker_message_item)
        {
          if((int32) message->msg_Result < 0)
            {
              PrintfSysErr((Item) message->msg_Result);
              return 0;
            }
        }
      else
        {
          copy_matching_record_values((const RecordValueGroup *) message->msg_DataPtr);
          (void) ReplyMsg(message_item, 0, NULL, 0);
        }
    }

  filtered_buttons = ((g_CONTROL_INPUT_THREAD_STATE.suppressed_buttons ^
                       g_CURRENT_RECORD_VALUES.values[JOYSTICK_RECORD_BUTTONS_INDEX]) &
                      g_CURRENT_RECORD_VALUES.values[JOYSTICK_RECORD_BUTTONS_INDEX]);
  g_CURRENT_RECORD_VALUES.values[JOYSTICK_RECORD_BUTTONS_INDEX] = filtered_buttons;
  g_CONTROL_INPUT_THREAD_STATE.suppressed_buttons               = 0;
  g_JOYSTICK_CALIBRATION_STATE.current_x =
    g_CURRENT_RECORD_VALUES.values[JOYSTICK_RECORD_HORIZONTAL_INDEX];
  g_JOYSTICK_CALIBRATION_STATE.current_y =
    g_CURRENT_RECORD_VALUES.values[JOYSTICK_RECORD_VERTICAL_INDEX];
  g_JOYSTICK_CALIBRATION_STATE.buttons = filtered_buttons;

  return 1;
}

void
calibrate_joystick_input(void)
{
  g_CONTROL_INPUT_THREAD_STATE.output_enabled = 0;
  _reset_joystick_calibration_extrema();
  (void) receive_joystick_event_broker_messages();
  _wait_for_joystick_button_release();

  while((g_JOYSTICK_CALIBRATION_STATE.buttons & ControlDown) == 0)
    {
      (void) send_control_input_timer_offset_request(1);
      (void) receive_joystick_event_broker_messages();
      _record_joystick_calibration_extrema();
    }

  (void) receive_joystick_event_broker_messages();
  g_JOYSTICK_CALIBRATION_STATE.x.center = g_JOYSTICK_CALIBRATION_STATE.current_x;
  g_JOYSTICK_CALIBRATION_STATE.y.center = g_JOYSTICK_CALIBRATION_STATE.current_y;
  calculate_joystick_calibration_scales();
  g_CONTROL_INPUT_THREAD_STATE.output_enabled = 1;
  (void) receive_joystick_event_broker_messages();
  _wait_for_joystick_button_release();
}

void
poll_joystick_control_input(void)
{
  (void) receive_joystick_event_broker_messages();
  if((g_JOYSTICK_CALIBRATION_STATE.buttons & JOYSTICK_CONTROL_BUTTON_MASK) != 0)
    g_CONTROL_INPUT_THREAD_STATE.output_enabled = 1;

  if(g_JOYSTICK_CALIBRATION_STATE.recalibration_requested != 0)
    {
      calibrate_joystick_input();
      g_JOYSTICK_CALIBRATION_STATE.recalibration_requested = 0;
    }

  g_CONTROL_INPUT_THREAD_STATE.raw_buttons = g_JOYSTICK_CALIBRATION_STATE.buttons;
  g_JOYSTICK_AXIS_VALUES[0] = _scale_joystick_axis(&g_JOYSTICK_CALIBRATION_STATE.x,
                                                   g_JOYSTICK_CALIBRATION_STATE.current_x,
                                                   JOYSTICK_X_MINIMUM,
                                                   JOYSTICK_X_MAXIMUM);
  g_JOYSTICK_AXIS_VALUES[1] = _scale_joystick_axis(&g_JOYSTICK_CALIBRATION_STATE.y,
                                                   g_JOYSTICK_CALIBRATION_STATE.current_y,
                                                   JOYSTICK_Y_MINIMUM,
                                                   JOYSTICK_Y_MAXIMUM);
  if(g_CONTROL_INPUT_THREAD_STATE.output_enabled == 0)
    return;

  G_CONTROL_INPUT_BUTTONS = 0;
  if((g_CONTROL_INPUT_THREAD_STATE.raw_buttons & (ControlDown | ControlUp)) != 0)
    G_CONTROL_INPUT_BUTTONS |= ControlA;
  if((g_CONTROL_INPUT_THREAD_STATE.raw_buttons & ControlRight) != 0)
    G_CONTROL_INPUT_BUTTONS |= ControlB;
  if((g_CONTROL_INPUT_THREAD_STATE.raw_buttons & ControlLeft) != 0)
    G_CONTROL_INPUT_BUTTONS |= ControlC;
  if((g_CONTROL_INPUT_THREAD_STATE.raw_buttons & ControlX) != 0)
    G_CONTROL_INPUT_BUTTONS |= ControlStart;
  if((g_CONTROL_INPUT_THREAD_STATE.raw_buttons & ControlRightShift) != 0)
    G_CONTROL_INPUT_BUTTONS |= ControlX;
  if((g_CONTROL_INPUT_THREAD_STATE.raw_buttons & ControlLeftShift) != 0)
    G_CONTROL_INPUT_BUTTONS |= ControlLeftShift;
  if((g_CONTROL_INPUT_THREAD_STATE.raw_buttons & ControlStart) != 0)
    G_CONTROL_INPUT_BUTTONS |= ControlRightShift;

  if(g_JOYSTICK_AXIS_VALUES[0] < -JOYSTICK_AXIS_DIGITAL_THRESHOLD)
    G_CONTROL_INPUT_BUTTONS |= ControlLeft;
  else if(g_JOYSTICK_AXIS_VALUES[0] > JOYSTICK_AXIS_DIGITAL_THRESHOLD)
    G_CONTROL_INPUT_BUTTONS |= ControlRight;
  if(g_JOYSTICK_AXIS_VALUES[1] < -JOYSTICK_AXIS_DIGITAL_THRESHOLD)
    G_CONTROL_INPUT_BUTTONS |= ControlUp;
  else if(g_JOYSTICK_AXIS_VALUES[1] > JOYSTICK_AXIS_DIGITAL_THRESHOLD)
    G_CONTROL_INPUT_BUTTONS |= ControlDown;
}

void
run_control_input_worker(void)
{
  uint32 received_signals;

  initialize_joystick_calibration_state();
  (void) initialize_joystick_event_broker();
  g_CONTROL_INPUT_THREAD_STATE.signal_mask = AllocSignal(0);
  if(g_CONTROL_INPUT_THREAD_STATE.signal_mask == 0)
    return;

  g_CONTROL_INPUT_THREAD_STATE.start_complete   = 1;
  g_CONTROL_INPUT_THREAD_STATE.timer_io_request = GetVBLIOReq();
  g_CONTROL_INPUT_THREAD_STATE.run_requested    = 1;
  while(g_CONTROL_INPUT_THREAD_STATE.run_requested != 0)
    {
      if(g_CONTROL_INPUT_THREAD_STATE.enabled != 0)
        {
          poll_joystick_control_input();
        }
      else
        {
          received_signals = WaitSignal(g_CONTROL_INPUT_THREAD_STATE.signal_mask);
          if((received_signals & g_CONTROL_INPUT_THREAD_STATE.signal_mask) != 0)
            poll_joystick_control_input();
        }
    }
}

int32
initialize_control_input_system(void)
{
  g_CONTROL_INPUT_THREAD_STATE.start_complete = 0;
  g_CONTROL_INPUT_THREAD_STATE.thread_item =
    create_task_with_allocated_stack(run_control_input_worker,
                                     CONTROL_INPUT_WORKER_STACK_BYTES,
                                     CONTROL_INPUT_WORKER_PRIORITY,
                                     CONTROL_INPUT_WORKER_NAME,
                                     &g_CONTROL_INPUT_THREAD_STATE.stack_allocation);
  while(g_CONTROL_INPUT_THREAD_STATE.start_complete == 0)
    {
    }

  return g_CONTROL_INPUT_THREAD_STATE.thread_item != 0;
}

void
disable_control_input_thread(void)
{
  g_CONTROL_INPUT_THREAD_STATE.enabled = 0;
  SendSignal(g_CONTROL_INPUT_THREAD_STATE.thread_item, g_CONTROL_INPUT_THREAD_STATE.signal_mask);
}

void
enable_control_input_thread(void)
{
  g_CONTROL_INPUT_THREAD_STATE.enabled = 1;
  SendSignal(g_CONTROL_INPUT_THREAD_STATE.thread_item, g_CONTROL_INPUT_THREAD_STATE.signal_mask);
}

Err
send_control_input_timer_offset_request(int32 offset_)
{
  return send_io_offset_request(g_CONTROL_INPUT_THREAD_STATE.timer_io_request, offset_);
}

int32
read_forced_control_pad(int32 control_pad_number_, int32 wait_, ControlPadEventData *event_data_)
{
  (void) control_pad_number_;
  (void) wait_;
  event_data_->cped_ButtonBits = G_FORCED_CONTROL_PAD_BUTTONS;
  return 0;
}

uint32
translate_control_pad_buttons(uint32 portfolio_buttons_)
{
  uint32 game_buttons;

  game_buttons = 0;
  if((portfolio_buttons_ & ControlLeft) != 0)
    game_buttons |= GAME_CONTROL_LEFT;
  if((portfolio_buttons_ & ControlRight) != 0)
    game_buttons |= GAME_CONTROL_RIGHT;
  if((portfolio_buttons_ & ControlUp) != 0)
    game_buttons |= GAME_CONTROL_UP;
  if((portfolio_buttons_ & ControlDown) != 0)
    game_buttons |= GAME_CONTROL_DOWN;
  if((portfolio_buttons_ & ControlA) != 0)
    game_buttons |= GAME_CONTROL_A;
  if((portfolio_buttons_ & ControlB) != 0)
    game_buttons |= GAME_CONTROL_B;
  if((portfolio_buttons_ & ControlC) != 0)
    game_buttons |= GAME_CONTROL_C;
  if((portfolio_buttons_ & ControlStart) != 0)
    game_buttons |= GAME_CONTROL_START;
  if((portfolio_buttons_ & ControlX) != 0)
    game_buttons |= GAME_CONTROL_X;
  if((portfolio_buttons_ & ControlRightShift) != 0)
    game_buttons |= GAME_CONTROL_RIGHT_SHIFT;
  if((portfolio_buttons_ & ControlLeftShift) != 0)
    game_buttons |= GAME_CONTROL_LEFT_SHIFT;

  return game_buttons;
}

void
initialize_control_pads(int32 control_pad_count_)
{
  if(g_CONTROL_PAD_READER != NULL)
    return;

  g_CONTROL_PAD_COUNT = control_pad_count_;
  InitEventUtility(control_pad_count_, 0, 0);
  g_CONTROL_PAD_READER = GetControlPad;
}

uint32
read_control_pad_buttons(int32 control_pad_index_)
{
  ControlPadEventData event_data;

  if(g_CONTROL_PAD_READER == NULL)
    return 0;

  g_CONTROL_PAD_READER(control_pad_index_ + 1, 0, &event_data);
  return translate_control_pad_buttons(event_data.cped_ButtonBits);
}

uint32
read_primary_control_pad_buttons(void)
{
  return read_control_pad_buttons(0);
}

static int32
_scale_joystick_axis(const JoystickAxisCalibration *calibration_,
                     int32                          current_,
                     int32                          negative_limit_,
                     int32                          positive_limit_)
{
  int32 axis_delta;
  int32 scaled_axis;

  axis_delta = current_ - calibration_->center;
  /* ARM60 signed multiplication wraps before the fixed-point shift. */
  if(axis_delta < 0)
    {
      scaled_axis = (((calibration_->negative_scale * axis_delta) * JOYSTICK_AXIS_SCALE) >>
                     JOYSTICK_AXIS_FIXED_SHIFT) +
                    JOYSTICK_AXIS_NEGATIVE_OFFSET;
      if(scaled_axis > 0)
        return 0;
      if(scaled_axis < negative_limit_)
        return negative_limit_;

      return scaled_axis;
    }

  scaled_axis = (((calibration_->positive_scale * axis_delta) * JOYSTICK_AXIS_SCALE) >>
                 JOYSTICK_AXIS_FIXED_SHIFT) -
                JOYSTICK_AXIS_POSITIVE_OFFSET;
  if(scaled_axis < 0)
    return 0;
  if(scaled_axis > positive_limit_)
    return positive_limit_;

  return scaled_axis;
}

static void
_reset_joystick_calibration_extrema(void)
{
  g_JOYSTICK_CALIBRATION_STATE.x.minimum        = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.x.center         = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.x.maximum        = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.x.negative_scale = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.x.positive_scale = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.y.minimum        = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.y.center         = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.y.maximum        = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.y.negative_scale = JOYSTICK_CALIBRATION_SENTINEL;
  g_JOYSTICK_CALIBRATION_STATE.y.positive_scale = JOYSTICK_CALIBRATION_SENTINEL;
}

static void
_record_joystick_calibration_extrema(void)
{
  if(g_JOYSTICK_CALIBRATION_STATE.current_x < g_JOYSTICK_CALIBRATION_STATE.x.minimum)
    g_JOYSTICK_CALIBRATION_STATE.x.minimum = g_JOYSTICK_CALIBRATION_STATE.current_x;
  if(g_JOYSTICK_CALIBRATION_STATE.current_x > g_JOYSTICK_CALIBRATION_STATE.x.maximum)
    g_JOYSTICK_CALIBRATION_STATE.x.maximum = g_JOYSTICK_CALIBRATION_STATE.current_x;
  if(g_JOYSTICK_CALIBRATION_STATE.current_y < g_JOYSTICK_CALIBRATION_STATE.y.minimum)
    g_JOYSTICK_CALIBRATION_STATE.y.minimum = g_JOYSTICK_CALIBRATION_STATE.current_y;
  if(g_JOYSTICK_CALIBRATION_STATE.current_y > g_JOYSTICK_CALIBRATION_STATE.y.maximum)
    g_JOYSTICK_CALIBRATION_STATE.y.maximum = g_JOYSTICK_CALIBRATION_STATE.current_y;
}

static void
_wait_for_joystick_button_release(void)
{
  while((g_JOYSTICK_CALIBRATION_STATE.buttons & JOYSTICK_CONTROL_BUTTON_MASK) != 0)
    {
      (void) send_control_input_timer_offset_request(1);
      (void) receive_joystick_event_broker_messages();
    }
}
