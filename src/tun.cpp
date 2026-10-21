#include "upf_core.h"
#include <exception>
#include <iostream>

int main() {
    try {
        upf::UpfCore upf;
        upf.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error during startup: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
