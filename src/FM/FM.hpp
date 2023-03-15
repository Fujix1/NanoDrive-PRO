#ifndef FM_H
#define FM_H

#include <Arduino.h>
#include "tick.hpp"

class FMChip {
 public:
  void begin();
  void reset();
  u_int8_t set_register(byte addr, byte value, boolean a1);
  u_int8_t set_register_opm(byte addr, byte value);
  void set_output(byte);
};

extern FMChip FM;

#endif
