#include "test32.h"

#include <stdio.h>


int main( int, char ** argv ) {
   display_char_s v;
   uint16_t v_row[test32_font_MAX_SYMBOL_WIDTH];
   const char * s = argv[1];
   uint32_t c = get_next_utf8_code( &s );
   display_char_init( &v, c, &test32_font, v_row, 0, 0xFFFF );
   bool v_rc;
   do {
     v_rc = display_char_row( &v );
     for ( size_t i = 0; i < (sizeof(uint16_t) * v.m_cols_count); ++i ) {
       ::printf( "%02X",((uint8_t *)v_row)[i] );
     }
     ::printf( "\n" );
   } while ( !v_rc );
   return 0;
}
