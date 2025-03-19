#include <iostream>
#include <tbb/tbb.h>

void parallel_function(int i) {
    std::cout << "Hello from TBB! Iteration: " << i << std::endl;
}

int main() {
    tbb::parallel_for(0, 10, parallel_function);
    return 0;
}