#pragma once

#include <Arduino.h>

// =========================
// ASS1: Bagażnik (alert)
// Standard bitmap: jak output z image2cpp (plik typu `gen`)
// =========================
#define TRUNK_BITMAP_ENABLED 1
#define TRUNK_BITMAP_FRAME_DELAY_MS 0
// 0 = pod zegarem, 1 = nad zegarem (może zasłaniać)
#define TRUNK_BITMAP_DRAW_OVER_CLOCK 0

// Ustaw na rozmiar bitmap wygenerowanych w image2cpp dla tego alertu.
#define TRUNK_BITMAP_W 16
#define TRUNK_BITMAP_H 16

// Pozycja ikony/animacji
#define TRUNK_BITMAP_X 0
#define TRUNK_BITMAP_Y 0

// Aliasy na dane z generatora (poniżej)
#define TRUNK_BITMAP_FRAME_COUNT (trunk_epd_bitmap_allArray_LEN)
#define TRUNK_BITMAP_FRAMES (trunk_epd_bitmap_allArray)

// ==========================================================
// Wklej tutaj output z https://javl.github.io/image2cpp
// UWAGA: żeby nie było konfliktów nazw z innymi bitmapami:
// - ustaw w generatorze unikalną nazwę/identyfikator z prefixem "trunk_"
// - albo wklej i podmień nazwy na trunk_epd_bitmap_...
// ==========================================================

// 'trunk_open', 16x16px
const unsigned char trunk_epd_bitmap_trunk_open[] PROGMEM = {
  0x00, 0x00, 0xfe, 0x7f, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40,
  0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0xfe, 0x7f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Array of all bitmaps for convenience.
const int trunk_epd_bitmap_allArray_LEN = 1;
const unsigned char *trunk_epd_bitmap_allArray[1] = {
  trunk_epd_bitmap_trunk_open
};

