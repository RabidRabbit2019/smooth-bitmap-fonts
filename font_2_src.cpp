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
  void fill_packed( packed_symbol_desc_s & a_dst, int a_offset, bool a_nibble ) const {
    a_dst.m_code = m_code;
    a_dst.m_offset = a_offset;
    a_dst.m_nibble = (a_nibble ? 1 : 0);
    a_dst.m_width = m_width;
    a_dst.m_height = m_height;
    a_dst.m_x_offset = m_x_offset;
    a_dst.m_y_offset = m_y_offset;
    a_dst.m_x_advance = m_x_advance;
  }
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
  const char * m_header_file_name; // font header file name
  std::string m_face;           // font face name, using as alias name
  std::vector<source_symbol_desc_s> m_symbols; // descriptions of symbols ptr
  source_font_desc_s()
    : m_bmp_width(0)
    , m_bmp_height(0)
    , m_symbols_count(0)
    , m_row_height(0)
    , m_def_code_idx(0)
    , m_tga_line_bytes(0)
    , m_tga_pixel_bytes(0)
    , m_header_file_name(0)
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


// comparator for map with key const char *
struct compare_two_char_ptr {
  bool operator () ( const char * const & a1, const char * const & a2 ) const {
    return ::strcmp( a1, a2 ) < 0;
  }
};


// line name, first get_value() parse key-value pairs, next just search in map
std::string g_line_cache;
// key-value pairs for current line
std::map<const char *, const char *, compare_two_char_ptr> g_line_parts;
// load font description from text file and bmp data from targa image file
bool load_font_desc( FILE * a_fp, source_font_desc_s & a_dst );
// write out .h and .c files with packed font
void write_packed_font( FILE * a_out_h, FILE * a_out_c, const source_font_desc_s & a_src );
// parse params line
bool parse_line( char * a_src );
// get string value
bool get_value( char * a_src, const char * a_name, std::string & a_dst );
// get int value
bool get_value( char * a_src, const char * a_name, int & a_dst );


// entry point
int main( int argc, char ** argv ) {
  if ( 4 != argc ) {
    ::fprintf( stderr, "need a input.txt and output.h with output.c(pp) file names\n" );
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
  v_font_desc.m_header_file_name = argv[2];
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


#define LN_START_INFO     "info "
#define LN_START_COMMON   "common "
#define LN_START_PAGE     "page "
#define LN_START_CHARS    "chars "
#define LN_START_CHAR     "char "

#define RD_ST_INFO    (1 << 0)
#define RD_ST_COMMON  (1 << 1)
#define RD_ST_PAGE    (1 << 2)
#define RD_ST_CHARS   (1 << 3)
#define RD_ST_ALL     (RD_ST_INFO|RD_ST_COMMON|RD_ST_PAGE|RD_ST_CHARS)


void replace_extra_symbols( std::string & a_str ) {
  // replace symbols
  for ( auto & c: a_str ) {
    if ( !isalpha(c) and !isdigit(c) ) {
      c = '_';
    }
  }
}


bool load_font_desc( FILE * a_fp, source_font_desc_s & a_dst ) {
  char v_line[1024];
  // read "common" line
  int v_info_read_state = 0;
  int v_char_idx = 0;
  while ( ::fgets( v_line, sizeof(v_line), a_fp ) ) {
    if ( 0 == ::strncmp( v_line, LN_START_INFO, ::strlen(LN_START_INFO) ) ) {
      if ( 0 != (v_info_read_state & RD_ST_INFO) ) {
        ::fprintf( stderr, "more than one line with '%s' at begin\n", LN_START_INFO );
      }
      if ( !get_value( v_line, "face", a_dst.m_face ) ) {
        return false;
      }
      //
      if ( a_dst.m_face.empty() ) {
        ::fprintf( stderr, "info.face seems to be empty string\n" );
        return false;
      }
      // replace symbols
      replace_extra_symbols( a_dst.m_face );
      if ( !a_dst.m_face.empty() ) {
        if ( !isalpha(a_dst.m_face.front()) ) {
          a_dst.m_face.insert( a_dst.m_face.begin(), 'f' );
        }
      }
      v_info_read_state |= RD_ST_INFO;
      continue;
    }
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
    bool v_remove_last_dquote = false;
    // trim string and remove double quotes
    while ( !a_dst.empty() && isspace(a_dst.front()) ) {
      a_dst.erase(a_dst.begin());
    }
    if ( !a_dst.empty() && '"' == a_dst.front() ) {
      a_dst.erase(a_dst.begin());
      v_remove_last_dquote = true;
    }
    while ( !a_dst.empty() && isspace(a_dst.back()) ) {
      a_dst.pop_back();
    }
    if ( v_remove_last_dquote && !a_dst.empty() && '"' == a_dst.back() ) {
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


std::string get_packed_data_name( const source_font_desc_s & a_src ) {
  std::string v_result = a_src.m_face;
  v_result.append( "_data" );
  return v_result;
}


std::string get_packed_symbols_name( const source_font_desc_s & a_src ) {
  std::string v_result = a_src.m_face;
  v_result.append( "_symdesc" );
  return v_result;
}


std::string get_packed_font_name( const source_font_desc_s & a_src ) {
  std::string v_result = a_src.m_face;
  v_result.append( "_font" );
  return v_result;
}


std::string get_define_header_name( const source_font_desc_s & a_src ) {
  std::string v_result( a_src.m_header_file_name );
  replace_extra_symbols( v_result );
  v_result.insert( v_result.begin(), {'_', '_'} );
  v_result.append( "__" );
  return v_result;
}


void write_packed_font( FILE * a_out_h, FILE * a_out_c, const source_font_desc_s & a_src ) {
  // write out font files
  ::printf( "write font files\n" );
  // prepare packet bmp array
  bool v_curr_nibble = false; // current nibble to write, false - high, true - low
  uint8_t v_curr_byte = 0; // current byte to write
  std::vector<uint8_t> v_symdata; // bmp array
  v_symdata.resize( a_src.m_bmp_width * a_src.m_bmp_height / 2 );
  // packed symbols desc
  std::vector<packed_symbol_desc_s> v_psyms;
  v_psyms.resize(a_src.m_symbols_count);
  // symbol index
  int v_sym_idx = 0;
  // index of byte to write into v_sym_data
  int v_write_idx = 0;
  // 
  for ( const source_symbol_desc_s &s: a_src.m_symbols ) {
    // fill packed symbol info
    s.fill_packed( v_psyms[v_sym_idx++], v_write_idx, v_curr_nibble );
    // calc idx in pixels array
    int v_bmp_idx = (s.m_y * a_src.m_tga_line_bytes) + (s.m_x * a_src.m_tga_pixel_bytes);
    // current color 0
    int v_curr_color = 0;
    int v_curr_color_counter = 0;
    for ( int y = 0; y < s.m_height; ++y ) {
      for ( int x = 0; x < s.m_width; ++x ) {
        int v_color = ((a_src.m_bmp.at(v_bmp_idx) + a_src.m_bmp.at(v_bmp_idx + 1) + a_src.m_bmp.at(v_bmp_idx + 2)) / 3) >> 5;
        if ( v_color != v_curr_color ) {
          // first check v_curr_color_counter
          if ( v_curr_color_counter > 0 ) {
            if ( v_curr_nibble ) {
              v_curr_byte |= (v_curr_color_counter - 1) | 0x8;
              v_symdata[v_write_idx++] = v_curr_byte;
              v_curr_nibble = false;
              v_curr_byte = 0;
            } else {
              v_curr_byte |= ((v_curr_color_counter - 1) | 0x8) << 4;
              v_curr_nibble = true;
            }
            v_curr_color_counter = 0;
          }
          v_curr_color = v_color;
          //
          if ( v_curr_nibble ) {
            v_curr_byte |= v_color;
            v_symdata[v_write_idx++] = v_curr_byte;
            v_curr_nibble = false;
            v_curr_byte = 0;
          } else {
            v_curr_byte |= v_color << 4;
            v_curr_nibble = true;
          }
        } else {
          // each counter from 1 to 8
          if ( ++v_curr_color_counter >= 8 ) {
            if ( v_curr_nibble ) {
              v_curr_byte |= 0x0F;
              v_symdata[v_write_idx++] = v_curr_byte;
              v_curr_nibble = false;
              v_curr_byte = 0;
            } else {
              v_curr_byte |= 0xF0;
              v_curr_nibble = true;
            }
            v_curr_color_counter = 0;
          }
        }
        v_bmp_idx += a_src.m_tga_pixel_bytes;
      }
      v_bmp_idx += (a_src.m_bmp_width - s.m_width) * a_src.m_tga_pixel_bytes;
    }
    // check v_curr_color_counter
    if ( v_curr_color_counter > 0 ) {
      // write last nibble of symbol
      if ( v_curr_nibble ) {
        v_curr_byte |= (v_curr_color_counter - 1) | 0x8;
        v_symdata[v_write_idx++] = v_curr_byte;
        v_curr_nibble = false;
        v_curr_byte = 0;
      } else {
        v_curr_byte |= ((v_curr_color_counter - 1) | 0x8) << 4;
        v_curr_nibble = true;
      }
    }
  }
  // last nibble
  if ( v_curr_nibble ) {
    v_symdata[v_write_idx++] = v_curr_byte;
  }
  // includes
  std::string v_define_header_name = get_define_header_name( a_src );
  ::fprintf( a_out_h
           , "#ifndef %s\n#define %s\n\n#include \"font_bmp.h\"\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n"
           , v_define_header_name.c_str()
           , v_define_header_name.c_str()
           );
  ::fprintf( a_out_c, "#include \"%s\"\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", a_src.m_header_file_name );
  // write symbols packed data
  std::string v_packed_data_name = get_packed_data_name( a_src );
  ::fprintf( a_out_c, "static const uint8_t %s[%d] = {\n", v_packed_data_name.c_str(), v_write_idx );
  int v_line_bytes_count = 0;
  for ( int i = 0; i < v_write_idx; ++i ) {
    if ( 0 == i ) {
      ::fprintf( a_out_c, " " );
    } else {
      ::fprintf( a_out_c, "," );
    }
    ::fprintf( a_out_c, " 0x%02X", v_symdata.at(i) );
    if ( ++v_line_bytes_count >= 16 ) {
      ::fprintf( a_out_c, "\n" );
      v_line_bytes_count = 0;
    }
  }
  if ( 0 != v_line_bytes_count ) {
    ::fprintf( a_out_c, "\n" );
  }
  ::fprintf( a_out_c, "};\n\n" );
  // write symbols description
  std::string v_packes_symbols_name = get_packed_symbols_name( a_src );
  ::fprintf( a_out_c, "static const packed_symbol_desc_s %s[%d] = {\n", v_packes_symbols_name.c_str(), a_src.m_symbols_count );
  for ( int i = 0; i < a_src.m_symbols_count; ++i ) {
    if ( 0 == i ) {
      ::fprintf( a_out_c, " " );
    } else {
      ::fprintf( a_out_c, "," );
    }
    ::fprintf( a_out_c
             , " {%u, %u, %u, %u, %u, %u, %u, %u}\n"
             , v_psyms.at(i).m_code
             , v_psyms.at(i).m_offset
             , v_psyms.at(i).m_nibble
             , v_psyms.at(i).m_width
             , v_psyms.at(i).m_height
             , v_psyms.at(i).m_x_offset
             , v_psyms.at(i).m_y_offset
             , v_psyms.at(i).m_x_advance
             );
  }
  ::fprintf( a_out_c, "};\n\n" );
  // write font description
  std::string v_font_desc_name = get_packed_font_name( a_src );
  ::fprintf( a_out_h
           , "extern const packed_font_desc_s %s;\n\n#ifdef __cplusplus\n}\n#endif\n\n#endif // %s\n"
           , v_font_desc_name.c_str()
           , v_define_header_name.c_str()
           );
  ::fprintf( a_out_c, "const packed_font_desc_s %s = {\n", v_font_desc_name.c_str() );
  ::fprintf( a_out_c
           , "%s, %d, %d, %d, %s"
           , v_packed_data_name.c_str()
           , a_src.m_symbols_count
           , a_src.m_row_height
           , a_src.m_def_code_idx
           , v_packes_symbols_name.c_str()
           );
  ::fprintf( a_out_c, "\n};\n\n#ifdef __cplusplus\n}\n#endif\n" );
}
