#pragma once

#include <Arduino.h>

// =========================
// ASS2: Drzwi (alert)
// Standard bitmap: jak output z image2cpp (plik typu `gen`)
// =========================
#define DOOR_BITMAP_ENABLED 1
#define DOOR_BITMAP_FRAME_DELAY_MS 0
// 0 = pod zegarem, 1 = nad zegarem (może zasłaniać)
#define DOOR_BITMAP_DRAW_OVER_CLOCK 0

// Ustaw na rozmiar bitmap wygenerowanych w image2cpp dla tego alertu.
#define DOOR_BITMAP_W 16
#define DOOR_BITMAP_H 16

// Pozycja ikony/animacji
#define DOOR_BITMAP_X 18
#define DOOR_BITMAP_Y 0

// Aliasy na dane z generatora (poniżej)
#define DOOR_BITMAP_FRAME_COUNT (door_epd_bitmap_allArray_LEN)
#define DOOR_BITMAP_FRAMES (door_epd_bitmap_allArray)

// ==========================================================
// Wklej tutaj output z https://javl.github.io/image2cpp
// UWAGA: żeby nie było konfliktów nazw z innymi bitmapami:
// - ustaw w generatorze unikalną nazwę/identyfikator z prefixem "door_"
// - albo wklej i podmień nazwy na door_epd_bitmap_...
// ==========================================================

// 'door_open', 16x16px
const unsigned char door_epd_bitmap_door_open[] PROGMEM = {
  0x00, 0x00, 0xfe, 0x7f, 0x82, 0x40, 0x82, 0x40, 0x82, 0x40, 0x82, 0x40,
  0x82, 0x40, 0x82, 0x40, 0x82, 0x40, 0x82, 0x40, 0xfe, 0x7f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Array of all bitmaps for convenience.
const int door_epd_bitmap_allArray_LEN = 1;
const unsigned char *door_epd_bitmap_allArray[1] = {
  door_epd_bitmap_door_open
};

