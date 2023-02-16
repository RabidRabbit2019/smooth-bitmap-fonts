# Build
g++ test_font_bmp.cpp ../font_bmp.c test32.c -I ../ -Wall -Wextra -O0 -g -o test_font_bmp

# Use
./test_font_bmp "!"
