#ifndef IMAGE_POPUP_H
#define IMAGE_POPUP_H

#include <lvgl.h>
#include <Arduino.h>

// Zeigt ein Bild von der SD-Karte oder URL als Fullscreen-Popup an
// path: Pfad zur Datei (z.B. "/bild.bin") oder URL (http/https)
void show_image_popup(const char* path, uint16_t slideshow_sec = 10);
void hide_image_popup();
void preload_image_popup(const char* path);

#endif // IMAGE_POPUP_H
