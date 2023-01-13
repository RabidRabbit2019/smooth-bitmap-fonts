#ifndef __FONT_BMP_H__
#define __FONT_BMP_H__

#include <inttypes.h>
#include <stdbool.h>


// symbol description
typedef struct {
  uint32_t m_code;       // code
  uint16_t m_x;          // x-coord in font bitmat
  uint16_t m_y;          // y-coord in font bitmap
  uint8_t m_width;       // width in pixels of symbol bitmap
  uint8_t m_height;      // height in pixels of symbol bitmap
  uint8_t m_x_offset;    // x offset for display symbol
  uint8_t m_y_offset;    // y offset for display symbol
  uint8_t m_x_advance;   // displayed width of symbol
} symbol_desc_s;


// font description
typedef struct {
  int m_bmp_width;                  // font bmp width
  int m_bmp_height;                 // font bmp height
  const uint8_t * m_bmp;            // font bmp ptr
  int m_symbols_count;              // total symbols
  int m_row_height;                 // text row height
  int m_def_code_idx;               // default symbol index, if symbol code not found
  const struct symbol_desc_s * m_symbols; // descriptions of symbols ptr
} font_desc_s;


typedef struct {
  int r;
  int g;
  int b;
} rgb_unpacked_s;

// display char structure
typedef struct {
  const struct font_desc_s * m_font;      // font desc ptr
  const struct symbol_desc_s * m_symbol;  // symbol desc ptr
  const uint8_t * m_bmp_ptr;              // symbol ptr within font bmp
  int m_row;                              // current row to display
  uint16_t * m_pixbuf;                    // dst pixels row
  int m_cols_count;                       // width of pixels for symbol place
  struct rgb_unpacked_s m_bgcolor;        // background colour
  struct rgb_unpacked_s m_fgcolor;        // foreground colour
} display_char_s;


// prepare to display symbol, init a_data structure
void display_char_init( display_char_s * a_data, uint32_t a_code, const struct font_desc_s * a_font, uint16_t * a_dst_row, uint16_t a_bgcolor, uint16_t a_fgcolor );

// prepare one row pixels buffer, returns true, if it was last row
bool display_char_row( display_char_s * a_data );

#endif // __FONT_BMP_H__