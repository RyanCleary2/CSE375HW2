#include <cilk/cilk.h>
#include <iostream>

int main() {
    cilk_for (int i = 0; i < 10; ++i) {
        std::cout << "Hello from Cilk thread " << i << std::endl;
    }
    return 0;
}