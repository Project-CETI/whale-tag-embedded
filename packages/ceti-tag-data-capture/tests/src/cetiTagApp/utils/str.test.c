#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <unity.h>

#include "cetiTagApp/utils/str.h"

void test_strtobool(void){
    //test "true"
    for(int i = 0; i <  (1 << 4); i++){
        char true_string[5 + 4 + 1] = "     true";
        for(int letter = 0; letter < 4; letter++){
            if(i & (1 << letter)){
                true_string[letter] = toupper(true_string[letter]);
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(strtobool(&true_string[5], NULL), true_string); // base string
        TEST_ASSERT_TRUE_MESSAGE(strtobool(&true_string[rand()%5], NULL), true_string); //string with leading whitespace
    }

    //test "false"
    TEST_ASSERT_FALSE(strtobool("trueness", NULL));
    TEST_ASSERT_FALSE(strtobool("tru", NULL));
    TEST_ASSERT_FALSE(strtobool("", NULL));
    TEST_ASSERT_FALSE(strtobool("_true", NULL));
    TEST_ASSERT_FALSE(strtobool("false", NULL));
    TEST_ASSERT_FALSE(strtobool("false", NULL));
    TEST_ASSERT_FALSE(strtobool("false true", NULL));

    //test end_ptr setting
    const char *end_ptr;
    TEST_ASSERT_TRUE(strtobool("true false true", &end_ptr));
    TEST_ASSERT_FALSE(strtobool(end_ptr, &end_ptr));
    TEST_ASSERT_TRUE(strtobool(end_ptr, NULL));
}


void test_strtoquotedstring(void) {
    char result_string[256];
    const char *start_ptr;
    const char *end_ptr;
    size_t len;

    start_ptr = strtoquotedstring("\"test1\"", &end_ptr);
    TEST_ASSERT_NOT_NULL(start_ptr);
    len = end_ptr - start_ptr;
    memcpy(result_string, start_ptr, len);
    result_string[len] = 0;
    TEST_ASSERT_EQUAL_STRING("\"test1\"", result_string);

    start_ptr = strtoquotedstring("   \"  test 2  \"", &end_ptr);
    TEST_ASSERT_NOT_NULL(start_ptr);
    len = end_ptr - start_ptr;
    memcpy(result_string, start_ptr, len);
    result_string[len] = 0;
    TEST_ASSERT_EQUAL_STRING("\"  test 2  \"", result_string);

    start_ptr = strtoquotedstring("asdf\"test 3\"", &end_ptr);
    TEST_ASSERT_NULL(start_ptr);

    start_ptr = strtoquotedstring("\"test 4", &end_ptr);
    TEST_ASSERT_NULL(start_ptr);

    start_ptr = strtoquotedstring("\"test\\\"5\"   \"test6\"", &end_ptr);
    TEST_ASSERT_NOT_NULL(start_ptr);
    len = end_ptr - start_ptr;
    memcpy(result_string, start_ptr, len);
    result_string[len] = 0;
    TEST_ASSERT_EQUAL_STRING("\"test\\\"5\"", result_string);

    start_ptr = strtoquotedstring(end_ptr, &end_ptr);
    TEST_ASSERT_NOT_NULL(start_ptr);
    len = end_ptr - start_ptr;
    memcpy(result_string, start_ptr, len);
    result_string[len] = 0;
    TEST_ASSERT_EQUAL_STRING("\"test6\"", result_string);
}

void setUp(void) {
    // set stuff up here
    srand(time(NULL));
}

void tearDown(void) {
    // clean stuff up here
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_strtobool);
    RUN_TEST(test_strtoquotedstring);
    return UNITY_END();
}