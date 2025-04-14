#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <cstring>

#ifdef _WIN32
    #include <ostream>
    #include <Windows.h>
    #include <string>
    #include <sstream>
#endif

// Cross-platform intrinsics headers
#if defined(__clang__)
#include <immintrin.h>
#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#endif
#elif defined(GNUC)
#include <x86intrin.h>
#endif

#if 0
#if !defined(_MSC_VER) || defined(__clang__)
    static const auto __cpuidex = [](int* cpuInfo, int func1, int func2) { __asm__ __volatile__("cpuid\n\t" : "=a"(cpuInfo[0]), "=b"(cpuInfo[1]), "=c"(cpuInfo[2]), "=d"(cpuInfo[3]) : "a"(func1), "c"(func2)); };  // NOLINT 
    // static constexpr auto _mm_clwb = [](const void *addr) { __asm__ __volatile__("clwb (%0)\n\t" : : "r"(addr)); };                                                                                                 // NOLINT 
    static const auto _mm_clwb = [](const void* addr) { __asm__ __volatile__(".byte 0x66, 0x0f, 0xae, 0x30\n\t" : : "a"(addr)); };  // NOLINT 
    static const auto _mm_clflushopt = [](const void* addr) { __asm__ __volatile__("clflushopt (%0)\n\t" : : "r"(addr)); };         // NOLINT 
    static const auto _mm_clflush = [](const void* addr) { __asm__ __volatile__("clflush (%0)\n\t" : : "r"(addr)); };               // NOLINT 
    static const auto _mm_sfence = []() { __asm__ __volatile__("sfence\n\t"); };                                                    // NOLINT 
#endif 
#endif 

// Size of L1 cache line (typical value)
constexpr size_t CACHE_LINE_SIZE = 64;
// Approximate size of L1 data cache (32KB is common)
constexpr size_t L1_CACHE_SIZE = 32 * 1024;

// Function to demonstrate different cache flush techniques
void cacheFlushing() {
    // Allocate a buffer aligned to cache line size
    constexpr size_t bufferSize = 1024;
    alignas(64) std::vector<int> data(bufferSize, 42);
    
    // Method 1: Using _mm_clflush for individual addresses
    for (size_t i = 0; i < bufferSize; i += 16) {
        _mm_clflush(&data[i]);
    }
    _mm_mfence();  // Memory fence to ensure flush completes
    
    // Method 2: Using _mm_clflushopt (if supported) - faster than _mm_clflush
    for (size_t i = 0; i < bufferSize; i += 16) {
        _mm_clflushopt(&data[i]);
    }
    _mm_mfence();
    
    // Method 3: Using _mm_clwb (Cache Line Write Back) if supported
    for (size_t i = 0; i < bufferSize; i += 16) {
        _mm_clwb(&data[i]);
    }
    _mm_mfence();
}

// Function to perform explicit cache eviction
void explicitCacheEviction() {
    std::cout << "Demonstrating explicit cache eviction techniques...\n";

    // Allocate a buffer the size of L1 cache
    auto buffer = std::make_unique<char[]>(L1_CACHE_SIZE);
    
    // Method 1: Eviction by accessing competing memory locations
    for (size_t i = 0; i < L1_CACHE_SIZE; i += CACHE_LINE_SIZE) {
        // Access memory at intervals of cache line size
        buffer[i] = 1;  // Write to force cache load
        _mm_mfence();   // Ensure write completes
    }

    // Method 2: Eviction using streaming stores
    alignas(32) float streamBuffer[1024];
    for (size_t i = 0; i < 1024; i++) {
        // _mm_stream_ps writes directly to memory, bypassing cache
        _mm_stream_ps(&streamBuffer[i & ~3], _mm_set1_ps(0.0f));
    }
    _mm_mfence();

    // Method 3: Eviction by accessing memory in reverse order
    // This can help defeat hardware prefetchers
    for (size_t i = L1_CACHE_SIZE; i > 0; i -= CACHE_LINE_SIZE) {
        buffer[i-1] = 2;
        _mm_mfence();
    }
}

// Function to verify cache eviction effectiveness
void verifyCacheEviction() {
    constexpr size_t testSize = 1024;
    alignas(64) std::vector<int> testData(testSize);
    
    // First access - populate cache
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < testSize; i++) {
        testData[i] = i;
    }
    auto firstAccess = std::chrono::high_resolution_clock::now();

    // Perform explicit eviction
    explicitCacheEviction();
    
    // Access after eviction
    auto evictStart = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < testSize; i++) {
        testData[i] = i * 2;
    }
    auto evictEnd = std::chrono::high_resolution_clock::now();

    // Calculate and print timings
    auto initialTime = std::chrono::duration_cast<std::chrono::nanoseconds>
                      (firstAccess - start).count();
    auto postEvictTime = std::chrono::duration_cast<std::chrono::nanoseconds>
                        (evictEnd - evictStart).count();
#ifdef KILL
    std::wstreambuf* backup;
    backup = std::wcout.rdbuf();
    std::wstringstream ss;
    std::wcout.rdbuf(ss.rdbuf());
#endif
    std::cout << "Initial access time: " << initialTime << " ns\n";
    std::cout << "Post-eviction access time: " << postEvictTime << " ns\n";
    std::cout << "Time difference: " << (postEvictTime - initialTime) << " ns\n";
#ifdef KILL
    OutputDebugString(ss.str().c_str());
    std::wcout.rdbuf(backup);
#endif
}

// Demonstration of cache flush impact on performance
void measureCacheImpact() {
    constexpr size_t arraySize = 1024 * 1024;  // 1MB of integers
    std::vector<int> largeArray(arraySize, 0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // First access - cold cache
    for (size_t i = 0; i < arraySize; i += 16) {
        largeArray[i] *= 2;
    }
    
    auto firstAccess = std::chrono::high_resolution_clock::now();
    
    // Second access - warm cache
    for (size_t i = 0; i < arraySize; i += 16) {
        largeArray[i] *= 2;
    }
    
    auto secondAccess = std::chrono::high_resolution_clock::now();
    
    // Flush cache
    for (size_t i = 0; i < arraySize; i += 16) {
        _mm_clflush(&largeArray[i]);
    }
    _mm_mfence();
    
    // Third access - after flush
    for (size_t i = 0; i < arraySize; i += 16) {
        largeArray[i] *= 2;
    }
    
    auto thirdAccess = std::chrono::high_resolution_clock::now();
    
    // Calculate and print timings
    auto coldTime = std::chrono::duration_cast<std::chrono::microseconds>
                   (firstAccess - start).count();
    auto warmTime = std::chrono::duration_cast<std::chrono::microseconds>
                   (secondAccess - firstAccess).count();
    auto flushTime = std::chrono::duration_cast<std::chrono::microseconds>
                    (thirdAccess - secondAccess).count();
    
    std::cout << "Cold cache access time: " << coldTime << " microseconds\n";
    std::cout << "Warm cache access time: " << warmTime << " microseconds\n";
    std::cout << "Post-flush access time: " << flushTime << " microseconds\n";
}

int main() {
    std::cout << "Demonstrating cache flushing techniques...\n";
    cacheFlushing();
    
    std::cout << "\nDemonstrating cache eviction...\n";
    explicitCacheEviction();
    
    std::cout << "\nVerifying cache eviction effectiveness...\n";
    verifyCacheEviction();
    
    std::cout << "\nMeasuring cache impact on performance...\n";
    measureCacheImpact();
    
    return 0;
}