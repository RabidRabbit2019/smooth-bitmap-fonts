# smooth-bitmap-fonts
Library for display bitmap-based raster fonts from https://snowb.org/

## Targets
1. use prepared by https://snowb.org/ symbol sets
2. for ucontrollers with small amount of FlashROM
3. display symbols with variable width size and fixed line height
4. display smooth images of symbols with 8 grades from background to foreground
   (0 - background colour, 7 - foreground colour, 1..6 - shades)
5. current code support 16 bits per pixel

## Build
g++ font_2_src.cpp -Wall -Wextra -O2 -s -o font_2_src

## Using
1. go to and set up your character set
2. use white for foreground color and black for background
3. export font with "Font Name" = test_font, "File Name" = test_font and "Format" = "BMFont TEXT"
4. unpack archive test_font.zip (two files: test_font.png and test_font.txt)
5. open test_font.png in GIMP, export as test_font.tga with disabled "RLE compression" and "Origin" = Top left
6. in file test_font.txt replace file name "Unnamed.png" with "test_font.tga"
7. create sources by command ./font_2_src test_font.txt test_font.h test_font.c

## Extra
in utils/ directory:
1. font_bmp.c - release display routine for ST7735 like display controller
2. test_font.h and test_font.c - example font
3. test_font_bmp.cpp - test utility
4. build: g++ test_font_bmp.cpp font_bmp.c test_font.c -I ../ -Wall -Wextra -O0 -g -o test_font_bmp
5. test: ./test_font_bmp "%" | hexdump -v -e '"%04_ad |" 50/1 "%02X" "|\n"'
6. 50 - x_advance parameter of symbol "%" in this font
