/*
 * Copyright (c) 2022
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef DPCOMMON_MEMORY_POOL_H
#define DPCOMMON_MEMORY_POOL_H
#include "common.h"

typedef struct DP_MemoryPoolBucket {
    char *elements; // char* for pointer arithmetics
} DP_MemoryPoolBucket;

typedef struct DP_MemoryPoolFreeNode {
    struct DP_MemoryPoolFreeNode *next;
} DP_MemoryPoolFreeNode;

typedef struct DP_MemoryPool {
    size_t el_size;         // How many bytes per element
    size_t bucket_el_count; // How many elements are in a bucket
    size_t buckets_len;     // How many buckets there are
    DP_MemoryPoolBucket *buckets;
    // Singly linked list to free elements. NULL if all buckets are full.
    DP_MemoryPoolFreeNode *free_list;
} DP_MemoryPool;

typedef struct DP_MemoryPoolStatistics {
    size_t el_size;
    size_t bucket_el_count;
    size_t buckets_len;
    size_t el_free;
} DP_MemoryPoolStatistics;

#define DP_memory_pool_new_type(TYPE, bucket_el_count) \
    DP_memory_pool_new(sizeof(TYPE), bucket_el_count)
DP_MemoryPool DP_memory_pool_new(size_t el_size, size_t bucket_el_count);
void DP_memory_pool_free(DP_MemoryPool *pool);

void DP_memory_pool_reset(DP_MemoryPool *pool);
void *DP_memory_pool_alloc_el(DP_MemoryPool *pool);
void DP_memory_pool_free_el(DP_MemoryPool *pool, void *el);

DP_MemoryPoolStatistics DP_memory_pool_statistics(DP_MemoryPool *pool);

#endif
