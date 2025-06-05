#include "gtest/gtest.h"
#include <iostream>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Set up test environment
    std::cout << "Running Clipchamp BabylonNative Tests..." << std::endl;
    std::cout << "Testing Clipchamp's specific usage patterns of BabylonNative" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    int result = RUN_ALL_TESTS();
    
    std::cout << "===============================================" << std::endl;
    std::cout << "Clipchamp BabylonNative Tests completed with result: " << result << std::endl;
    
    return result;
}
