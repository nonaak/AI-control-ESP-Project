#pragma once
#include <Arduino.h>
class Arduino_GFX;

// Playlist events
enum PlaylistEvent : uint8_t { 
  PE_NONE=0, 
  PE_PLAY_FILE,      // Speel geselecteerd bestand af
  PE_DELETE_CONFIRM, // Toon delete bevestiging
  PE_DELETE_FILE,    // Verwijder geselecteerd bestand (na bevestiging)
  PE_AI_ANALYZE,     // Start AI analyse van geselecteerd bestand
  PE_BACK            // Terug naar hoofdscherm
};

// Playlist interface
void playlist_begin(Arduino_GFX* gfx);
PlaylistEvent playlist_poll();
String playlist_getSelectedFile();  // Krijg de geselecteerde filename