#include "bt-embedded/data_matcher.h"

#include <gtest/gtest.h>

TEST(DataMatcher, testInit)
{
    BteDataMatcher matcher;

    bte_data_matcher_init(&matcher);
    /* An empty matcher should match everything */
    bool matches = bte_data_matcher_compare(&matcher, "Hello", 5);
    ASSERT_TRUE(matches);
    matches = bte_data_matcher_compare(&matcher, "Hello", 0);
    ASSERT_TRUE(matches);

    ASSERT_TRUE(bte_data_matcher_is_empty(&matcher));
}

TEST(DataMatcher, testOneRule)
{
    BteDataMatcher matcher;

    bte_data_matcher_init(&matcher);
    bool ok = bte_data_matcher_add_rule(&matcher, "hello", 5, 2);
    ASSERT_TRUE(ok);
    ASSERT_FALSE(bte_data_matcher_is_empty(&matcher));

    ok = bte_data_matcher_compare(&matcher, "My hello world", 14);
    ASSERT_FALSE(ok);
    ok = bte_data_matcher_compare(&matcher, "A Hello", 7);
    ASSERT_FALSE(ok);
    ok = bte_data_matcher_compare(&matcher, "A hello", 7);
    ASSERT_TRUE(ok);
}

TEST(DataMatcher, testTwoRules)
{
    BteDataMatcher matcher;

    bte_data_matcher_init(&matcher);
    bool ok = bte_data_matcher_add_rule(&matcher, "hello", 5, 2);
    ASSERT_TRUE(ok);
    ok = bte_data_matcher_add_rule(&matcher, "A", 1, 0);
    ASSERT_TRUE(ok);

    ok = bte_data_matcher_compare(&matcher, "My hello world", 14);
    ASSERT_FALSE(ok);
    ok = bte_data_matcher_compare(&matcher, "A Hello", 7);
    ASSERT_FALSE(ok);
    ok = bte_data_matcher_compare(&matcher, "a hello", 7);
    ASSERT_FALSE(ok);
    ok = bte_data_matcher_compare(&matcher, "A hello", 7);
    ASSERT_TRUE(ok);
}

TEST(DataMatcher, testAddOverflow)
{
    BteDataMatcher matcher;

    bte_data_matcher_init(&matcher);
    /* Overflow in a single add */
    bool ok = bte_data_matcher_add_rule(&matcher, "hello", 200, 2);
    ASSERT_FALSE(ok);

    /* Overflow on the second rule */
    ok = bte_data_matcher_add_rule(&matcher, "Hi!", 3, 0);
    ASSERT_TRUE(ok);
    ok = bte_data_matcher_add_rule(&matcher, "Hi!", 200, 0);
    ASSERT_FALSE(ok);
}

TEST(DataMatcher, testCompareOverflow)
{
    BteDataMatcher matcher;

    bte_data_matcher_init(&matcher);
    bool ok = bte_data_matcher_add_rule(&matcher, "hello", 5, 2);
    ASSERT_TRUE(ok);

    ok = bte_data_matcher_compare(&matcher, "he", 2);
    ASSERT_FALSE(ok);
}

TEST(DataMatcher, testIsSame)
{
    BteDataMatcher a, b;

    bte_data_matcher_init(&a);
    bte_data_matcher_init(&b);
    ASSERT_TRUE(bte_data_matcher_is_same(&a, &b));

    bte_data_matcher_add_rule(&a, "hello", 5, 2);
    ASSERT_FALSE(bte_data_matcher_is_same(&a, &b));

    bte_data_matcher_add_rule(&b, "hillo", 5, 2);
    ASSERT_FALSE(bte_data_matcher_is_same(&a, &b));

    bte_data_matcher_init(&b);
    bte_data_matcher_add_rule(&b, "hello", 4, 2);
    ASSERT_FALSE(bte_data_matcher_is_same(&a, &b));

    bte_data_matcher_init(&b);
    bte_data_matcher_add_rule(&b, "hello", 5, 1);
    ASSERT_FALSE(bte_data_matcher_is_same(&a, &b));

    bte_data_matcher_init(&b);
    bte_data_matcher_add_rule(&b, "hello", 5, 2);
    ASSERT_TRUE(bte_data_matcher_is_same(&a, &b));

    /* Try with two rules */
    bte_data_matcher_add_rule(&a, "hi", 2, 12);
    ASSERT_FALSE(bte_data_matcher_is_same(&a, &b));

    bte_data_matcher_add_rule(&b, "he", 2, 12);
    ASSERT_FALSE(bte_data_matcher_is_same(&a, &b));

    bte_data_matcher_copy(&b, &a);
    ASSERT_TRUE(bte_data_matcher_is_same(&a, &b));
}
