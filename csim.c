#include "cachelab.h"
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ADDR_LEN 64 // Defines the maximum address length for our simulation.

// Define a custom type for handling addresses within our cache.
typedef unsigned long long int address_t;

// Structure to represent a cache line, with metadata for cache management.
typedef struct cache_entry {
    char is_valid; // Valid bit indicating if this cache line is in use.
    address_t entry_tag; // The tag part of the cached address.
    unsigned long long int usage_counter; // Counter for implementing LRU eviction policy.
    char is_dirty; // Indicates if the line has been written to since being loaded.
    address_t access_time; // Tracks the last time this line was accessed.
} cache_entry_t;

typedef cache_entry_t* set_ptr; // Defines a pointer to a set of cache lines.
typedef set_ptr* cache_mem; // Defines a pointer to the entire cache.

// Global variables for configuring the cache and storing simulation results.
int output_details = 0; // Flag to enable detailed output.
int set_bits = 0; // The number of bits used for the set index.
int block_bits = 0; // The number of bits used to identify the block offset.
int lines_per_set = 0; // The associativity, i.e., number of lines per set.
char* access_trace = NULL; // File path for the memory access trace.

// Derived configuration values.
int num_sets; // Total number of sets in the cache, computed from set_bits.
int block_size; // Block size, computed from block_bits.

// Performance counters.
int misses = 0; // Total cache misses.
int hits = 0; // Total cache hits.
int evictions = 0; // Total lines evicted.
int evicted_dirty_bytes = 0; // Number of dirty bytes evicted.
int active_dirty_bytes = 0; // Number of dirty bytes currently in the cache.
int repeated_accesses = 0; // Number of sequential accesses to the same address.
unsigned long long cycle_counter = 1; // Global counter for LRU policy.
address_t* last_memory_access; // Tracks the most recent access per set.

address_t last_accessed_address = ULLONG_MAX;

cache_mem main_cache; // The primary cache data structure.
address_t set_mask; // Mask for extracting the set index from an address.

// Initialize cache based on the global configuration parameters.
void initializeCache() {
    main_cache = (set_ptr*)malloc(sizeof(set_ptr) * num_sets);
    last_memory_access = (address_t*)malloc(sizeof(address_t) * num_sets);
    for (int i = 0; i < num_sets; i++) {
        main_cache[i] = (cache_entry_t*)malloc(sizeof(cache_entry_t) * lines_per_set);
        last_memory_access[i] = ULLONG_MAX; // Initialize to max to signify no access yet.
        for (int j = 0; j < lines_per_set; j++) {
            main_cache[i][j].is_valid = 0;
            main_cache[i][j].entry_tag = 0;
            main_cache[i][j].usage_counter = 0;
            main_cache[i][j].is_dirty = 0;
            main_cache[i][j].access_time = 0; // Not used, consider removing for clarity.
        }
    }
    set_mask = (address_t)(pow(2, set_bits) - 1); // Precompute the set mask for later use.
}

// Deallocate all allocated memory for the cache, avoiding memory leaks.
void clearCache() {
    for (int i = 0; i < num_sets; i++) {
        free(main_cache[i]); // Free each set individually.
    }
    free(main_cache); // Free the array of pointers to sets.
    free(last_memory_access); // Free the last access tracking array.
}

void processMemoryLoad(address_t mem_addr) {
    if (last_accessed_address == mem_addr) {
        repeated_accesses++; // Increment if this is a repeated access.
    }
    last_accessed_address = mem_addr;
}
// Processes a memory access, updating the cache state accordingly.
void processMemoryAccess(address_t mem_addr, int ignore_repeat) {
    int found = 0; // Flag to mark a hit.
    unsigned long long eviction_metric = ULONG_MAX;
    unsigned int evict_line = 0;
    address_t index = (mem_addr >> block_bits) & set_mask;
    address_t tag_val = mem_addr >> (set_bits + block_bits);

    set_ptr current_set = main_cache[index]; // Get the relevant set.

    // Search for a hit or an empty line.
    for (int i = 0; i < lines_per_set; ++i) {
        if (current_set[i].is_valid && current_set[i].entry_tag == tag_val) {
            hits++; // A hit!
            current_set[i].usage_counter = cycle_counter++; // Update LRU.
            if (!current_set[i].is_dirty) {
                current_set[i].is_dirty = 1; // Mark as dirty if this is a write.
                active_dirty_bytes += block_size;
            }
            found = 1; // Mark we've found our target.
            break; // Stop searching.
        }
    }

    // Handle a miss.
    if (!found) {
        misses++; // Increment miss count.
        // Find the LRU line or an empty line to use for this new entry.
        for (int i = 0; i < lines_per_set; ++i) {
            if (!current_set[i].is_valid || current_set[i].usage_counter < eviction_metric) {
                evict_line = i; // Candidate line for eviction.
                eviction_metric = current_set[i].usage_counter; // Update metric for LRU.
            }
        }

        // Evict if necessary.
        if (current_set[evict_line].is_valid) {
            evictions++; // Increment evictions.
            if (current_set[evict_line].is_dirty) {
                evicted_dirty_bytes += block_size; // Track evicted dirty data.
                active_dirty_bytes -= block_size; // Update active dirty byte count.
            }
        }

        // Place the new entry.
        current_set[evict_line].is_valid = 1;
        current_set[evict_line].entry_tag = tag_val;
        current_set[evict_line].usage_counter = cycle_counter++; // Update LRU.
        current_set[evict_line].is_dirty = 0; // New entry is not dirty.
    }

    if (last_accessed_address == mem_addr && ignore_repeat == 0) {
        repeated_accesses++; // Increment if this is a repeated access.
    }
    last_accessed_address = mem_addr;


}

// Read and simulate memory access from the trace file.
void analyzeTrace(char* trace_path) {
    FILE* trace = fopen(trace_path, "r");
    if (!trace) {
        fprintf(stderr, "Error opening trace file: %s\n", trace_path);
        exit(1);
    }

    char operation;
    address_t address;
    int size;

    // Loop through all lines in the trace file.
    while (fscanf(trace, " %c %llx,%d", &operation, &address, &size) == 3) {
        switch (operation) {
        case 'L': // Load operation
            processMemoryLoad(address);
        case 'S': // Store operation
            processMemoryAccess(address, 0); // Process the memory access.
            break;
        case 'M': // Modify operation, processed as a load followed by a store.
            processMemoryAccess(address, 0); // First access (load).
            processMemoryAccess(address, 1); // Second access (store).
            break;
        default: // Ignore unrecognized operations.
            break;
        }
    }

    fclose(trace); // Close the trace file.
}

// Displays command-line usage information.
void usage(char* prog[]) {
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", prog[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag for detailed simulation output.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set, determining cache associativity.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file containing memory accesses to simulate.\n");
    exit(0);
}

// Parses command-line arguments and runs the cache simulation.
int main(int argc, char* argv[]) {
    char opt;

    // Parse command-line options.
    while ((opt = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (opt) {
        case 's': // Number of set index bits.
            set_bits = atoi(optarg);
            break;
        case 'E': // Associativity (lines per set).
            lines_per_set = atoi(optarg);
            break;
        case 'b': // Block size.
            block_bits = atoi(optarg);
            break;
        case 't': // Trace file path.
            access_trace = optarg;
            break;
        case 'v': // Verbose output flag.
            output_details = 1;
            break;
        case 'h': // Help flag.
        default: // Any unrecognized option will trigger usage information.
            usage(argv);
        }
    }

    // Validate that all required arguments have been supplied.
    if (set_bits == 0 || lines_per_set == 0 || block_bits == 0 || access_trace == NULL) {
        fprintf(stderr, "Missing required command line argument\n");
        usage(argv); // Print usage info and exit if missing arguments.
        exit(1);
    }

    // Compute the number of sets and block size based on provided bits.
    num_sets = (int)pow(2, set_bits);
    block_size = (int)pow(2, block_bits);

    // Initialize the cache, process the access trace, then clean up.
    initializeCache();
    analyzeTrace(access_trace);
    clearCache();

    // Output the simulation summary with performance metrics.
    printSummary(hits, misses, evictions, evicted_dirty_bytes, active_dirty_bytes, repeated_accesses);
    return 0;
}
