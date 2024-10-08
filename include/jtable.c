#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "jtable.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define LDF 0.8

long pve_offset(long i, uint16_t offset, size_t mod)
{
        i += offset;
        return i >= (long)mod ? i - (long)mod : i;
}

long nve_offset(long i, uint16_t offset, size_t mod)
{
        i -= offset;
        return i < 0 ? (long)mod + i : i;
}

long index_delta(long i, long j, long mod)
{
        return j < i ? mod - i + j : j - i;
}

size_t hash(keyint_t k)
{
        return k;
}

void jtable_init_with_capacity(jtable *self, size_t cap)
{
        self->buckets = calloc(cap, sizeof(struct bucket));
        self->len = 0;
        self->cap = cap;
}

void jtable_init(jtable *self)
{
        self->buckets = NULL;
        self->len = 0;
        self->cap = 0;
}

void bucket_print(struct bucket b)
{
        switch (b.ctrl) {
        case CTRL_EMPTY:
                printf("e[]");
                break;
        case CTRL_DISPLACED:
                printf("d[");
                break;
        case CTRL_DISPLACED_HEAD:
                printf("h[");
                break;
        case CTRL_SNUG:
                printf("s[");
                break;
        }
        if (b.ctrl != CTRL_EMPTY) {
                printf("%lu: %lu] %d <-> %d => %d", b.key, b.val, b.prev, b.next, b.chain_start);
        }
}

void jtable_print(jtable *self)
{
        printf("jtable {\n");
        for (size_t i = 0; i < self->cap; ++i) {
                printf("  ");
                bucket_print(self->buckets[i]);
                printf(",\n");
        }
        printf("}\n");
}

static inline size_t jtable_lprobe_until_empty(jtable *self, size_t i)
{
        do {
                i += 1;
                i %= self->cap;
        } while (self->buckets[i].ctrl != CTRL_EMPTY);
        return i;
}

static inline size_t jtable_qprobe_until_empty(jtable *self, size_t i)
{
        size_t pi = 1;
        do {
                i += pi++;
                i %= self->cap;
        } while (self->buckets[i].ctrl != CTRL_EMPTY);
        return i;
}

void jtable_realloc(jtable *self)
{
        jtable newtbl;
        jtable_init_with_capacity(&newtbl, self->cap ? self->cap * 4 : 32);
        for (size_t i = 0; i < self->cap; ++i) {
                struct bucket b = self->buckets[i];
                if (b.ctrl != CTRL_EMPTY) {
                        jtable_insert(&newtbl, b.key, b.val);
                }
        }
        jtable_deinit(self);
        *self = newtbl;
}

static inline long jtable_follow_chain(jtable *self, keyint_t k, long i)
{
        struct bucket *b = &self->buckets[i];
        i = pve_offset(i, b->chain_start, self->cap);
        b = &self->buckets[i];
        // Follow the chain until the end
        do {
                if (b->key == k) {
                        // Update in-place, if we find a match along the way
                        return i;
                }
                i = pve_offset(i, b->next, self->cap);
                b = &self->buckets[i];
        } while (b->next != 0);
        return i;
}

void jtable_insert(jtable *self, keyint_t k, valint_t v)
{
        if (self->len >= 3 * self->cap / 4) {
                jtable_realloc(self);
        }
        size_t h = hash(k);
        long i = h % self->cap;
        struct bucket *b = &self->buckets[i];
        if (b->ctrl == CTRL_EMPTY) {
                // We can immediately insert into the bucket
                // extremely low-cost
                memset(b, 0, sizeof(struct bucket));
                b->ctrl = CTRL_SNUG;
                b->key = k;
                b->val = v;
                self->len++;
                return;
        }
        if (unlikely(b->ctrl != CTRL_SNUG && b->chain_start == 0)) {
                // No chain exists for this hash, and this cell is already
                // occupied
                // low-cost -- this is just a linear probe
                long j = jtable_qprobe_until_empty(self, i);
                b->chain_start = index_delta(i, j, self->cap);
                b = &self->buckets[j];
                i = (i + 1) % self->cap;
                memset(b, 0, sizeof(struct bucket));
                b->ctrl = CTRL_DISPLACED_HEAD;
                b->key = k;
                b->val = v;
                self->len++;
                return;
        }
        // else: A chain exists for this hash, but it starts elsewhere
        i = jtable_follow_chain(self, k, i);
        b = &self->buckets[i];
        if (b->key == k) {
                b->val = v;
                return;
        }
        // We have reached the end of a chain, without finding a match. We need
        // to probe for a new spot
        long j = jtable_lprobe_until_empty(self, i);
        uint16_t d = index_delta(i, j, self->cap);
        b->next = d;
        self->buckets[j] = (struct bucket){
                .ctrl = CTRL_DISPLACED,
                .prev = d,
                .next = 0,
                .key = k,
                .val = v,
        };
        self->len++;
}

/** Lookup the index of a bucket that matches `k`. `-1` represents an absent 
  * key 
  */
static inline long jtable_lookup_bucket(jtable *self, keyint_t k)
{
        size_t h = hash(k);
        long i = h % self->cap;
        struct bucket *b = &self->buckets[i];
        if (b->ctrl == CTRL_EMPTY) {
                return -1;
        }
        i = jtable_follow_chain(self, k, i);
        b = &self->buckets[i];
        if (b->key == k) {
                return i;
        }
        return -1;
}

/** For some bucket `b` that is being removed, replace it with the chain which
  * it blocks. 
  */
static inline void jtable_replace_with_chain_start(jtable *self, long i)
{
        struct bucket *b = &self->buckets[i];
        if (b->chain_start) {
                long j = (i + (long)b->chain_start) % self->cap;
                struct bucket *headb = &self->buckets[j];
                headb->ctrl = CTRL_EMPTY;
                b->ctrl = CTRL_SNUG;
                if (headb->next) {
                        b->next = headb->next + b->chain_start;
                        struct bucket *nextb = &self->buckets[(i + (long)b->next) % self->cap];
                        nextb->prev = b->next;
                } else {
                        b->next = 0;
                }
                b->chain_start = 0;
                b->prev = 0;
                b->key = headb->key;
                b->val = headb->val;
                if (headb->chain_start) {
                        jtable_replace_with_chain_start(self, j);
                }
        }
}

void jtable_remove(jtable *self, keyint_t k)
{
        if (self->cap == 0) {
                return;
        }

        size_t h = hash(k);
        long i = h % self->cap;
        struct bucket *b = &self->buckets[i];

        // If the cell is already empty, we do not need to remove anything, exit
        if (b->ctrl == CTRL_EMPTY) {
                return;
        }

        // rmvi is the index of the bucket to remove, rmvb is the actual bucket
        long rmvi = jtable_follow_chain(self, k, i);
        struct bucket *rmvb = &self->buckets[rmvi];
        // this should be static inlined and optimised to the obvious code that doesn't
        // check this twice
        if (rmvb->key != k) {
                return;
        }

        if (rmvb->ctrl == CTRL_SNUG) {
                if (!rmvb->next) {
                        rmvb->ctrl = CTRL_EMPTY;
                        return;
                }
                long j = (rmvi + (long)rmvb->next) % self->cap;
                struct bucket *nextb = &self->buckets[j];
                nextb->ctrl = CTRL_EMPTY;
                rmvb->next = nextb->next ? rmvb->next + nextb->next : 0;
                rmvb->key = nextb->key;
                rmvb->val = nextb->val;
                rmvb->prev = 0;
                if (nextb->next) {
                        struct bucket *nextnextb =
                                &self->buckets[pve_offset(j, nextb->next, self->cap)];
                        nextnextb->prev = rmvb->next;
                }
                jtable_replace_with_chain_start(self, j);
                return;
        }

        struct bucket rmvb_copy = *rmvb;
        rmvb->ctrl = CTRL_EMPTY;
        if (rmvb->prev) {
                long j = nve_offset(rmvi, rmvb->prev, self->cap);
                struct bucket *prevb = &self->buckets[j];
                if (rmvb->next) {
                        prevb->next += rmvb->next;
                } else {
                        prevb->next = 0;
                }
        }
        if (rmvb->next) {
                long j = pve_offset(rmvi, rmvb->next, self->cap);
                struct bucket *nextb = &self->buckets[j];
                if (rmvb->prev) {
                        nextb->prev += rmvb->prev;
                } else {
                        nextb->prev = 0;
                }
        }
        jtable_replace_with_chain_start(self, rmvi);
        if ((i + b->chain_start) % (long)self->cap != rmvi) {
                return;
        }
        // We just removed the thing the chain_start points to, we need to
        // clean that up
        if (rmvb_copy.next) {
                b->chain_start += rmvb_copy.next;
        } else {
                b->chain_start = 0;
        }
}

valint_t *jtable_lookup(jtable *self, keyint_t k)
{
        if (self->cap == 0) {
                return NULL;
        }
        long i = jtable_lookup_bucket(self, k);
        if (i == -1) {
                return NULL;
        }
        return &self->buckets[i].val;
}

void jtable_deinit(jtable *self)
{
        if (self->buckets != NULL) {
                free(self->buckets);
        }
        self->buckets = NULL;
        self->cap = 0;
        self->len = 0;
}