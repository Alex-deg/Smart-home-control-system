#include <gtest/gtest.h>
#include <iostream>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "=== Запуск тестов системы умного дома ===" << std::endl;
    std::cout << "Количество тестов: " << RUN_ALL_TESTS() << std::endl;
    
    return RUN_ALL_TESTS();
}