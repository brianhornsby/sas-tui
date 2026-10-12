#include "sas/progress.hpp"

#include <cassert>
#include <string>

int main() {
  assert(sas::progress_bar(0, 100) == "                    ");
  assert(sas::progress_bar(50, 100) == "=========>          ");
  assert(sas::progress_bar(100, 100) == "===================>");
  assert(sas::progress_bar(150, 100) == "===================>");
  assert(sas::progress_bar(-10, 100) == "                    ");
  assert(sas::progress_bar(10, 0) == "                    ");
  assert(sas::progress_bar(5, 10, 4) == "=>  ");
  assert(sas::progress_bar(10, 10, 1) == ">");
  assert(sas::progress_bar(10, 10, 0) == ">");
  return 0;
}
