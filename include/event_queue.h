#pragma once
#include "types.h"

class EventQueue {
public:
  bool push(const Event& e);
  bool pop(Event& out);
  bool empty() const;

private:
  static const int CAP = 16;
  Event q[CAP];
  int h = 0;
  int t = 0;
};
