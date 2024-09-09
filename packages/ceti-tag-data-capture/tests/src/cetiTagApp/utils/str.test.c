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
    return UNITY_END();
}