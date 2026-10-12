#include "sas/app.hpp"
#include "sas/version.hpp"

#include <iostream>
#include <string>

auto main(int argc, char **argv) -> int {
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "sas-tui " << SAS_TUI_VERSION << '\n';
    return 0;
  }
  return sas::run(argc, argv);
}
