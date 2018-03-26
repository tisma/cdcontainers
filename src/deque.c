// The MIT License (MIT)
// Copyright (c) 2017 Maksim Andrianov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
#include "cdcontainers/deque.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "data-info.h"

#define CDC_DEQUE_MIN_CAPACITY     4  // must be pow 2
#define CDC_DEQUE_COPACITY_SHIFT   1
#define CDC_DEQUE_SHRINK_THRESHOLD 4.0f
#define CDC_DEQUE_MAX_LEN          SIZE_MAX

static size_t mod(size_t index, size_t capacity)
{
        return index & (capacity - 1);
}

static size_t real_idx(struct cdc_deque *d, size_t index)
{
        return mod(d->head + index, d->capacity);
}

static bool should_shrink(struct cdc_deque *d)
{
        return d->size * CDC_DEQUE_SHRINK_THRESHOLD <= d->capacity;
}

static bool should_grow(struct cdc_deque *d)
{
        return d->size == d->capacity;
}

static bool reach_limit_size(struct cdc_deque *d)
{
        return d->size == CDC_DEQUE_MAX_LEN;
}

static enum cdc_stat reallocate(struct cdc_deque *d, size_t capacity)
{
        void **tmp;
        size_t len;

        if (capacity < CDC_DEQUE_MIN_CAPACITY) {
                if (d->capacity > CDC_DEQUE_MIN_CAPACITY)
                        capacity = CDC_DEQUE_MIN_CAPACITY;
                else
                        return CDC_STATUS_OK;
        }

        tmp = (void **)malloc(capacity * sizeof (void *));
        if (!tmp)
                return CDC_STATUS_BAD_ALLOC;

        if (d->head < d->tail) {
                memcpy(tmp, d->buffer + d->head, d->size * sizeof(void *));
        } else {
                len = d->capacity - d->head;
                memcpy(tmp, d->buffer + d->head, len * sizeof(void *));
                memcpy(tmp + len, d->buffer, d->tail * sizeof(void *));
        }

        free(d->buffer);

        d->tail     = d->size;
        d->head     = 0;
        d->capacity = capacity;
        d->buffer   = tmp;

        return CDC_STATUS_OK;
}

static enum cdc_stat grow(struct cdc_deque *d)
{
        return reallocate(d, d->capacity << CDC_DEQUE_COPACITY_SHIFT);
}

static enum cdc_stat shrink(struct cdc_deque *d)
{
        return reallocate(d, d->capacity >> CDC_DEQUE_COPACITY_SHIFT);
}

static enum cdc_stat pop_back_f(struct cdc_deque *d, bool must_free)
{
        d->tail = mod(d->tail - 1 + d->capacity, d->capacity);
        if (must_free && CDC_HAS_DFREE(d))
                d->dinfo->dfree(d->buffer[d->tail]);

        --d->size;
        if (should_shrink(d)) {
                enum cdc_stat stat = shrink(d);
                if (stat != CDC_STATUS_OK)
                        return stat;
        }

        return CDC_STATUS_OK;
}

static enum cdc_stat pop_front_f(struct cdc_deque *d, bool must_free)
{
        if (must_free && CDC_HAS_DFREE(d))
                d->dinfo->dfree(d->buffer[d->head]);

        d->head = mod(d->head + 1, d->capacity);
        --d->size;
        if (should_shrink(d)) {
                enum cdc_stat stat = shrink(d);
                if (stat != CDC_STATUS_OK)
                        return stat;
        }

        return CDC_STATUS_OK;
}

static void move_left_head_tail(struct cdc_deque *d, size_t idx)
{
        size_t num;

        if (d->head == 0) {
                CDC_SWAP(void *, d->buffer[0], d->buffer[d->capacity - 1]);
                num = (idx - d->head) * sizeof(void *);
                memmove(d->buffer, d->buffer + 1, num);
        } else {
                num = (idx - d->head) * sizeof(void *);
                memmove(d->buffer + d->head - 1, d->buffer + d->head, num);
        }
}

static void move_left_tail_head(struct cdc_deque *d, size_t idx)
{
        size_t num;

        if (idx < d->tail) {
                num = (d->capacity - d->head) * sizeof(void *);
                memmove(d->buffer + d->head - 1, d->buffer + d->head, num);
                if (idx == 0 && d->head == d->capacity - 1)
                        return;

                CDC_SWAP(void *, d->buffer[0], d->buffer[d->capacity - 1]);
                if (idx == 0)
                        return;

                num = (idx - 1) * sizeof(void *);
                memmove(d->buffer, d->buffer + 1, num);
        } else {
                num = (idx - d->head) * sizeof(void *);
                memmove(d->buffer + d->head - 1, d->buffer + d->head, num);
        }
}

static void move_left(struct cdc_deque *d, size_t idx)
{
        if (d->head < d->tail)
                move_left_head_tail(d, idx);
        else
                move_left_tail_head(d, idx);
}

static void move_right_head_tail(struct cdc_deque *d, size_t idx)
{
        size_t num;

        if (d->tail == d->capacity) {
                CDC_SWAP(void *, d->buffer[0], d->buffer[d->capacity - 1]);
                num = (d->tail - idx - 1) * sizeof(void *);
        } else {
                num = (d->tail - idx) * sizeof(void *);
        }

        memmove(d->buffer + idx + 1, d->buffer + idx, num);
}

static void move_right_tail_head(struct cdc_deque *d, size_t idx)
{
        size_t num;

        if (idx < d->tail) {
                num = (d->tail - idx) * sizeof(void *);
        } else {
                num = d->tail * sizeof(void *);
                memmove(d->buffer + 1, d->buffer, num);
                CDC_SWAP(void *, d->buffer[0], d->buffer[d->capacity - 1]);
                num = (d->capacity - idx - 1) * sizeof(void *);
        }

        memmove(d->buffer + idx + 1, d->buffer + idx, num);
}

static void move_right(struct cdc_deque *d, size_t idx)
{
        if (d->head < d->tail)
                move_right_head_tail(d, idx);
        else
                move_right_tail_head(d, idx);
}

static void move_to_left_head_tail(struct cdc_deque *d, size_t idx)
{
        size_t num = (d->tail - idx) * sizeof(void *);

        memmove(d->buffer + idx, d->buffer + idx + 1, num);
}

static void move_to_left_tail_head(struct cdc_deque *d, size_t idx)
{
        size_t num;

        if (idx < d->tail) {
                num = (d->tail - idx) * sizeof(void *);
                memmove(d->buffer + idx, d->buffer + idx + 1, num);
        } else {
                num = (d->capacity - idx) * sizeof(void *);
                memmove(d->buffer + idx, d->buffer + idx + 1, num);
                CDC_SWAP(void *, d->buffer[0],
                                d->buffer[d->capacity - 1]);
                if (d->tail == 0)
                        return;

                num = (d->tail - 1) * sizeof(void *);
                memmove(d->buffer, d->buffer + 1, num);
        }
}

static void move_to_left(struct cdc_deque *d, size_t idx)
{
        if (d->head < d->tail)
                move_to_left_head_tail(d, idx);
        else
                move_to_left_tail_head(d, idx);
}

static void move_to_right_head_tail(struct cdc_deque *d, size_t idx)
{
        size_t num = (idx - d->head) * sizeof(void *);

        memmove(d->buffer + d->head + 1, d->buffer + d->head, num);
}

static void move_to_right_tail_head(struct cdc_deque *d, size_t idx)
{
        size_t num;

        if (idx < d->tail) {
                num = idx * sizeof(void *);
                memmove(d->buffer + idx, d->buffer + idx - 1, num);
                CDC_SWAP(void *, d->buffer[0],
                                d->buffer[d->capacity - 1]);
                num = (d->capacity - d->head - 1) * sizeof(void *);
                memmove(d->buffer + d->head + 1, d->buffer + d->head, num);
        } else {
                num = (idx - d->head) * sizeof(void *);
                memmove(d->buffer + d->head + 1, d->buffer + d->head, num);
        }
}


static void move_to_right(struct cdc_deque *d, size_t idx)
{
        if (d->head < d->tail)
                move_to_right_head_tail(d, idx);
        else
                move_to_right_tail_head(d, idx);
}

static bool should_move_right(struct cdc_deque *d, size_t idx)
{
        bool result;

        if (d->head < d->tail)
                result = (d->tail - idx) < (idx - d->head);
        else
                result = idx < d->tail
                                ? (d->tail - idx) < (d->capacity - d->tail + idx)
                                : (idx - d->head) > (d->capacity - idx + d->head);

        return result;
}

static bool should_move_to_right(struct cdc_deque *d, size_t idx)
{
        return !should_move_right(d, idx);
}

static void free_range(struct cdc_deque *d, size_t start, size_t end)
{
        size_t nstart = mod(d->head + start, d->capacity);
        size_t count = end - start;

        while (count--) {
                d->dinfo->dfree(d->buffer[nstart]);
                nstart = mod(nstart + 1, d->capacity);
        }
}

static enum cdc_stat init_varg(struct cdc_deque *d, va_list args)
{
        enum cdc_stat stat;
        void *elem;

        while ((elem = va_arg(args, void *)) != NULL) {
                stat = cdc_deque_push_back(d, elem);
                if (stat != CDC_STATUS_OK)
                        return stat;
        }

        return CDC_STATUS_OK;
}

enum cdc_stat cdc_deque_ctor(struct cdc_deque **d, struct cdc_data_info *info)
{
        assert(d != NULL);

        struct cdc_deque *tmp;
        enum cdc_stat stat;

        tmp = (struct cdc_deque *)calloc(sizeof(struct cdc_deque), 1);
        if (!tmp)
                return CDC_STATUS_BAD_ALLOC;

        if (info && !(tmp->dinfo = cdc_di_shared_ctorc(info))) {
                stat = CDC_STATUS_BAD_ALLOC;
                goto error1;
        }

        stat = reallocate(tmp, CDC_DEQUE_MIN_CAPACITY);
        if (stat != CDC_STATUS_OK)
                goto error2;

        *d = tmp;
        return CDC_STATUS_OK;

error2:
        cdc_di_shared_dtor(tmp->dinfo);
error1:
        free(tmp);
        return stat;
}

enum cdc_stat cdc_deque_ctorl(struct cdc_deque **d,
                              struct cdc_data_info *info, ...)
{
        assert(d != NULL);

        enum cdc_stat stat;
        va_list args;

        va_start(args, info);
        stat = cdc_deque_ctorv(d, info, args);
        va_end(args);

        return stat;
}

enum cdc_stat cdc_deque_ctorv(struct cdc_deque **d,
                              struct cdc_data_info *info, va_list args)
{
        assert(d != NULL);

        enum cdc_stat stat;

        stat = cdc_deque_ctor(d, info);
        if (stat != CDC_STATUS_OK)
                return stat;

        return init_varg(*d, args);
}

void cdc_deque_dtor(struct cdc_deque *d)
{
        assert(d != NULL);

        if (CDC_HAS_DFREE(d))
                free_range(d, 0, d->size);

        free(d->buffer);
        cdc_di_shared_dtor(d->dinfo);
        free(d);
}

enum cdc_stat cdc_deque_at(struct cdc_deque *d, size_t index, void **elem)
{
        assert(d != NULL);
        assert(elem != NULL);

        size_t idx;

        if (index > d->size)
                return CDC_STATUS_OUT_OF_RANGE;

        idx = real_idx(d, index);
        *elem = d->buffer[idx];

        return CDC_STATUS_OK;
}

enum cdc_stat cdc_deque_insert(struct cdc_deque *d, size_t index, void *value)
{
        assert(d != NULL);
        assert(index <= d->size);

        size_t idx;

        if (index == 0)
                return cdc_deque_push_front(d, value);

        if (index == d->size)
                return cdc_deque_push_back(d, value);

        if (should_grow(d)) {
                enum cdc_stat stat = grow(d);
                if (stat != CDC_STATUS_OK)
                        return stat;
        }

        idx  = real_idx(d, index);
        if (should_move_right(d, idx)) {
                move_right(d, idx);
                d->tail = mod(d->tail + 1, d->capacity);
        } else {
                move_left(d, idx);
                d->head = mod(d->head - 1 + d->capacity, d->capacity);
                idx = real_idx(d, index);
        }

        d->buffer[idx] = value;
        ++d->size;
        return CDC_STATUS_OK;
}

enum cdc_stat cdc_deque_remove(struct cdc_deque *d, size_t index, void **elem)
{
        assert(d != NULL);
        assert(index < d->size);

        size_t idx = real_idx(d, index);

        if (elem)
                *elem = d->buffer[idx];
        else if (CDC_HAS_DFREE(d))
                d->dinfo->dfree(d);

        if (index == d->size - 1)
                return pop_back_f(d, false);

        if (index == 0)
                return pop_front_f(d, false);

        if (should_move_to_right(d, idx)) {
                move_to_right(d, idx);
                d->head = mod(d->head + 1, d->capacity);
        } else {
                move_to_left(d, idx);
                d->tail = mod(d->tail - 1, d->capacity);
        }

        --d->size;
        if (should_shrink(d)) {
                enum cdc_stat stat = shrink(d);
                if (stat != CDC_STATUS_OK)
                        return stat;
        }

        return CDC_STATUS_OK;
}

void cdc_deque_clear(struct cdc_deque *d)
{
        assert(d != NULL);

        if (CDC_HAS_DFREE(d))
                free_range(d, 0, d->size);

        d->tail = 0;
        d->head = 0;
        d->size = 0;
}

enum cdc_stat cdc_deque_push_back(struct cdc_deque *d, void *value)
{
        assert(d != NULL);

        if (should_grow(d)) {
                enum cdc_stat stat = grow(d);
                if (stat != CDC_STATUS_OK)
                        return stat;
        }

        d->buffer[d->tail] = value;
        d->tail = mod(d->tail + 1, d->capacity);
        ++d->size;

        return CDC_STATUS_OK;
}

enum cdc_stat cdc_deque_pop_back(struct cdc_deque *d)
{
        assert(d != NULL);
        assert(d->size > 0);

        return pop_back_f(d, true);
}

enum cdc_stat cdc_deque_push_front(struct cdc_deque *d, void *value)
{
        assert(d != NULL);

        if (should_grow(d)) {
                enum cdc_stat stat = grow(d);
                if (stat != CDC_STATUS_OK)
                        return stat;
        }

        d->head = mod(d->head - 1 + d->capacity, d->capacity);
        d->buffer[d->head] = value;
        ++d->size;

        return CDC_STATUS_OK;
}

enum cdc_stat cdc_deque_pop_front(struct cdc_deque *d)
{
        assert(d != NULL);
        assert(d->size > 0);

        return pop_front_f(d, true);
}

void cdc_deque_swap(struct cdc_deque *a, struct cdc_deque *b)
{
        assert(a != NULL);
        assert(b != NULL);

        CDC_SWAP(size_t, a->size, b->size);
        CDC_SWAP(size_t, a->capacity, b->capacity);
        CDC_SWAP(size_t, a->head, b->head);
        CDC_SWAP(size_t, a->tail, b->tail);
        CDC_SWAP(void **, a->buffer, b->buffer);
        CDC_SWAP(struct cdc_data_info *, a->dinfo, b->dinfo);
}
