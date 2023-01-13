//
#include <memory>
#include <string>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <libgen.h>


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


struct font_props_s {
  const char * m_tga_file_name;
  int m_line_height;
  
};


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

  // read targa header
  targaheader_s v_tga_head;
  ::bzero( &v_tga_head, sizeof(v_tga_head) );
  if ( 1 != ::fread( &v_tga_head, sizeof(v_tga_head), 1, v_fp_in.get() ) ) {
    ::fprintf( stderr, "can't read TGA header from '%s'\n", argv[1] );
    return 1;
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
    ::fprintf( stderr, "unexpected TGA header from '%s'\n", argv[1] );
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
    return 1;
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
    ::fprintf( stderr, "file '%s' size %lu, expected %u\n", argv[1], v_stat.st_size, v_expected_file_size );
    return 1;
  }
  // read pixel's array
  std::unique_ptr<uint8_t, void(*)(void *)> v_tga_pixels((uint8_t *)::malloc(v_data_bytes), ::free);
  if ( 1 != ::fread( v_tga_pixels.get(), v_data_bytes, 1, v_fp_in.get() ) ) {
    ::fprintf( stderr, "can't read pixel's array from '%s'\n", argv[1] );
    return 1;
  }
  // convert to 0r5g5b5; red in MS byte
  uint8_t * v_ptr = v_tga_pixels.get();
  uint8_t * v_rgb  = v_ptr;
  for ( int k = 0; k < v_pixels_count; ++k ) {
    uint16_t v_color = (uint16_t)(((v_ptr[2] >> 3) << 10) | ((v_ptr[1] >> 3) << 5) | (v_ptr[0] >> 3));
    // save as BE
    *v_rgb++ = v_color >> 8;
    *v_rgb++ = v_color & 0xFF;
    v_ptr += v_pixel_bytes;
  }
  // compress
  std::unique_ptr<uint8_t, void(*)(void *)> v_zic_data((uint8_t *)::malloc(v_data_bytes), ::free);
  int v_zic_bytes = zic_compress( (uint16_t *)v_tga_pixels.get(), v_pixels_count, v_zic_data.get(), v_data_bytes );
  // base name
  std::string v_base_name = ::basename( argv[1] );
  std::replace( v_base_name.begin(), v_base_name.end(), '.', '_' );
  std::string v_header_name = argv[2];
  std::replace( v_header_name.begin(), v_header_name.end(), '.', '_' );
  // write out file.h
  ::fprintf( v_fp_out_h.get(), "#ifndef _I%s_\n"
                               "#define _I%s_\n\n"
                               "#include <inttypes.h>\n\n\n"
                               "#define I%s_width %d\n"
                               "#define I%s_height %d\n\n"
                               "extern const uint8_t I%s_zic[%d];\n\n"
                               "#endif\n"
                             , v_header_name.c_str()
                             , v_header_name.c_str()
                             , v_base_name.c_str(), v_tga_head.width
                             , v_base_name.c_str(), v_tga_head.height
                             , v_base_name.c_str(), v_zic_bytes
                             );
  // write out file.c
  ::fprintf( v_fp_out_c.get(), "#include \"%s\"\n\n\n"
                               "const uint8_t I%s_zic[%d] = {\n"
                             , argv[2]
                             , v_base_name.c_str(), v_zic_bytes
                             );
  v_ptr = v_zic_data.get();
  int v_line_bytes = 0;
  for ( int k = 0; k < v_zic_bytes; ++k ) {
    ::fprintf( v_fp_out_c.get(), "0x%02X, ", v_ptr[k] );
    if ( ++v_line_bytes >= 16 ) {
      ::fprintf( v_fp_out_c.get(), "\n" );
      v_line_bytes = 0;
    } else {
      ::fprintf( v_fp_out_c.get(), " " );
    }
  }
  if ( 0 != v_line_bytes ) {
    ::fprintf( v_fp_out_c.get(), "\n" );
  }
  ::fprintf( v_fp_out_c.get(), "};\n" );
  
  return 0;
}
