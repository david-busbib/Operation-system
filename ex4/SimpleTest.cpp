#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cstdio>
#include <cassert>

int main(int argc, char **argv) {
    VMinitialize();
    for (uint64_t i = 0; i <100; ++i) {
        printf("writing to %llu\n", (long long int) i);
        VMwrite(5 * i * PAGE_SIZE, i);
    }
            word_t value;

    VMread(5 * 5* PAGE_SIZE, nullptr);
    printf("reading from %llu %d\n", (long long int) 5, value);

//    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
//        word_t value;
//        VMread(5 * i * PAGE_SIZE, &value);
//        printf("reading from %llu %d\n", (long long int) i, value);
//        assert(uint64_t(value) == i);
//    }
    printf("success\n");
//    printEvictionCounter();


    return 0;
}
