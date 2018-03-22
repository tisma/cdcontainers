// The MIT License (MIT)
// Copyright (c) 2018 Maksim Andrianov
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
#include "cdcontainers/pairing-heap.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "data-info.h"

static inline void free_node(struct cdc_pairing_heap *h,
                             struct cdc_pairing_heap_node *node)
{
        assert(h != NULL);

        if (CDC_HAS_DFREE(h))
                h->dinfo->dfree(node->key);

        free(node);
}

static inline void free_heap(struct cdc_pairing_heap *h,
                             struct cdc_pairing_heap_node *root)
{
        assert(h != NULL);

        struct cdc_pairing_heap_node *tmp = NULL;

        while (root) {
                free_heap(h, root->child);
                tmp = root->sibling;
                free_node(h, root);
                root = tmp;
        }
}

static inline bool is_heap(struct cdc_pairing_heap_node *root,
                           cdc_binary_pred_fn_t compar)
{

        while (root) {
                if (!is_heap(root->child, compar) ||
                    (root->parent && compar(root->key, root->parent->key)))
                        return false;

                root = root->sibling;
        }

        return true;
}

static inline void link(struct cdc_pairing_heap_node *a,
                        struct cdc_pairing_heap_node *broot)
{
        a->parent = broot;
        a->sibling = broot->child;
        broot->child = a;
}

static inline struct cdc_pairing_heap_node *meld(struct cdc_pairing_heap_node *a,
                                                 struct cdc_pairing_heap_node *b,
                                                 cdc_binary_pred_fn_t compare)
{
        assert(compare);

        if (a == NULL)
                return b;

        if (b == NULL)
                return a;

        if (compare(a->key, b->key))
                CDC_SWAP(struct cdc_pairing_heap_node *, a, b);

        link(a, b);
        return b;
}

static inline struct cdc_pairing_heap_node *two_pass_meld(
                struct cdc_pairing_heap_node *root, cdc_binary_pred_fn_t compare)
{
        assert(compare);

        struct cdc_pairing_heap_node *a, *b, *c;

        if (root == NULL || root->sibling == NULL)
                return root;

        a = root;
        b = root->sibling;
        c = root->sibling->sibling;

        a->parent = NULL;
        a->sibling = NULL;
        b->parent = NULL;
        b->sibling = NULL;

        return meld(meld(a, b, compare),
                    two_pass_meld(c, compare), compare);
}

static inline struct cdc_pairing_heap_node *decrease_key(
                struct cdc_pairing_heap *h, struct cdc_pairing_heap_node *pos,
                void *key)
{

        struct cdc_pairing_heap_node *node = pos, *p = node->parent;

        if (CDC_HAS_DFREE(h))
                h->dinfo->dfree(node->key);

        node->key = key;
        while (p != NULL && h->compar(node->key, p->key)) {
                CDC_SWAP(void *, node->key, p->key);
                node = p;
                p = node->parent;
        }

        return node;
}

static inline struct cdc_pairing_heap_node *increase_key(
                struct cdc_pairing_heap *h, struct cdc_pairing_heap_node *pos,
                void *key)
{
        assert(h);

        struct cdc_pairing_heap_node *curr = pos, *ch = curr->child, *t;

        if (CDC_HAS_DFREE(h))
                h->dinfo->dfree(curr->key);

        curr->key = key;
        while (ch != NULL) {
                if (h->compar(ch->key, curr->key)) {
                        t = ch;
                        while (ch->sibling != NULL) {
                                if (h->compar(ch->sibling->key, t->key))
                                        t = ch->sibling;

                                ch = ch->sibling;
                        }

                        CDC_SWAP(void *, t->key, curr->key);
                        curr = t;
                        ch = curr->child;
                } else {
                        ch = ch->sibling;
                }
        }

        return curr;
}

static inline enum cdc_stat init_varg(struct cdc_pairing_heap *h, va_list args)
{
        assert(h != NULL);

        enum cdc_stat ret;
        void *elem;

        while ((elem = va_arg(args, void *)) != NULL) {
                ret = cdc_pairing_heap_insert(h, elem);
                if (ret != CDC_STATUS_OK)
                        return ret;
        }

        return CDC_STATUS_OK;
}

enum cdc_stat cdc_pairing_heap_ctor(struct cdc_pairing_heap **h,
                                    struct cdc_data_info *info,
                                    cdc_binary_pred_fn_t compar)
{
        assert(h !=  NULL);

        struct cdc_pairing_heap *tmp;

        tmp = (struct cdc_pairing_heap *)calloc(
                                sizeof(struct cdc_pairing_heap), 1);
        if (!tmp)
                return CDC_STATUS_BAD_ALLOC;

        if (info && !(tmp->dinfo = cdc_di_shared_ctorc(info))) {
                free(tmp);
                return CDC_STATUS_BAD_ALLOC;
        }
        tmp->compar = compar;
        *h = tmp;
        return CDC_STATUS_OK;
}

enum cdc_stat cdc_pairing_heap_ctorl(struct cdc_pairing_heap **h,
                                     struct cdc_data_info *info,
                                     cdc_binary_pred_fn_t compar, ...)
{
        assert(h != NULL);

        enum cdc_stat ret;
        va_list args;

        va_start(args, compar);
        ret = cdc_pairing_heap_ctorv(h, info, compar, args);
        va_end(args);
        return ret;
}

enum cdc_stat cdc_pairing_heap_ctorv(struct cdc_pairing_heap **h,
                                     struct cdc_data_info *info,
                                     cdc_binary_pred_fn_t compar, va_list args)
{
        assert(h != NULL);

        enum cdc_stat ret;

        ret = cdc_pairing_heap_ctor(h, info, compar);
        if (ret != CDC_STATUS_OK)
                return ret;

        return init_varg(*h, args);
}

void cdc_pairing_heap_dtor(struct cdc_pairing_heap *h)
{
        assert(h != NULL);

        free_heap(h, h->root);
        cdc_di_shared_dtor(h->dinfo);
        free(h);
}

enum cdc_stat cdc_pairing_heap_extract_top(struct cdc_pairing_heap *h)
{
        assert(h != NULL);

        struct cdc_pairing_heap_node *node = h->root;

        h->root = two_pass_meld(h->root->child, h->compar);
        free_node(h, node);
        --h->size;
        return CDC_STATUS_OK;
}

enum cdc_stat cdc_pairing_heap_riinsert(struct cdc_pairing_heap *h, void *key,
                                        struct cdc_pairing_heap_iter *ret)
{
        assert(h != NULL);

        struct cdc_pairing_heap_node *root;

        root = (struct cdc_pairing_heap_node *)calloc(
                                sizeof(struct cdc_pairing_heap_node), 1);
        if (!root)
                return CDC_STATUS_BAD_ALLOC;

        root->key = key;
        h->root = meld(h->root, root, h->compar);
        ++h->size;
        if (ret) {
                ret->container = h;
                ret->current = root;
        }

        return CDC_STATUS_OK;
}


void cdc_pairing_heap_change_key(struct cdc_pairing_heap *h,
                                 struct cdc_pairing_heap_iter *pos, void *key)
{
        //        assert(h != NULL);
        //        assert(pos != NULL);
        //        assert(h == pos->container);

        //        struct cdc_pairing_heap_node *node;
        //        struct cdc_pairing_heap_node *curr = pos->current;

        //        if (h->compar(key, curr->key))
        //                node = decrease_key(h, curr, key);
        //        else
        //                node = increase_key(h, curr, key);


        //        pos->current = node;
}

void cdc_pairing_heap_clear(struct cdc_pairing_heap *h)
{
        assert(h != NULL);

        free_heap(h, h->root);
        h->size = 0;
        h->root = NULL;
}

void cdc_pairing_heap_swap(struct cdc_pairing_heap *a,
                           struct cdc_pairing_heap *b)
{
        assert(a != NULL);
        assert(b != NULL);

        CDC_SWAP(struct cdc_pairing_heap_node *, a->root, b->root);
        CDC_SWAP(size_t, a->size, b->size);
        CDC_SWAP(cdc_binary_pred_fn_t, a->compar, b->compar);
        CDC_SWAP(struct cdc_data_info *, a->dinfo, b->dinfo);
}

void cdc_pairing_heap_merge(struct cdc_pairing_heap *h,
                            struct cdc_pairing_heap *other)
{
        assert(h);
        assert(other);

        h->root = meld(h->root, other->root, h->compar);
        h->size += other->size;
        other->size = 0;
        other->root = NULL;
}

bool cdc_pairing_heap_is_heap(struct cdc_pairing_heap *h)
{
        assert(h != NULL);

        return is_heap(h->root, h->compar);
}