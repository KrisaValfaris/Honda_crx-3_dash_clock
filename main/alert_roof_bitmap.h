#pragma once

#include <Arduino.h>

// =========================
// ASS3: Dach (alert)
// Standard bitmap: jak output z image2cpp (plik typu `gen`)
// =========================
#define ROOF_BITMAP_ENABLED 1
#define ROOF_BITMAP_FRAME_DELAY_MS 0
// 0 = pod zegarem, 1 = nad zegarem (może zasłaniać)
#define ROOF_BITMAP_DRAW_OVER_CLOCK 0

// Ustaw na rozmiar bitmap wygenerowanych w image2cpp dla tego alertu.
#define ROOF_BITMAP_W 16
#define ROOF_BITMAP_H 16

// Pozycja ikony/animacji
#define ROOF_BITMAP_X 36
#define ROOF_BITMAP_Y 0

// Aliasy na dane z generatora (poniżej)
#define ROOF_BITMAP_FRAME_COUNT (roof_epd_bitmap_allArray_LEN)
#define ROOF_BITMAP_FRAMES (roof_epd_bitmap_allArray)

// ==========================================================
// Wklej tutaj output z https://javl.github.io/image2cpp
// UWAGA: żeby nie było konfliktów nazw z innymi bitmapami:
// - ustaw w generatorze unikalną nazwę/identyfikator z prefixem "roof_"
// - albo wklej i podmień nazwy na roof_epd_bitmap_...
// ==========================================================

// 'roof_open', 16x16px
const unsigned char roof_epd_bitmap_roof_open[] PROGMEM = {
  0x00, 0x00, 0x18, 0x00, 0x3c, 0x00, 0x7e, 0x00, 0xff, 0x01, 0xc3, 0x01,
  0x81, 0x01, 0x81, 0x01, 0x81, 0x01, 0xc3, 0x01, 0xff, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Array of all bitmaps for convenience.
const int roof_epd_bitmap_allArray_LEN = 1;
const unsigned char *roof_epd_bitmap_allArray[1] = {
  roof_epd_bitmap_roof_open
};

