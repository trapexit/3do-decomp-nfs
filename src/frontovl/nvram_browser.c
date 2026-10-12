#include "nvram_browser.h"

#include "controller_input.h"
#include "directory_veneers.h"
#include "frontend_frame.h"
#include "post_race_legacy.h"
#include "runtime_memory.h"
#include "runtime_string.h"
#include "sdk_veneers.h"
#include "tracked_memory.h"

#include "filefunctions.h"

#define NVRAM_DIRECTORY_PATH "/nvram"
#define NVRAM_ENTRY_PATH_FORMAT "/nvram/%s"
#define NVRAM_ENTRY_NAME_BYTE_COUNT 256
#define NVRAM_ENTRY_COPY_BYTE_COUNT 255
#define NVRAM_ENTRY_LIMIT 201
#define NVRAM_DELETE_PATH_BYTE_COUNT 80
#define NVRAM_BROWSER_SHAPE_PATH "frontend/display/nvram.3sh"
#define NVRAM_BROWSER_BACKGROUND_SHAPE "bgnd"
#define NVRAM_BROWSER_SELECTOR_SHAPE "car "
#define NVRAM_VISIBLE_ENTRY_COUNT 9
#define NVRAM_VISIBLE_NAME_BYTE_COUNT 25
#define NVRAM_TEXT_BUFFER_BYTE_COUNT 32
#define NVRAM_TEXT_X 57
#define NVRAM_TEXT_FIRST_Y 70
#define NVRAM_SELECTOR_X 20
#define NVRAM_SELECTOR_FIRST_Y 67
#define NVRAM_ENTRY_LINE_HEIGHT 12
#define NVRAM_SHAPE_POSITION_WORD 3
#define NVRAM_SHAPE_X_SHIFT 16
#define NVRAM_BROWSER_ALLOCATION_BYTE_COUNT 0xC900
#define NVRAM_BROWSER_INPUT_FRAME_LIMIT 3600
#define NVRAM_BROWSER_INPUT_MASK 0xCF800000
#define NVRAM_BROWSER_CANCEL_BUTTON 0x01000000
#define NVRAM_BROWSER_EXIT_BUTTON 0x00800000
#define NVRAM_BROWSER_ACTION_MASK 0x0E000000
#define NVRAM_BROWSER_DELETE_ACTION 0x0A000000
#define NVRAM_BROWSER_UP_BUTTON 0x40000000
#define NVRAM_BROWSER_DOWN_BUTTON 0x80000000
#define NVRAM_BROWSER_LAST_VISIBLE_OFFSET 8

void *g_NVRAM_BROWSER_SHAPES;
char *g_NVRAM_ENTRY_NAMES;
int32 g_NVRAM_BROWSER_ACTIVE;

void
draw_nvram_browser(int32 entry_count_, int32 selected_index_, int32 first_visible_index_)
{
  char  text[NVRAM_TEXT_BUFFER_BYTE_COUNT];
  char *text_end;
  void *background_shape;
  void *selector_shape;
  int32 end_index;
  int32 entry_index;

  initialize_frontend_display();
  if(g_NVRAM_BROWSER_SHAPES == NULL)
    {
      g_NVRAM_BROWSER_SHAPES = load_runtime_file_data(NVRAM_BROWSER_SHAPE_PATH, 0);
      track_primary_allocation(g_NVRAM_BROWSER_SHAPES);
    }
  begin_runtime_frame();
  finish_runtime_frame();
  clear_runtime_display(0);
  background_shape =
    locate_runtime_shape(g_NVRAM_BROWSER_SHAPES, (const uint32 *) NVRAM_BROWSER_BACKGROUND_SHAPE);
  blit_runtime_shape_at(background_shape,
                        ((int32 *) background_shape)[NVRAM_SHAPE_POSITION_WORD] >>
                          NVRAM_SHAPE_X_SHIFT,
                        (int16) ((int32 *) background_shape)[NVRAM_SHAPE_POSITION_WORD]);

  text_end  = &text[NVRAM_VISIBLE_NAME_BYTE_COUNT];
  end_index = first_visible_index_ + NVRAM_VISIBLE_ENTRY_COUNT;
  if(entry_count_ < end_index)
    {
      end_index = entry_count_;
    }
  *text_end = 0;
  for(entry_index = first_visible_index_; entry_index < end_index; ++entry_index)
    {
      copy_runtime_string_n(text,
                            g_NVRAM_ENTRY_NAMES + entry_index * NVRAM_ENTRY_NAME_BYTE_COUNT,
                            NVRAM_VISIBLE_NAME_BYTE_COUNT);
      draw_runtime_text(text,
                        NVRAM_TEXT_X,
                        NVRAM_TEXT_FIRST_Y +
                          (entry_index - first_visible_index_) * NVRAM_ENTRY_LINE_HEIGHT);
    }

  selector_shape =
    locate_runtime_shape(g_NVRAM_BROWSER_SHAPES, (const uint32 *) NVRAM_BROWSER_SELECTOR_SHAPE);
  blit_runtime_shape_at(selector_shape,
                        NVRAM_SELECTOR_X,
                        NVRAM_SELECTOR_FIRST_Y +
                          (selected_index_ - first_visible_index_) * NVRAM_ENTRY_LINE_HEIGHT);
  finish_runtime_frame();
  present_runtime_frame();
}

int32
read_nvram_directory(void)
{
  DirectoryEntry entry;
  Directory     *directory;
  char          *entry_name;
  int32          entry_count;

  entry_count = 0;
  directory   = open_runtime_directory(NVRAM_DIRECTORY_PATH);
  while(read_runtime_directory(directory, &entry) >= 0)
    {
      entry_name = g_NVRAM_ENTRY_NAMES + entry_count * NVRAM_ENTRY_NAME_BYTE_COUNT;
      copy_runtime_string_n(entry_name, entry.de_FileName, NVRAM_ENTRY_COPY_BYTE_COUNT);
      entry_name[NVRAM_ENTRY_COPY_BYTE_COUNT] = 0;
      ++entry_count;
      if(entry_count == NVRAM_ENTRY_LIMIT)
        {
          break;
        }
    }
  CLOSE_DIRECTORY_VENEER(directory);
  return entry_count;
}

int32
delete_nvram_entry(int32 entry_index_)
{
  char path[NVRAM_DELETE_PATH_BYTE_COUNT];

  runtime_format_string(path,
                        NVRAM_ENTRY_PATH_FORMAT,
                        g_NVRAM_ENTRY_NAMES + entry_index_ * NVRAM_ENTRY_NAME_BYTE_COUNT);
  return DeleteFile(path);
}

int32
run_nvram_recovery_browser(void)
{
  uint32 control_bits;
  int32  deleted_entry;
  int32  entry_count;
  int32  first_visible_index;
  int32  selected_index;

  deleted_entry = 0;
  if(g_NVRAM_BROWSER_ACTIVE != 0)
    {
      return -1;
    }

  g_NVRAM_ENTRY_NAMES = allocate_primary_memory(NVRAM_BROWSER_ALLOCATION_BYTE_COUNT);
  entry_count         = read_nvram_directory();
  if(entry_count == 0)
    {
      free_primary_memory(g_NVRAM_ENTRY_NAMES);
      return 1;
    }

  selected_index      = 0;
  first_visible_index = 0;
  for(;;)
    {
      draw_nvram_browser(entry_count, selected_index, first_visible_index);
      control_bits =
        poll_controller_input(NVRAM_BROWSER_INPUT_FRAME_LIMIT, 1, NVRAM_BROWSER_INPUT_MASK);
      if((control_bits & NVRAM_BROWSER_CANCEL_BUTTON) != 0)
        {
          free_primary_memory(g_NVRAM_ENTRY_NAMES);
          free_primary_memory(g_NVRAM_BROWSER_SHAPES);
          g_NVRAM_BROWSER_SHAPES = NULL;
          return 1;
        }
      if((control_bits & NVRAM_BROWSER_EXIT_BUTTON) != 0)
        {
          free_primary_memory(g_NVRAM_ENTRY_NAMES);
          free_primary_memory(g_NVRAM_BROWSER_SHAPES);
          g_NVRAM_BROWSER_SHAPES = NULL;
          g_NVRAM_BROWSER_ACTIVE = 1;
          return deleted_entry;
        }

      if((control_bits & NVRAM_BROWSER_ACTION_MASK) == NVRAM_BROWSER_DELETE_ACTION)
        {
          delete_nvram_entry(selected_index);
          deleted_entry = 1;
          entry_count   = read_nvram_directory();
          if(selected_index >= entry_count && selected_index > 0)
            {
              --selected_index;
            }
          if(entry_count - first_visible_index <= NVRAM_BROWSER_LAST_VISIBLE_OFFSET)
            {
              first_visible_index = entry_count - NVRAM_VISIBLE_ENTRY_COUNT;
              if(first_visible_index <= 0)
                {
                  first_visible_index = 0;
                }
            }
          control_bits = 0;
        }

      if((control_bits & NVRAM_BROWSER_UP_BUTTON) != 0)
        {
          if(selected_index > 0)
            {
              --selected_index;
            }
          if(selected_index < first_visible_index)
            {
              first_visible_index = selected_index;
            }
        }
      if((control_bits & NVRAM_BROWSER_DOWN_BUTTON) != 0)
        {
          if(selected_index < entry_count - 1)
            {
              ++selected_index;
            }
          if(selected_index - first_visible_index >= NVRAM_VISIBLE_ENTRY_COUNT)
            {
              first_visible_index = selected_index - NVRAM_BROWSER_LAST_VISIBLE_OFFSET;
            }
        }
    }
}
