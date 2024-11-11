cmake_minimum_required(VERSION 3.17)
project(os4)

set(CMAKE_CXX_STANDARD 14)

#add_executable(os4 test1_write_read_all_virtual_memory.cpp MemoryConstants.h PhysicalMemory.h VirtualMemory.h VirtualMemory.cpp PhysicalMemory.cpp )
add_executable(os4_1_2  test2_write_one_page_twice_and_read.cpp MemoryConstants.h
        PhysicalMemory.h VirtualMemory.h VirtualMemory.cpp PhysicalMemory.cpp )
add_executable(os4 SimpleTest.cpp MemoryConstants.h PhysicalMemory.h VirtualMemory.h VirtualMemory.cpp PhysicalMemory.cpp )
add_executable(s main.cpp example.txt)

#add_executable(os4_1 test1_write_read_all_virtual_memory.cpp MemoryConstants.h PhysicalMemory.h VirtualMemory.h v2.cpp PhysicalMemory.cpp v2.cpp)
#
#cmake_minimum_required(VERSION 2.8.2)
#
#project(googletest-download NONE)

add_executable(os4_1_2_3  test2_write_one_page_twice_and_read.cpp MemoryConstants.h PhysicalMemory.h VirtualMemory.h v2.cpp PhysicalMemory.cpp )
