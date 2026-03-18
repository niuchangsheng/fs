#include "kvfs.h"
#include <iostream>

int main() {
    std::cout << "--- Starting KVFS Test ---" << std::endl;

    auto engine = kvfs::CreateKVEngine();
    
    // First initialization (should format the filesystem)
    std::cout << "\n[Test 1] First Initialization:" << std::endl;
    engine->Init("mock_device_1").get();

    // Second initialization on the same engine instance 
    // (mock device is persistent in memory during the process lifetime)
    std::cout << "\n[Test 2] Second Initialization (Mounting existing):" << std::endl;
    engine->Init("mock_device_1").get();

    engine->Shutdown().get();

    std::cout << "\n--- KVFS Test Completed ---" << std::endl;
    return 0;
}

