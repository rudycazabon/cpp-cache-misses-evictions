#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <cstring>
#include <cstdlib>

// Cross-platform intrinsics headers
#if defined(clang)
#include <immintrin.h>
#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#endif
#elif defined(GNUC)
#include <x86intrin.h>
#endif
// Size of L1 cache line (typical value)
constexpr size_t CACHE_LINE_SIZE = 64;
// Approximate size of L1 data cache (32KB is common)
constexpr size_t L1_CACHE_SIZE = 32 * 1024;
// Check if specific intrinsics are available
#if defined(CLFLUSHOPT)
#define HAS_CLFLUSHOPT 1
#else
#define HAS_CLFLUSHOPT 0
#endif
#if defined(CLWB)
#define HAS_CLWB 1
#else
#define HAS_CLWB 0
#endif
// Memory fence function that works across compilers
inline void memory_fence() {
#if defined(_MSC_VER)
    _mm_mfence();
#else
    __sync_synchronize();
#endif
}
// Cache line flush function portable across compilers
inline void flush_cache_line(void* ptr) {
#if defined(_MSC_VER)
    _mm_clflush(ptr);
#else
    __builtin_ia32_clflush(ptr);
#endif
}
// Optional optimized cache line flush if available
inline void flush_cache_line_opt(void* ptr) {
#if HAS_CLFLUSHOPT
    _mm_clflushopt(ptr);
#else
    flush_cache_line(ptr);
#endif
}
// Optional cache line writeback if available
inline void cache_line_writeback(void* ptr) {
#if HAS_CLWB
    _mm_clwb(ptr);
#else
    flush_cache_line(ptr);
#endif
}
// Function to demonstrate different cache flush techniques
void cacheFlushing() {
    // Create aligned buffer using modern C++ alignment
    std::cout << "Creating aligned buffer...\n";
    alignas(CACHE_LINE_SIZE) std::vector<int> data(1024, 42);
    // Method 1: Using standard cache line flush
    std::cout << "Method 1: Standard cache line flush\n";
    for (size_t i = 0; i < data.size(); i += 16) {
        flush_cache_line(&data[i]);
    }
    memory_fence();

    // Method 2: Using optimized cache line flush if available
    std::cout << "Method 2: Optimized cache line flush\n";
    for (size_t i = 0; i < data.size(); i += 16) {
        flush_cache_line_opt(&data[i]);
    }
    memory_fence();

    // Method 3: Using cache line writeback if available
    std::cout << "Method 3: Cache line writeback\n";
    for (size_t i = 0; i < data.size(); i += 16) {
        cache_line_writeback(&data[i]);
    }
    memory_fence();
}
// Function to perform explicit cache eviction
void explicitCacheEviction() {
    std::cout << "Demonstrating explicit cache eviction techniques...\n";
    // Allocate a buffer the size of L1 cache with proper alignment
    auto buffer = std::unique_ptr<char[], decltype(&std::free)>(
        static_cast<char*>(_aligned_malloc(CACHE_LINE_SIZE, L1_CACHE_SIZE)), std::free);

    // Method 1: Eviction by accessing competing memory locations
    std::cout << "Method 1: Eviction by accessing competing memory\n";
    for (size_t i = 0; i < L1_CACHE_SIZE; i += CACHE_LINE_SIZE) {
        // Access memory at intervals of cache line size
        buffer[i] = 1;  // Write to force cache load
        memory_fence();  // Ensure write completes
    }

    // Method 2: Eviction using non-temporal stores if available
    std::cout << "Method 2: Eviction using non-temporal stores\n";
#if defined(__SSE__)
    alignas(32) float streamBuffer[1024];
    for (size_t i = 0; i < 1024; i += 4) {
        // Use non-temporal store to bypass cache
        _mm_stream_ps(&streamBuffer[i], _mm_set1_ps(0.0f));
    }
    memory_fence();
#else
    std::cout << "  SSE non-temporal stores not available\n";
#endif

    // Method 3: Eviction by accessing memory in reverse order
    std::cout << "Method 3: Eviction by reverse-order access\n";
    for (size_t i = L1_CACHE_SIZE; i > 0; i -= CACHE_LINE_SIZE) {
        buffer[i - 1] = 2;
        memory_fence();
    }
}
// Function to verify cache eviction effectiveness
void verifyCacheEviction() {
    constexpr size_t testSize = 1024;
    alignas(CACHE_LINE_SIZE) std::vector<int> testData(testSize);
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

    std::cout << "Initial access time: " << initialTime << " ns\n";
    std::cout << "Post-eviction access time: " << postEvictTime << " ns\n";
    std::cout << "Time difference: " << (postEvictTime - initialTime) << " ns\n";
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
        flush_cache_line(&largeArray[i]);
    }
    memory_fence();

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