#include "event_queue.h"

bool EventQueue::push(const Event& e) {
  int n = (h + 1) % CAP;
  if (n == t) return false;
  q[h] = e;
  h = n;
  return true;
}

bool EventQueue::pop(Event& out) {
  if (t == h) return false;
  out = q[t];
  t = (t + 1) % CAP;
  return true;
}

bool EventQueue::empty() const {
  return t == h;
}
