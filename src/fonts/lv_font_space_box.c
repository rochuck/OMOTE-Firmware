/*******************************************************************************
 * Single-glyph LVGL font: U+2423 OPEN BOX (␣), the "space" symbol.
 *
 * Hand-authored to match lv_font_montserrat_16 (bpp 4, line_height 18,
 * base_line 3) so it can be chained as a `.fallback` font and render the
 * space glyph on the T9 keypad. See src/guis/gui_t9.cpp.
 *
 * The glyph is a wide, shallow open box (flat bottom with two short sides) so
 * it reads as a squared-off "u". 10 px wide => byte-aligned rows at 4 bpp.
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl.h"
#endif

/*-----------------
 *    BITMAP
 *----------------*/

/* 10 px wide x 5 px tall, 4 bpp. 0xF = opaque, 0x0 = transparent.
 * Each row is 10 nibbles = 5 bytes (byte aligned).
 *   F . . . . . . . . F   <- left/right sides
 *   F . . . . . . . . F
 *   F . . . . . . . . F
 *   F . . . . . . . . F
 *   F F F F F F F F F F   <- bottom bar
 */
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+2423 "␣" */
    0xf0, 0x00, 0x00, 0x00, 0x0f,
    0xf0, 0x00, 0x00, 0x00, 0x0f,
    0xf0, 0x00, 0x00, 0x00, 0x0f,
    0xf0, 0x00, 0x00, 0x00, 0x0f,
    0xff, 0xff, 0xff, 0xff, 0xff,
};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0,   .box_w = 0,  .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 192, .box_w = 10, .box_h = 5, .ofs_x = 1, .ofs_y = 0} /* U+2423 */,
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 0x2423, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,
        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,
    },
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LV_VERSION_CHECK(8, 0, 0)
static lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

#if LV_VERSION_CHECK(8, 0, 0)
const lv_font_t lv_font_space_box = {
#else
lv_font_t lv_font_space_box = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 18,
    .base_line = 3,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,
};
