#include "volatile_examples.hpp"

#include <iostream>

// See https://zhuanlan.zhihu.com/p/62060524
namespace volatile_tests {

void WithoutVolatile() {
    int i = 10;
    int a = i;

    std::cout << "i = " << a << std::endl;

    // 使用汇编改变内存中i的值, 因此编译器并不知道
    // __asm__ ("mov dword ptr [ebp-4], 20h");

    int b = i;
    std::cout << "i = " << b << std::endl;
}

void WithVolatile() {
    volatile int i = 10;
    int a = i;

    std::cout << "i = " << a << std::endl;

    // 使用汇编改变内存中i的值, 因此编译器并不知道
    //  __asm__ ("mov dword ptr [ebp-4], 20h");

    int b = i;
    std::cout << "i = " << b << std::endl;
}
    
} // namespace volatile