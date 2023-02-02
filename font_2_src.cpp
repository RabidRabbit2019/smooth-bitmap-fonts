//
#include <memory>
#include <string>
#include <algorithm>
#include <vector>
#include <map>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <libgen.h>

#include "font_bmp.h"


// font symbol description
struct source_symbol_desc_s{
  int m_code;       // code
  int m_x;          // x-coord in font bitmat
  int m_y;          // y-coord in font bitmap
  int m_width;       // width in pixels of symbol bitmap
  int m_height;      // height in pixels of symbol bitmap
  int m_x_offset;    // x offset for display symbol
  int m_y_offset;    // y offset for display symbol
  int m_x_advance;   // displayed width of symbol
};


// source font desription
struct source_font_desc_s {
  std::vector<uint8_t> m_bmp;   // font image
  int m_bmp_width;              // font symbols bmp width
  int m_bmp_height;             // font symbols bmp height
  int m_symbols_count;          // total symbols
  int m_row_height;             // text row height
  int m_def_code_idx;           // default symbol index, if symbol code not found
  int m_tga_line_bytes;         // bytes in one tga line
  int m_tga_pixel_bytes;        // bytes in one tga pixel (usually 3 or 4)
  std::vector<source_symbol_desc_s> m_symbols; // descriptions of symbols ptr
  source_font_desc_s()
    : m_bmp_width(0)
    , m_bmp_height(0)
    , m_symbols_count(0)
    , m_row_height(0)
    , m_def_code_idx(0)
    , m_tga_line_bytes(0)
    , m_tga_pixel_bytes(0)
    {}
};


#pragma pack(push,1)
struct targaheader_s
{
 uint8_t textsize;
 uint8_t maptype;
 uint8_t datatype;
 int16_t maporg;
 int16_t maplength;
 uint8_t cmapbits;
 int16_t xoffset;
 int16_t yoffset;
 int16_t width;
 int16_t height;
 uint8_t databits;
 uint8_t imtype;
};
#pragma pack(pop)

#define TGA_FOOTER_SIZE     26
#define TGA_DATATYPE        2
#define TGA_DATABITS        24


struct compare_two_char_ptr {
  bool operator () ( const char * const & a1, const char * const & a2 ) const {
    return ::strcmp( a1, a2 ) < 0;
  }
};



//
std::string g_line_cache;
std::map<const char *, const char *, compare_two_char_ptr> g_line_parts;
// load font description from text file and bmp data from targa image file
bool load_font_desc( FILE * a_fp, source_font_desc_s & a_dst );
// write out .h and .c files with packed font
void write_packed_font( FILE * a_out_h, FILE * a_out_c, const source_font_desc_s & a_src );
//
bool parse_line( char * a_src );
//
bool get_value( char * a_src, const char * a_name, std::string & a_dst );
bool get_value( char * a_src, const char * a_name, int & a_dst );


int main( int argc, char ** argv ) {
  if ( 4 != argc ) {
    ::fprintf( stderr, "need a input.txt and output.h with output.c(pp) file names\n" );
    return 1;
  }

  struct stat v_stat;
  ::bzero( &v_stat, sizeof(v_stat) );

  if ( 0 != ::stat( argv[1], &v_stat ) ) {
    ::fprintf( stderr, "can't stat file '%s'\n", argv[1] );
    return 1;
  }
  if ( ((off_t)(sizeof(targaheader_s) + TGA_FOOTER_SIZE)) >= v_stat.st_size ) {
    ::fprintf( stderr, "file '%s' too small for truevision targa\n", argv[1] );
    return 1;
  }


  // font description from https://snowb.org/  
  std::unique_ptr<FILE, int(*)(FILE *)> v_fp_in(::fopen( argv[1], "rb" ), ::fclose);
  if ( !v_fp_in ) {
    ::fprintf( stderr, "can't open file '%s' for read\n", argv[1] );
    return 1;
  }

  std::unique_ptr<FILE, int(*)(FILE *)> v_fp_out_h(::fopen( argv[2], "wb" ), ::fclose);
  if ( !v_fp_out_h ) {
    ::fprintf( stderr, "can't open file '%s' for write\n", argv[2] );
    return 1;
  }

  std::unique_ptr<FILE, int(*)(FILE *)> v_fp_out_c(::fopen( argv[3], "wb" ), ::fclose);
  if ( !v_fp_out_c ) {
    ::fprintf( stderr, "can't open file '%s' for write\n", argv[3] );
    return 1;
  }


  source_font_desc_s v_font_desc;
  if ( !load_font_desc(v_fp_in.get(), v_font_desc) ) {
    ::fprintf( stderr, "error loading font description from file '%s'\n", argv[1] );
    return 1;
  }  

  write_packed_font( v_fp_out_h.get(), v_fp_out_c.get(), v_font_desc );
  return 0;
}


bool get_value( const char * a_src, const char * a_name, int & a_dst );
bool get_value( const char * a_src, const char * a_name, std::string & a_dst );
bool load_tga_file( const char * a_file_name, source_font_desc_s & a_dst );

#define LN_START_COMMON   "common "
#define LN_START_PAGE     "page "
#define LN_START_CHARS    "chars "
#define LN_START_CHAR     "char "

#define RD_ST_COMMON  (1 << 0)
#define RD_ST_PAGE    (1 << 1)
#define RD_ST_CHARS   (1 << 2)
#define RD_ST_ALL     (RD_ST_COMMON|RD_ST_PAGE|RD_ST_CHARS)


bool load_font_desc( FILE * a_fp, source_font_desc_s & a_dst ) {
  char v_line[1024];
  // read "common" line
  int v_info_read_state = 0;
  int v_char_idx = 0;
  while ( ::fgets( v_line, sizeof(v_line), a_fp ) ) {
    if ( 0 == ::strncmp( v_line, LN_START_COMMON, ::strlen(LN_START_COMMON) ) ) {
      if ( 0 != (v_info_read_state & RD_ST_COMMON) ) {
        ::fprintf( stderr, "more than one line with '%s' at begin\n", LN_START_COMMON );
      }
      if ( !get_value( v_line, "lineHeight", a_dst.m_row_height )
        || !get_value( v_line, "scaleW", a_dst.m_bmp_width )
        || !get_value( v_line, "scaleH", a_dst.m_bmp_height ) ) {
        return false;
      }
      v_info_read_state |= RD_ST_COMMON;
      continue;
    }
    if ( 0 == ::strncmp( v_line, LN_START_PAGE, ::strlen(LN_START_PAGE) ) ) {
      if ( 0 != (v_info_read_state & RD_ST_PAGE) ) {
        ::fprintf( stderr, "more than one line with '%s' at begin\n", LN_START_PAGE );
      }
      std::string v_file_name;
      if ( !get_value( v_line, "file", v_file_name ) ) {
        return false;
      }
      if ( !load_tga_file( v_file_name.c_str(), a_dst ) ) {
        return false;
      }
      v_info_read_state |= RD_ST_PAGE;
      continue;
    }
    if ( 0 == ::strncmp( v_line, LN_START_CHARS, ::strlen(LN_START_CHARS) ) ) {
      if ( 0 != (v_info_read_state & RD_ST_CHARS) ) {
        ::fprintf( stderr, "more than one line with '%s' at begin\n", LN_START_CHARS );
      }
      if ( !get_value( v_line, "count", a_dst.m_symbols_count ) ) {
        return false;
      }
      a_dst.m_symbols.resize(a_dst.m_symbols_count);
      v_info_read_state |= RD_ST_CHARS;
      continue;
    }
    if ( 0 == ::strncmp( v_line, LN_START_CHAR, ::strlen(LN_START_CHAR) ) ) {
      if ( RD_ST_ALL != v_info_read_state ) {
        ::fprintf( stderr, "not all font info exists\n" );
        return false;
      }
      if ( v_char_idx >= (int)a_dst.m_symbols.size() ) {
        ::fprintf( stderr, "char definitions more than chars count\n" );
        return false;
      }
      if ( !get_value( v_line, "id", a_dst.m_symbols[v_char_idx].m_code )
        || !get_value( v_line, "x", a_dst.m_symbols[v_char_idx].m_x )
        || !get_value( v_line, "y", a_dst.m_symbols[v_char_idx].m_y )
        || !get_value( v_line, "width", a_dst.m_symbols[v_char_idx].m_width )
        || !get_value( v_line, "height", a_dst.m_symbols[v_char_idx].m_height )
        || !get_value( v_line, "xoffset", a_dst.m_symbols[v_char_idx].m_x_offset )
        || !get_value( v_line, "yoffset", a_dst.m_symbols[v_char_idx].m_y_offset)
        || !get_value( v_line, "xadvance", a_dst.m_symbols[v_char_idx].m_x_advance ) ) {
        return false;
      }
      ++v_char_idx;
    }
  }
  // check and return result
  return ( RD_ST_ALL == v_info_read_state
    && !a_dst.m_symbols.empty()
    && a_dst.m_symbols.size() == (size_t)v_char_idx );
}


void write_packed_font( FILE * a_out_h, FILE * a_out_c, const source_font_desc_s & a_src ) {
  // write out font files
  ::printf( "write font files\n" );
}

bool parse_line( char * a_src ) {
  g_line_parts.clear();
  char * v_ctx = 0;
  char * v_ptr = ::strtok_r( a_src, " ", &v_ctx );
  if ( ! v_ptr ) {
    return false;
  }
  for ( v_ptr = ::strtok_r( 0, " ", &v_ctx ); v_ptr ; v_ptr = ::strtok_r( 0, " ", &v_ctx ) ) {
    char * v_value = ::strchr( v_ptr, '=' );
    if ( v_value ) {
      *v_value++ = 0;
    }
    if ( ::strlen( v_ptr ) && ::strlen( v_value ) ) {
      g_line_parts.emplace( v_ptr, v_value );
    }
  }
  g_line_cache = a_src;
  return true;
}


bool get_value( char * a_src, const char * a_name, std::string & a_dst ) {
  if ( g_line_cache != a_src ) {
    // parse line
    if ( !parse_line(a_src) ) {
      return false;
    }
  }
  decltype(g_line_parts)::const_iterator v_it = g_line_parts.find(a_name);
  if ( v_it != g_line_parts.cend() ) {
    a_dst = v_it->second;
    // trim string and remove double quotes
    while ( !a_dst.empty() && isspace(a_dst.front()) ) {
      a_dst.erase(a_dst.begin());
    }
    if ( !a_dst.empty() && '"' == a_dst.front() ) {
      a_dst.erase(a_dst.begin());
    }
    while ( !a_dst.empty() && isspace(a_dst.back()) ) {
      a_dst.pop_back();
    }
    if ( !a_dst.empty() && '"' == a_dst.back() ) {
      a_dst.pop_back();
    }
    return !a_dst.empty();
  }
  return false;
}


bool get_value( char * a_src, const char * a_name, int & a_dst ) {
  if ( g_line_cache != a_src ) {
    // parse line
    if ( !parse_line(a_src) ) {
      return false;
    }
  }
  decltype(g_line_parts)::const_iterator v_it = g_line_parts.find(a_name);
  if ( v_it != g_line_parts.cend() ) {
    a_dst = (int)::strtol(v_it->second, 0,10);
    return true;
  }
  return false;
}


bool load_tga_file( const char * a_file_name, source_font_desc_s & a_dst ) {
  ::printf( "loading file '%s'\n", a_file_name );

  struct stat v_stat;
  ::bzero( &v_stat, sizeof(v_stat) );

  if ( 0 != ::stat( a_file_name, &v_stat ) ) {
    ::fprintf( stderr, "can't stat file '%s'\n", a_file_name );
    return false;
  }
  if ( ((off_t)(sizeof(targaheader_s) + TGA_FOOTER_SIZE)) >= v_stat.st_size ) {
    ::fprintf( stderr, "file '%s' too small for truevision targa\n", a_file_name );
    return false;
  }
  // open tga file for read  
  std::unique_ptr<FILE, int(*)(FILE *)> v_ftga(::fopen( a_file_name, "rb" ), ::fclose);
  if ( !v_ftga ) {
    ::fprintf( stderr, "can't open file '%s' for read\n", a_file_name );
    return false;
  }
  // read targa header
  targaheader_s v_tga_head;
  ::bzero( &v_tga_head, sizeof(v_tga_head) );
  if ( 1 != ::fread( &v_tga_head, sizeof(v_tga_head), 1, v_ftga.get() ) ) {
    ::fprintf( stderr, "can't read TGA header from '%s'\n", a_file_name );
    return false;
  }
  // check format, pixel bits wo alfa bits
  int v_pixel_bits = v_tga_head.databits - (v_tga_head.imtype & 0x0F);
  ::printf( "bits per pixel wo alfa: %d\n", v_pixel_bits );
  if ( TGA_DATATYPE  != v_tga_head.datatype
    || TGA_DATABITS  != v_pixel_bits
    ||             0 != v_tga_head.textsize
    ||             0 != v_tga_head.maptype
    ||             0 != v_tga_head.maporg
    ||             0 != v_tga_head.maplength
    ||             0 != v_tga_head.cmapbits ) {
    ::fprintf( stderr, "unexpected TGA header from '%s'\n", a_file_name );
    ::fprintf( stderr, "%c textsize = %02X (expected %02X)\n"
                       "%c maptype = %02X (expected %02X)\n"
                       "%c datatype = %02X (expected %02X)\n"
                       "%c maporg = %04X (expected %04X)\n"
                       "%c maplength = %04X (expected %04X)\n"
                       "%c cmapbits = %02X (expected %02X)\n"
                       "  xoffset = %d\n"
                       "  yoffset = %d\n"
                       "  width = %d\n"
                       "  height = %d\n"
                       "%c databits = %02X (expected %02X)\n"
                     , 0 != v_tga_head.textsize ? '*' : ' ', v_tga_head.textsize, 0
                     , 0 != v_tga_head.maptype  ? '*' : ' ', v_tga_head.maptype, 0
          , TGA_DATATYPE != v_tga_head.datatype ? '*' : ' ', v_tga_head.datatype, TGA_DATATYPE
                     , 0 != v_tga_head.maporg ? '*' : ' ', v_tga_head.maporg, 0
                     , 0 != v_tga_head.maplength ? '*' : ' ', v_tga_head.maplength, 0
                     , 0 != v_tga_head.cmapbits ? '*' : ' ', v_tga_head.cmapbits, 0
                     , v_tga_head.xoffset
                     , v_tga_head.yoffset
                     , v_tga_head.width
                     , v_tga_head.height
          , TGA_DATABITS != v_pixel_bits ? '*' : ' ', v_pixel_bits, TGA_DATABITS
      );
    return false;
  }
  // check file size
  int v_pixel_bytes = (int)v_tga_head.databits / 8;
  ::printf( "bytes per pixel: %d\n", v_pixel_bytes );
  int v_pixels_count = (int)v_tga_head.width * (int)v_tga_head.height;
  ::printf( "pixels count: %d\n", v_pixels_count );
  uint32_t v_data_bytes = v_pixels_count * v_pixel_bytes;
  ::printf( "tga data bytes: %d\n", v_data_bytes );
  uint32_t v_expected_file_size = sizeof(targaheader_s)
                                + TGA_FOOTER_SIZE
                                + v_data_bytes
                                ;
  if ( v_expected_file_size != v_stat.st_size ) {
    ::fprintf( stderr, "file '%s' size %lu, expected %u\n", a_file_name, v_stat.st_size, v_expected_file_size );
    return false;
  }
  // read pixel's array
  a_dst.m_bmp.resize( v_data_bytes );
  if ( 1 != ::fread( a_dst.m_bmp.data(), v_data_bytes, 1, v_ftga.get() ) ) {
    ::fprintf( stderr, "can't read pixel's array from '%s'\n", a_file_name );
    return false;
  }
  a_dst.m_tga_pixel_bytes = v_pixel_bytes;
  a_dst.m_tga_line_bytes = v_pixel_bytes * v_tga_head.width;
  return true;
}
