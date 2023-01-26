#ifndef FM_H
#define FM_H

#include <Arduino.h>
#include "tick.hpp"

class FMChip {
 public:
  void begin();
  void reset();
  void set_register(byte addr, byte value, boolean a1);
  void writeRaw(byte data, boolean a0, boolean a1 );
  void set_output(byte);
};

extern FMChip FM;

#endif
