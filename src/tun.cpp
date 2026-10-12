#include "upf_core.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        upf::UpfCore upf;
        upf.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
