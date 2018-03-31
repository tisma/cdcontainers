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
#include "test-common.h"

#include <CUnit/Basic.h>
#include <float.h>
#include <stdarg.h>
#include "cdcontainers/avl-tree.h"
#include "cdcontainers/casts.h"

static struct cdc_pair a = {CDC_INT_TO_PTR(0), CDC_INT_TO_PTR(0)};
static struct cdc_pair b = {CDC_INT_TO_PTR(1), CDC_INT_TO_PTR(1)};
static struct cdc_pair c = {CDC_INT_TO_PTR(2), CDC_INT_TO_PTR(2)};
static struct cdc_pair d = {CDC_INT_TO_PTR(3), CDC_INT_TO_PTR(3)};
static struct cdc_pair e = {CDC_INT_TO_PTR(4), CDC_INT_TO_PTR(4)};
static struct cdc_pair f = {CDC_INT_TO_PTR(5), CDC_INT_TO_PTR(5)};
static struct cdc_pair g = {CDC_INT_TO_PTR(6), CDC_INT_TO_PTR(6)};
static struct cdc_pair h = {CDC_INT_TO_PTR(7), CDC_INT_TO_PTR(7)};

static int lt_int(const void *l, const void *r)
{
        return l < r;
}

static bool avl_tree_key_int_eq(struct cdc_avl_tree *t, size_t count, ...)
{
        va_list args;
        int i;
        struct cdc_pair *val;
        void *tmp;

        va_start(args, count);
        for (i = 0; i < count; ++i) {
                val = va_arg(args, struct cdc_pair *);
                if (cdc_avl_tree_get(t, val->first, &tmp) != CDC_STATUS_OK ||
                    tmp != val->second) {
                        va_end(args);
                        return false;
                }
        }

        va_end(args);
        return true;
}

static inline void avl_tree_inorder_print_int(struct cdc_avl_tree_node *node)
{
        if (node->left)
                avl_tree_inorder_print_int(node->left);

        printf("%d ", CDC_PTR_TO_INT(node->key));

        if (node->right)
                avl_tree_inorder_print_int(node->right);
}

void test_avl_tree_ctor()
{
        struct cdc_avl_tree *t;

        CU_ASSERT(cdc_avl_tree_ctor1(&t, NULL, lt_int) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 0);

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_ctorl()
{
        struct cdc_avl_tree *t;

        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int, &a, &g, &h, &d, NULL) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 4);
        CU_ASSERT(avl_tree_key_int_eq(t, 4, &a, &g, &h, &d));

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_get()
{
        struct cdc_avl_tree *t = NULL;
        void *value;

        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int,
                                      &a, &b, &c, &d, &g, &h, &e, &f, NULL) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 8);
        CU_ASSERT(avl_tree_key_int_eq(t, 8, &a, &b, &c, &d, &g, &h, &e, &f));
        CU_ASSERT(cdc_avl_tree_get(t, CDC_INT_TO_PTR(10), &value) == CDC_STATUS_NOT_FOUND);
        cdc_avl_tree_dtor(t);
}

void test_avl_tree_count()
{
        struct cdc_avl_tree *t = NULL;

        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int, &a, &b, NULL) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 2);
        CU_ASSERT(cdc_avl_tree_count(t, a.first) == 1);
        CU_ASSERT(cdc_avl_tree_count(t, b.first) == 1);
        CU_ASSERT(cdc_avl_tree_count(t, CDC_INT_TO_PTR(10)) == 0);

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_find()
{
        struct cdc_avl_tree *t = NULL;
        struct cdc_avl_tree_iter it, it_end;


        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int, &a, &b, &c, &d, &g, NULL) == CDC_STATUS_OK);

        cdc_avl_tree_find(t, a.first, &it);
        CU_ASSERT(cdc_avl_tree_iter_value(&it) == a.second);
        cdc_avl_tree_find(t, b.first, &it);
        CU_ASSERT(cdc_avl_tree_iter_value(&it) == b.second);
        cdc_avl_tree_find(t, g.first, &it);
        CU_ASSERT(cdc_avl_tree_iter_value(&it) == g.second);
        cdc_avl_tree_find(t, h.first, &it);
        cdc_avl_tree_end(t, &it_end);
        CU_ASSERT(cdc_avl_tree_iter_is_eq(&it, &it_end));

        cdc_avl_tree_dtor(t);

}

void test_avl_tree_equal_range()
{
        struct cdc_pair_avl_tree_iter res;
        struct cdc_avl_tree *t = NULL;

        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int, &a, &b, &c, &d, &g, NULL) == CDC_STATUS_OK);

        cdc_avl_tree_equal_range(t, a.first, &res);
        CU_ASSERT(cdc_avl_tree_iter_value(&res.first) == a.second);
        cdc_avl_tree_iter_next(&res.first);
        CU_ASSERT(cdc_avl_tree_iter_is_eq(&res.second, &res.first));

        cdc_avl_tree_equal_range(t, b.first, &res);
        CU_ASSERT(cdc_avl_tree_iter_value(&res.first) == b.second);
        cdc_avl_tree_iter_next(&res.first);
        CU_ASSERT(cdc_avl_tree_iter_is_eq(&res.second, &res.first));

        cdc_avl_tree_equal_range(t, d.first, &res);
        CU_ASSERT(cdc_avl_tree_iter_value(&res.first) == d.second);
        cdc_avl_tree_iter_next(&res.first);
        CU_ASSERT(cdc_avl_tree_iter_is_eq(&res.second, &res.first));

        cdc_avl_tree_equal_range(t, g.first, &res);
        CU_ASSERT(cdc_avl_tree_iter_value(&res.first) == g.second);
        cdc_avl_tree_iter_next(&res.first);
        CU_ASSERT(cdc_avl_tree_iter_is_eq(&res.second, &res.first));

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_clear()
{
        struct cdc_avl_tree *t = NULL;

        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int, &a, &b, NULL) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 2);
        cdc_avl_tree_clear(t);
        CU_ASSERT(cdc_avl_tree_size(t) == 0);
        cdc_avl_tree_clear(t);
        CU_ASSERT(cdc_avl_tree_size(t) == 0);

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_insert()
{
        struct cdc_avl_tree *t;
        int i, count = 100;
        bool failed = false;
        void *val;

        CU_ASSERT(cdc_avl_tree_ctor1(&t, NULL, lt_int) == CDC_STATUS_OK);

        for (i = 0; i < count; ++i) {
                if (cdc_avl_tree_insert(t, CDC_INT_TO_PTR(i),
                                        CDC_INT_TO_PTR(i), NULL) != CDC_STATUS_OK) {
                        failed = true;
                        break;
                }
        }

        CU_ASSERT(cdc_avl_tree_size(t) == count);
        CU_ASSERT(failed == false);

        for (i = 0; i < count; ++i) {
                if (cdc_avl_tree_get(t, CDC_INT_TO_PTR(i), &val) != CDC_STATUS_OK &&
                    val != CDC_INT_TO_PTR(i)) {
                        failed = true;
                        break;
                }
        }

        CU_ASSERT(failed == false);

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_insert_or_assign()
{
        struct cdc_pair_avl_tree_iter_bool ret;
        struct cdc_avl_tree *t = NULL;
        void *value;

        CU_ASSERT(cdc_avl_tree_ctor1(&t, NULL, lt_int) == CDC_STATUS_OK);

        CU_ASSERT(cdc_avl_tree_insert_or_assign(t, a.first, a.second, &ret) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 1);
        CU_ASSERT(cdc_avl_tree_iter_value(&ret.first) == a.second);
        CU_ASSERT(ret.second == true);

        CU_ASSERT(cdc_avl_tree_insert_or_assign(t, a.first, b.second, &ret) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, a.first, &value) == CDC_STATUS_OK);
        CU_ASSERT(value == b.second);
        CU_ASSERT(cdc_avl_tree_iter_value(&ret.first) == b.second);
        CU_ASSERT(ret.second == false);

        CU_ASSERT(cdc_avl_tree_insert_or_assign(t, c.first, c.second, &ret) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 2);
        CU_ASSERT(cdc_avl_tree_iter_value(&ret.first) == c.second);
        CU_ASSERT(ret.second == true);

        CU_ASSERT(cdc_avl_tree_insert_or_assign(t, c.first, d.second, &ret) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 2);
        CU_ASSERT(cdc_avl_tree_get(t, c.first, &value) == CDC_STATUS_OK);
        CU_ASSERT(value == d.second);
        CU_ASSERT(cdc_avl_tree_iter_value(&ret.first) == d.second);
        CU_ASSERT(ret.second == false);

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_erase()
{
        struct cdc_avl_tree *t = NULL;
        void *value;

        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int,
                                      &a, &b, &c, &d, &g, &h, &e, &f, NULL) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 8);
        CU_ASSERT(avl_tree_key_int_eq(t, 8, &a, &b, &c, &d, &g, &h, &e, &f));
        CU_ASSERT(cdc_avl_tree_erase(t, a.first) == 1);

        CU_ASSERT(cdc_avl_tree_get(t, a.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(avl_tree_key_int_eq(t, 7, &b, &c, &d, &g, &h, &e, &f));

        CU_ASSERT(cdc_avl_tree_erase(t, h.first) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, h.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(cdc_avl_tree_size(t) == 6);
        CU_ASSERT(avl_tree_key_int_eq(t, 6, &b, &c, &d, &g, &e, &f));

        CU_ASSERT(cdc_avl_tree_erase(t, h.first) == 0);
        CU_ASSERT(cdc_avl_tree_size(t) == 6);
        CU_ASSERT(avl_tree_key_int_eq(t, 6, &b, &c, &d, &g, &e, &f));

        CU_ASSERT(cdc_avl_tree_erase(t, b.first) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, b.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(cdc_avl_tree_size(t) == 5);
        CU_ASSERT(avl_tree_key_int_eq(t, 5, &c, &d, &g, &e, &f));

        CU_ASSERT(cdc_avl_tree_erase(t, c.first) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, c.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(cdc_avl_tree_size(t) == 4);
        CU_ASSERT(avl_tree_key_int_eq(t, 4, &d, &g, &e, &f));

        CU_ASSERT(cdc_avl_tree_erase(t, d.first) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, d.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(cdc_avl_tree_size(t) == 3);
        CU_ASSERT(avl_tree_key_int_eq(t, 3, &g, &e, &f));

        CU_ASSERT(cdc_avl_tree_erase(t, f.first) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, f.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(cdc_avl_tree_size(t) == 2);
        CU_ASSERT(avl_tree_key_int_eq(t, 2, &g, &e));

        CU_ASSERT(cdc_avl_tree_erase(t, e.first) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, e.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(cdc_avl_tree_size(t) == 1);
        CU_ASSERT(avl_tree_key_int_eq(t, 1, &g));

        CU_ASSERT(cdc_avl_tree_erase(t, g.first) == 1);
        CU_ASSERT(cdc_avl_tree_get(t, g.first, &value) == CDC_STATUS_NOT_FOUND);
        CU_ASSERT(cdc_avl_tree_size(t) == 0);

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_iterators()
{
        struct cdc_avl_tree *t;
        struct cdc_avl_tree_iter it1, it2;
        struct cdc_pair *arr[] = {&a, &b, &c, &d, &e, &f, &g, &h};
        bool check;
        int i;

        CU_ASSERT(cdc_avl_tree_ctorl1(&t, NULL, lt_int,
                                      &a, &b, &c, &d, &e, &f, &g, &h, NULL) == CDC_STATUS_OK);
        CU_ASSERT(cdc_avl_tree_size(t) == 8);

        check = true;
        i = 0;
        cdc_avl_tree_begin(t, &it1);
        cdc_avl_tree_end(t, &it2);
        for ( ; !cdc_avl_tree_iter_is_eq(&it1, &it2); cdc_avl_tree_iter_next(&it1)) {
                if (cdc_avl_tree_iter_key(&it1) != arr[i]->first) {
                        check = false;
                        break;
                }
                ++i;
        }
        CU_ASSERT(check == true);
        CU_ASSERT(i == cdc_avl_tree_size(t));

        check = true;
        i = cdc_avl_tree_size(t) - 1;
        cdc_avl_tree_end(t, &it1);
        cdc_avl_tree_iter_prev(&it1);
        cdc_avl_tree_begin(t, &it2);
        for ( ; !cdc_avl_tree_iter_is_eq(&it1, &it2); cdc_avl_tree_iter_prev(&it1)) {
                if (cdc_avl_tree_iter_key(&it1) != arr[i]->first) {
                        check = false;
                        break;
                }
                --i;
        }
        CU_ASSERT(check == true);
        CU_ASSERT(i == 0);

        check = true;
        i = 0;
        cdc_avl_tree_begin(t, &it1);
        while (cdc_avl_tree_iter_has_next(&it1)) {
                if (cdc_avl_tree_iter_key(&it1) != arr[i]->first) {
                        check = false;
                        break;
                }
                ++i;
                cdc_avl_tree_iter_next(&it1);
        }
        CU_ASSERT(check == true);
        CU_ASSERT(i == cdc_avl_tree_size(t));

        check = true;
        i = cdc_avl_tree_size(t) - 1;
        cdc_avl_tree_end(t, &it1);
        cdc_avl_tree_iter_prev(&it1);
        while (cdc_avl_tree_iter_has_prev(&it1)) {
                if (cdc_avl_tree_iter_key(&it1) != arr[i]->first) {
                        check = false;
                        break;
                }
                --i;
                cdc_avl_tree_iter_prev(&it1);
        }
        CU_ASSERT(check == true);
        CU_ASSERT(i == 0);

        cdc_avl_tree_dtor(t);
}

void test_avl_tree_swap()
{

}