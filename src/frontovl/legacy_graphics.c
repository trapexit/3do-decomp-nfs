#include "sdk_veneers.h"

/* LaunchMe retains the SDK name for the same graphics-folio veneer. */
Err
DrawCels(Item bitmap_item_, CCB *cel_);

enum
{
  LEGACY_GRAPHICS_MAP_CEL_OFFSET              = 0x04,
  LEGACY_GRAPHICS_READ_PIXEL_OFFSET           = 0x08,
  LEGACY_GRAPHICS_READ_CLUT_COLOR_OFFSET      = 0x0C,
  LEGACY_GRAPHICS_GET_PIXEL_ADDRESS_OFFSET    = 0x10,
  LEGACY_GRAPHICS_WRITE_PIXEL_OFFSET          = 0x14,
  LEGACY_GRAPHICS_DRAW_TEXT_16_OFFSET         = 0x24,
  LEGACY_GRAPHICS_RESET_CURRENT_FONT_OFFSET   = 0x2C,
  LEGACY_GRAPHICS_CREATE_SCREEN_GROUP_OFFSET  = 0x30,
  LEGACY_GRAPHICS_SET_READ_ADDRESS_OFFSET     = 0x34,
  LEGACY_GRAPHICS_RESET_READ_ADDRESS_OFFSET   = 0x38,
  LEGACY_GRAPHICS_SET_CLIP_ORIGIN_OFFSET      = 0x3C,
  LEGACY_GRAPHICS_ENABLE_VAVG_OFFSET          = 0x40,
  LEGACY_GRAPHICS_DISABLE_VAVG_OFFSET         = 0x44,
  LEGACY_GRAPHICS_ENABLE_HAVG_OFFSET          = 0x48,
  LEGACY_GRAPHICS_DISABLE_HAVG_OFFSET         = 0x4C,
  LEGACY_GRAPHICS_SET_SCREEN_COLOR_OFFSET     = 0x50,
  LEGACY_GRAPHICS_RESET_SCREEN_COLORS_OFFSET  = 0x54,
  LEGACY_GRAPHICS_SET_SCREEN_COLORS_OFFSET    = 0x58,
  LEGACY_GRAPHICS_DELETE_SCREEN_GROUP_OFFSET  = 0x5C,
  LEGACY_GRAPHICS_SET_FG_PEN_OFFSET           = 0x60,
  LEGACY_GRAPHICS_SET_BG_PEN_OFFSET           = 0x64,
  LEGACY_GRAPHICS_ADD_SCREEN_GROUP_OFFSET     = 0x68,
  LEGACY_GRAPHICS_REMOVE_SCREEN_GROUP_OFFSET  = 0x6C,
  LEGACY_GRAPHICS_SET_CLIP_WIDTH_OFFSET       = 0x70,
  LEGACY_GRAPHICS_SET_CLIP_HEIGHT_OFFSET      = 0x74,
  LEGACY_GRAPHICS_MOVE_TO_OFFSET              = 0x78,
  LEGACY_GRAPHICS_DRAW_CHAR_OFFSET            = 0x80,
  LEGACY_GRAPHICS_DRAW_TO_OFFSET              = 0x84,
  LEGACY_GRAPHICS_FILL_RECT_OFFSET            = 0x88,
  LEGACY_GRAPHICS_SET_CURRENT_FONT_CCB_OFFSET = 0x8C,
  LEGACY_GRAPHICS_GET_CURRENT_FONT_OFFSET     = 0x90,
  LEGACY_GRAPHICS_DRAW_TEXT_8_OFFSET          = 0x94,
  LEGACY_GRAPHICS_SET_CE_CONTROL_OFFSET       = 0x98,
  LEGACY_GRAPHICS_DISPLAY_SCREEN_OFFSET       = 0xA0,
  LEGACY_GRAPHICS_SET_VDL_OFFSET              = 0xA4,
  LEGACY_GRAPHICS_SUBMIT_VDL_OFFSET           = 0xA8,
  LEGACY_GRAPHICS_DRAW_CELS_OFFSET            = 0xAC,
  LEGACY_GRAPHICS_DRAW_SCREEN_CELS_OFFSET     = 0xB0,
  LEGACY_GRAPHICS_SET_CE_WATCHDOG_OFFSET      = 0xB4,
  LEGACY_GRAPHICS_MODIFY_VDL_OFFSET           = 0xBC,
  LEGACY_GRAPHICS_QUERY_GRAPHICS_OFFSET       = 0xC0,
  LEGACY_GRAPHICS_QUERY_GRAPHICS_LIST_OFFSET  = 0xC4
};

typedef Err    (*LegacyItemFunction)(Item item_);
typedef Err    (*LegacyTwoItemFunction)(Item item_0_, Item item_1_);
typedef Err    (*LegacyAddScreenGroupFunction)(Item screen_group_, TagArg *tags_);
typedef Err    (*LegacyDrawCharFunction)(GrafCon *graphics_context_,
                                         Item     bitmap_item_,
                                         uint32   character_);
typedef Err    (*LegacyDrawText16Function)(GrafCon *graphics_context_,
                                           Item     bitmap_item_,
                                           uint16  *text_);
typedef Err    (*LegacyDrawText8Function)(GrafCon     *graphics_context_,
                                          Item         bitmap_item_,
                                          const uint8 *text_);
typedef Err    (*LegacyDrawToFunction)(Item     bitmap_item_,
                                       GrafCon *graphics_context_,
                                       Coord    x_,
                                       Coord    y_);
typedef Err    (*LegacyFillRectFunction)(Item         bitmap_item_,
                                         GrafCon     *graphics_context_,
                                         struct Rect *rectangle_);
typedef Font  *(*LegacyGetCurrentFontFunction)(void);
typedef Err    (*LegacySetCEControlFunction)(Item  bitmap_item_,
                                             int32 control_word_,
                                             int32 control_mask_);
typedef Err    (*LegacySetClipOriginFunction)(Item bitmap_item_, int32 x_, int32 y_);
typedef Err    (*LegacySetCurrentFontCCBFunction)(CCB *cel_);
typedef Err    (*LegacySetReadAddressFunction)(Item bitmap_item_, ubyte *buffer_, int32 width_);
typedef Err    (*LegacySetScreenColorsFunction)(Item screen_item_, uint32 *entries_, int32 count_);
typedef Item   (*LegacySubmitVDLFunction)(VDLEntry *vdl_data_, int32 length_, int32 type_);
typedef Err    (*LegacyDrawCelsFunction)(Item item_, CCB *cel_);
typedef Err    (*LegacyModifyVDLFunction)(Item vdl_item_, TagArg *tags_);
typedef Err    (*LegacyResetCurrentFontFunction)(void);
typedef Color  (*LegacyReadPixelFunction)(Item     bitmap_item_,
                                          GrafCon *graphics_context_,
                                          Coord    x_,
                                          Coord    y_);
typedef Err    (*LegacyWritePixelFunction)(Item     bitmap_item_,
                                           GrafCon *graphics_context_,
                                           Coord    x_,
                                           Coord    y_);
typedef void  *(*LegacyGetPixelAddressFunction)(Item screen_item_, Coord x_, Coord y_);
typedef void   (*LegacyMapCelFunction)(CCB *cel_, Point *quad_);
typedef RGB888 (*LegacyReadCLUTColorFunction)(uint32 index_);
typedef Item   (*LegacyCreateScreenGroupFunction)(Item *screen_items_, TagArg *tags_);
typedef Err    (*LegacyDeleteScreenGroupFunction)(Item screen_group_item_);
typedef void   (*LegacyMoveToFunction)(GrafCon *graphics_context_, Coord x_, Coord y_);
typedef void   (*LegacySetPenFunction)(GrafCon *graphics_context_, Color color_);
typedef Err    (*LegacyQueryGraphicsFunction)(int32 tag_, void *result_);
typedef Err    (*LegacyQueryGraphicsListFunction)(TagArg *tags_);

Err
AddScreenGroup(Item screen_group_, TagArg *tags_)
{
  LegacyAddScreenGroupFunction function;

  function = (*(LegacyAddScreenGroupFunction *) ((uint8 *) GrafBase -
                                                 LEGACY_GRAPHICS_ADD_SCREEN_GROUP_OFFSET));
  return function(screen_group_, tags_);
}

Err
DisableHAVG(Item screen_item_)
{
  LegacyItemFunction function;

  function = (*(LegacyItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DISABLE_HAVG_OFFSET));
  return function(screen_item_);
}

Err
DisableVAVG(Item screen_item_)
{
  LegacyItemFunction function;

  function = (*(LegacyItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DISABLE_VAVG_OFFSET));
  return function(screen_item_);
}

Err
DisplayScreen(Item screen_item_0_, Item screen_item_1_)
{
  LegacyTwoItemFunction function;

  function =
    (*(LegacyTwoItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DISPLAY_SCREEN_OFFSET));
  return function(screen_item_0_, screen_item_1_);
}

Err
DrawChar(GrafCon *graphics_context_, Item bitmap_item_, uint32 character_)
{
  LegacyDrawCharFunction function;

  function = (*(LegacyDrawCharFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DRAW_CHAR_OFFSET));
  return function(graphics_context_, bitmap_item_, character_);
}

Err
DrawText16(GrafCon *graphics_context_, Item bitmap_item_, uint16 *text_)
{
  LegacyDrawText16Function function;

  function =
    (*(LegacyDrawText16Function *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DRAW_TEXT_16_OFFSET));
  return function(graphics_context_, bitmap_item_, text_);
}

Err
DrawText8(GrafCon *graphics_context_, Item bitmap_item_, const uint8 *text_)
{
  LegacyDrawText8Function function;

  function =
    (*(LegacyDrawText8Function *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DRAW_TEXT_8_OFFSET));
  return function(graphics_context_, bitmap_item_, text_);
}

Err
DrawTo(Item bitmap_item_, GrafCon *graphics_context_, Coord x_, Coord y_)
{
  LegacyDrawToFunction function;

  function = (*(LegacyDrawToFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DRAW_TO_OFFSET));
  return function(bitmap_item_, graphics_context_, x_, y_);
}

Err
EnableHAVG(Item screen_item_)
{
  LegacyItemFunction function;

  function = (*(LegacyItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_ENABLE_HAVG_OFFSET));
  return function(screen_item_);
}

Err
EnableVAVG(Item screen_item_)
{
  LegacyItemFunction function;

  function = (*(LegacyItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_ENABLE_VAVG_OFFSET));
  return function(screen_item_);
}

Err
FillRect(Item bitmap_item_, GrafCon *graphics_context_, struct Rect *rectangle_)
{
  LegacyFillRectFunction function;

  function = (*(LegacyFillRectFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_FILL_RECT_OFFSET));
  return function(bitmap_item_, graphics_context_, rectangle_);
}

Font *
GetCurrentFont(void)
{
  LegacyGetCurrentFontFunction function;

  function = (*(LegacyGetCurrentFontFunction *) ((uint8 *) GrafBase -
                                                 LEGACY_GRAPHICS_GET_CURRENT_FONT_OFFSET));
  return function();
}

Err
RemoveScreenGroup(Item screen_group_)
{
  LegacyItemFunction function;

  function =
    (*(LegacyItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_REMOVE_SCREEN_GROUP_OFFSET));
  return function(screen_group_);
}

Err
ResetReadAddress(Item bitmap_item_)
{
  LegacyItemFunction function;

  function =
    (*(LegacyItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_RESET_READ_ADDRESS_OFFSET));
  return function(bitmap_item_);
}

Err
ResetScreenColors(Item screen_item_)
{
  LegacyItemFunction function;

  function =
    (*(LegacyItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_RESET_SCREEN_COLORS_OFFSET));
  return function(screen_item_);
}

Err
SetCEControl(Item bitmap_item_, int32 control_word_, int32 control_mask_)
{
  LegacySetCEControlFunction function;

  function =
    (*(LegacySetCEControlFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_CE_CONTROL_OFFSET));
  return function(bitmap_item_, control_word_, control_mask_);
}

Err
SetClipHeight(Item bitmap_item_, int32 clip_height_)
{
  LegacyTwoItemFunction function;

  function =
    (*(LegacyTwoItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_CLIP_HEIGHT_OFFSET));
  return function(bitmap_item_, clip_height_);
}

Err
SetClipOrigin(Item bitmap_item_, int32 x_, int32 y_)
{
  LegacySetClipOriginFunction function;

  function = (*(LegacySetClipOriginFunction *) ((uint8 *) GrafBase -
                                                LEGACY_GRAPHICS_SET_CLIP_ORIGIN_OFFSET));
  return function(bitmap_item_, x_, y_);
}

Err
SetClipWidth(Item bitmap_item_, int32 clip_width_)
{
  LegacyTwoItemFunction function;

  function =
    (*(LegacyTwoItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_CLIP_WIDTH_OFFSET));
  return function(bitmap_item_, clip_width_);
}

Err
SetCurrentFontCCB(CCB *cel_)
{
  LegacySetCurrentFontCCBFunction function;

  function = (*(LegacySetCurrentFontCCBFunction *) ((uint8 *) GrafBase -
                                                    LEGACY_GRAPHICS_SET_CURRENT_FONT_CCB_OFFSET));
  return function(cel_);
}

Err
SetReadAddress(Item bitmap_item_, ubyte *buffer_, int32 width_)
{
  LegacySetReadAddressFunction function;

  function = (*(LegacySetReadAddressFunction *) ((uint8 *) GrafBase -
                                                 LEGACY_GRAPHICS_SET_READ_ADDRESS_OFFSET));
  return function(bitmap_item_, buffer_, width_);
}

Err
SetScreenColor(Item screen_item_, uint32 color_entry_)
{
  LegacyTwoItemFunction function;

  function =
    (*(LegacyTwoItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_SCREEN_COLOR_OFFSET));
  return function(screen_item_, color_entry_);
}

Err
SetScreenColors(Item screen_item_, uint32 *entries_, int32 count_)
{
  LegacySetScreenColorsFunction function;

  function = (*(LegacySetScreenColorsFunction *) ((uint8 *) GrafBase -
                                                  LEGACY_GRAPHICS_SET_SCREEN_COLORS_OFFSET));
  return function(screen_item_, entries_, count_);
}

Err
SetVDL(Item screen_item_, Item vdl_item_)
{
  LegacyTwoItemFunction function;

  function = (*(LegacyTwoItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_VDL_OFFSET));
  return function(screen_item_, vdl_item_);
}

Item
SubmitVDL(VDLEntry *vdl_data_, int32 length_, int32 type_)
{
  LegacySubmitVDLFunction function;

  function =
    (*(LegacySubmitVDLFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SUBMIT_VDL_OFFSET));
  return function(vdl_data_, length_, type_);
}

Err
DrawCels(Item bitmap_item_, CCB *cel_)
{
  LegacyDrawCelsFunction function;

  function = (*(LegacyDrawCelsFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DRAW_CELS_OFFSET));
  return function(bitmap_item_, cel_);
}

Err
draw_cels_via_graphics_folio(Item bitmap_item_, CCB *cel_)
{
  LegacyDrawCelsFunction function;

  function = (*(LegacyDrawCelsFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DRAW_CELS_OFFSET));
  return function(bitmap_item_, cel_);
}

Err
DrawScreenCels(Item screen_item_, CCB *cel_)
{
  LegacyDrawCelsFunction function;

  function =
    (*(LegacyDrawCelsFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_DRAW_SCREEN_CELS_OFFSET));
  return function(screen_item_, cel_);
}

Err
SetCEWatchDog(Item bitmap_item_, int32 counter_)
{
  LegacyTwoItemFunction function;

  function =
    (*(LegacyTwoItemFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_CE_WATCHDOG_OFFSET));
  return function(bitmap_item_, counter_);
}

Err
ModifyVDL(Item vdl_item_, TagArg *tags_)
{
  LegacyModifyVDLFunction function;

  function =
    (*(LegacyModifyVDLFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_MODIFY_VDL_OFFSET));
  return function(vdl_item_, tags_);
}

Err
ResetCurrentFont(void)
{
  LegacyResetCurrentFontFunction function;

  function = (*(LegacyResetCurrentFontFunction *) ((uint8 *) GrafBase -
                                                   LEGACY_GRAPHICS_RESET_CURRENT_FONT_OFFSET));
  return function();
}

Color
ReadPixel(Item bitmap_item_, GrafCon *graphics_context_, Coord x_, Coord y_)
{
  LegacyReadPixelFunction function;

  function =
    (*(LegacyReadPixelFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_READ_PIXEL_OFFSET));
  return function(bitmap_item_, graphics_context_, x_, y_);
}

Err
WritePixel(Item bitmap_item_, GrafCon *graphics_context_, Coord x_, Coord y_)
{
  LegacyWritePixelFunction function;

  function =
    (*(LegacyWritePixelFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_WRITE_PIXEL_OFFSET));
  return function(bitmap_item_, graphics_context_, x_, y_);
}

void *
GetPixelAddress(Item screen_item_, Coord x_, Coord y_)
{
  LegacyGetPixelAddressFunction function;

  function = (*(LegacyGetPixelAddressFunction *) ((uint8 *) GrafBase -
                                                  LEGACY_GRAPHICS_GET_PIXEL_ADDRESS_OFFSET));
  return function(screen_item_, x_, y_);
}

void
MapCel(CCB *cel_, Point *quad_)
{
  LegacyMapCelFunction function;

  function = (*(LegacyMapCelFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_MAP_CEL_OFFSET));
  function(cel_, quad_);
}

RGB888
ReadCLUTColor(uint32 index_)
{
  LegacyReadCLUTColorFunction function;

  function = (*(LegacyReadCLUTColorFunction *) ((uint8 *) GrafBase -
                                                LEGACY_GRAPHICS_READ_CLUT_COLOR_OFFSET));
  return function(index_);
}

Item
CreateScreenGroup(Item *screen_items_, TagArg *tags_)
{
  LegacyCreateScreenGroupFunction function;

  function = (*(LegacyCreateScreenGroupFunction *) ((uint8 *) GrafBase -
                                                    LEGACY_GRAPHICS_CREATE_SCREEN_GROUP_OFFSET));
  return function(screen_items_, tags_);
}

Err
DeleteScreenGroup(Item screen_group_item_)
{
  LegacyDeleteScreenGroupFunction function;

  function = (*(LegacyDeleteScreenGroupFunction *) ((uint8 *) GrafBase -
                                                    LEGACY_GRAPHICS_DELETE_SCREEN_GROUP_OFFSET));
  return function(screen_group_item_);
}

void
MoveTo(GrafCon *graphics_context_, Coord x_, Coord y_)
{
  LegacyMoveToFunction function;

  function = (*(LegacyMoveToFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_MOVE_TO_OFFSET));
  function(graphics_context_, x_, y_);
}

void
SetBGPen(GrafCon *graphics_context_, Color color_)
{
  LegacySetPenFunction function;

  function = (*(LegacySetPenFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_BG_PEN_OFFSET));
  function(graphics_context_, color_);
}

void
SetFGPen(GrafCon *graphics_context_, Color color_)
{
  LegacySetPenFunction function;

  function = (*(LegacySetPenFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_SET_FG_PEN_OFFSET));
  function(graphics_context_, color_);
}

Err
QueryGraphics(int32 tag_, void *result_)
{
  LegacyQueryGraphicsFunction function;

  function =
    (*(LegacyQueryGraphicsFunction *) ((uint8 *) GrafBase - LEGACY_GRAPHICS_QUERY_GRAPHICS_OFFSET));
  return function(tag_, result_);
}

Err
QueryGraphicsList(TagArg *tags_)
{
  LegacyQueryGraphicsListFunction function;

  function = (*(LegacyQueryGraphicsListFunction *) ((uint8 *) GrafBase -
                                                    LEGACY_GRAPHICS_QUERY_GRAPHICS_LIST_OFFSET));
  return function(tags_);
}
