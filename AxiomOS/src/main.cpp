#include <Arduino.h>

#include "core/app.h"

axiom::App g_app;

void setup() {
  if (!g_app.Begin()) {
    for (;;) {
      delay(250);
    }
  }
}

void loop() { g_app.Loop(); }
