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
#include "memory_pool.h"
#include <string.h>


DP_MemoryPool DP_memory_pool_new(size_t el_size, size_t bucket_el_count)
{
    // Must be large enough to hold free list pointer
    el_size = DP_max_size(el_size, sizeof(DP_MemoryPoolFreeNode));

    // No overflow
    DP_ASSERT(!((el_size * bucket_el_count) / el_size != bucket_el_count));

    DP_MemoryPoolBucket *buckets = DP_malloc(sizeof(*buckets));
    DP_MemoryPoolBucket first_bucket = {
        DP_malloc_simd(el_size * bucket_el_count)};
    buckets[0] = first_bucket;

    DP_MemoryPool pool = (DP_MemoryPool){.el_size = el_size,
                                         .bucket_el_count = bucket_el_count,
                                         .buckets_len = 1,
                                         .buckets = buckets};
    DP_memory_pool_reset(&pool);
    return pool;
}

void DP_memory_pool_free(DP_MemoryPool *pool)
{
    DP_ASSERT(pool);

    for (size_t i = 0; i < pool->buckets_len; i++) {
        DP_free_simd(pool->buckets[i].elements);
    }
    DP_free(pool->buckets);
}

static DP_MemoryPoolFreeNode *
DP_memory_pool_reset_bucket(DP_MemoryPool *pool, DP_MemoryPoolBucket bucket,
                            DP_MemoryPoolFreeNode *prev_el)
{
    DP_ASSERT(pool);

    for (size_t j = 0; j < pool->bucket_el_count; j++) {
        DP_MemoryPoolFreeNode *el =
            (DP_MemoryPoolFreeNode *)&bucket.elements[j * pool->el_size];
        el->next = prev_el;
        prev_el = el;
    }

    return prev_el;
}

void DP_memory_pool_reset(DP_MemoryPool *pool)
{
    DP_ASSERT(pool);

    DP_MemoryPoolFreeNode *prev_el = NULL;
    for (size_t i = 0; i < pool->buckets_len; i++) {
        prev_el = DP_memory_pool_reset_bucket(pool, pool->buckets[i], prev_el);
    }

    pool->free_list = prev_el;
}

void *DP_memory_pool_alloc_el(DP_MemoryPool *pool)
{
    DP_ASSERT(pool);

    if (pool->free_list == NULL) {
        // allocate new bucket
        DP_MemoryPoolBucket new_bucket = {
            DP_malloc_simd(pool->el_size * pool->bucket_el_count)};

        pool->buckets_len++;
        pool->buckets = DP_realloc(pool->buckets,
                                   pool->buckets_len * sizeof(*pool->buckets));
        pool->buckets[pool->buckets_len - 1] = new_bucket;

        pool->free_list = DP_memory_pool_reset_bucket(pool, new_bucket, NULL);
    }

    DP_MemoryPoolFreeNode *el = pool->free_list;
    pool->free_list = el->next;
    return (void *)el;
}

void DP_memory_pool_free_el(DP_MemoryPool *pool, void *el_voidptr)
{
    DP_ASSERT(pool);

    DP_MemoryPoolFreeNode *el = el_voidptr;
    el->next = pool->free_list;
    pool->free_list = el;
}

DP_MemoryPoolStatistics DP_memory_pool_statistics(DP_MemoryPool *pool)
{
    DP_ASSERT(pool);
    size_t el_free = 0;
    for (DP_MemoryPoolFreeNode *n = pool->free_list; n; n = n->next) {
        ++el_free;
    }
    return (DP_MemoryPoolStatistics){pool->el_size, pool->bucket_el_count,
                                     pool->buckets_len, el_free};
}
