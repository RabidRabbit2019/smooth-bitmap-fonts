#include "test_font.h"

#include <stdio.h>


int main( int, char ** argv ) {
   display_char_s v;
   uint16_t v_row[32];
   display_char_init( &v, argv[1][0], &test_font_font, v_row, 0, 0xFFFF );
   bool v_rc;
   do {
     v_rc = display_char_row( &v );
     ::fwrite( v_row, sizeof(uint16_t) * v.m_cols_count, 1, stdout );
   } while ( !v_rc );
   return 0;
}
