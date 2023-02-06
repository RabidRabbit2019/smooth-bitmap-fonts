#include "font_bmp.h"

#ifdef __cplusplus
extern "C" {
#endif

// find symbol desc by it's code
// returns symbol desc index within a_font->m_symbols
static int find_symbol_index( const packed_font_desc_s * a_font, uint32_t a_code ) {
  int l, m, u;
  l = 0;
  u = a_font->m_symbols_count - 1;
  do {
    m = (l + u) / 2;
    if ( a_font->m_symbols[m].m_code == a_code ) {
      // found
      return m;
    }
    if ( a_font->m_symbols[m].m_code > a_code ) {
      // lower part
      u = m - 1;
    } else {
      // upper part
      l = m + 1;
    }
  } while ( l <= u );
  return a_font->m_def_code_idx;
}


// unpack from R5G6R5 to R8, G8, B8
static void unpack_color( rgb_unpacked_s * a_dst, uint16_t a_color ) {
  a_dst->r = (a_color >> 8) & 0xF8;
  a_dst->g = (a_color >> 3) & 0xFC;
  a_dst->b = (a_color << 3) & 0xF8;
}

// pack from R8, G8, B8 to R5G6B5
static uint16_t pack_color( rgb_unpacked_s * a_src ) {
  return (uint16_t)(
      ((a_src->r & 0xF8) << 8)
    | ((a_src->g & 0xFC) << 3)
    | ((a_src->b & 0xF8) >> 3)
    );
}

// prepare to display symbol, init a_data structure
void display_char_init( display_char_s * a_data, uint32_t a_code, const packed_font_desc_s * a_font, uint16_t * a_dst_row, uint16_t a_bgcolor, uint16_t a_fgcolor ) {
  a_data->m_font = a_font;
  a_data->m_symbol = &(a_font->m_symbols[find_symbol_index(a_font, a_code)]);
  a_data->m_bmp_ptr = a_font->m_bmp + a_data->m_symbol->m_offset;
  a_data->m_curr_nibble = a_data->m_symbol->m_nibble;
  a_data->m_row = 0;
  a_data->m_pixbuf = a_dst_row;
  a_data->m_cols_count = a_data->m_symbol->m_x_advance;
  a_data->m_counter = 0;
  a_data->m_curr_color = 0;
  a_data->m_curr_byte = *a_data->m_bmp_ptr++;
  a_data->m_last_row = a_data->m_symbol->m_y_offset + a_data->m_symbol->m_height;
  a_data->m_last_col = a_data->m_symbol->m_x_offset + a_data->m_symbol->m_width;
  // gen colors table
  rgb_unpacked_s v_rgb_bg;
  rgb_unpacked_s v_rgb_fg;
  rgb_unpacked_s v_rgb;
  unpack_color( &(v_rgb_bg), a_bgcolor );
  unpack_color( &(v_rgb_fg), a_fgcolor );
  for ( int i = 0; i < 8; ++i ) {
    v_rgb.r = ((v_rgb_bg.r * (7 - i)) / 7)
            + ((v_rgb_fg.r * i) / 7)
            ;
    v_rgb.g = ((v_rgb_bg.g * (7 - i)) / 7)
            + ((v_rgb_fg.g * i) / 7)
            ;
    v_rgb.b = ((v_rgb_bg.b * (7 - i)) / 7)
            + ((v_rgb_fg.b * i) / 7)
            ;
    a_data->m_colors[i] = pack_color( &v_rgb );
  }
}


// prepare to display symbol, usign existing font, colors and buffer
void display_char_init2( display_char_s * a_data, uint32_t a_code ) {
  a_data->m_symbol = &(a_data->m_font->m_symbols[find_symbol_index(a_data->m_font, a_code)]);
  a_data->m_bmp_ptr = a_data->m_font->m_bmp + a_data->m_symbol->m_offset;
  a_data->m_curr_nibble = a_data->m_symbol->m_nibble;
  a_data->m_row = 0;
  a_data->m_cols_count = a_data->m_symbol->m_x_advance;
  a_data->m_counter = 0;
  a_data->m_curr_color = 0;
  a_data->m_curr_byte = *a_data->m_bmp_ptr++;
}


// prepare one row pixels buffer, returns 0 (zero), if it was last row
bool display_char_row( display_char_s * a_data ) {
  uint16_t * a_dst = a_data->m_pixbuf;
  // first fill background for н offset
  if ( a_data->m_row < a_data->m_symbol->m_y_offset ) {
    for ( int i = 0; i < a_data->m_symbol->m_x_advance; ++i ) {
      *a_dst++ = a_data->m_colors[0];
    }
  } else {
    if ( a_data->m_row < a_data->m_last_row ) {
      int v_col = 0;
      // first fill background for x offset
      for ( ; v_col < a_data->m_symbol->m_x_offset; ++v_col ) {
        *a_dst++ = a_data->m_colors[0];
      }
      // next check counter
      if ( a_data->m_counter > 0 ) {
        // fill repeated color
        for ( ; a_data->m_counter > 0 && v_col < a_data->m_last_col; --a_data->m_counter, ++v_col ) {
          *a_dst++ = a_data->m_colors[a_data->m_curr_color];
        }
      }
      // next pixels
      for ( ; v_col < a_data->m_last_col; ++v_col ) {
        uint8_t v_packed_color;
        if ( a_data->m_curr_nibble ) {
          // low nibble
          v_packed_color = a_data->m_curr_byte & 0x0F;
          a_data->m_curr_nibble = false;
          a_data->m_curr_byte = *a_data->m_bmp_ptr++;
        } else {
          // high nibble
          v_packed_color = (a_data->m_curr_byte >> 4) & 0x0F;
          a_data->m_curr_nibble = true;
        }
        // it is color or repeat?
        if ( 0 == (v_packed_color & 0x08) ) {
          a_data->m_curr_color = v_packed_color;
          *a_dst++ = a_data->m_colors[v_packed_color];
        } else {
          a_data->m_counter = (v_packed_color & 0x07) + 1;
          for ( ; a_data->m_counter > 0 && v_col < a_data->m_last_col; --a_data->m_counter, ++v_col ) {
            *a_dst++ = a_data->m_colors[a_data->m_curr_color];
          }
          --v_col;
        }
      }
      // background color up to x_advance
      for ( ; v_col < a_data->m_symbol->m_x_advance; ++v_col ) {
        *a_dst++ = a_data->m_colors[0];
      }
    } else {
      // bottom space
      for ( int i = 0; i < a_data->m_symbol->m_x_advance; ++i ) {
        *a_dst++ = a_data->m_colors[0];
      }
    }
  }
  //
  return ++a_data->m_row >= a_data->m_font->m_row_height;
}


#ifdef __cplusplus
}
#endif
