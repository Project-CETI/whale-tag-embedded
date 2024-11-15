#include <unity.h>

#include "cetiTagApp/utils/error.h"
#include "cetiTagApp/utils/timing.h"

int g_exit = 0;
int g_stopAcquisition = 0;
int g_stopLogging = 0;

void test__get_next_time_of_day_occurance_s__month_overflow(void) {
    // set fake_time
    struct tm now = {
        .tm_year = 2024,
        .tm_mon = 11,
        .tm_mday = 31,
        .tm_hour = 23,
        .tm_min = 59,
    };

    struct tm target = {
        .tm_hour = 16,
        .tm_min = 05,
    };

    set_fake_time(&now);
    time_t result = get_next_time_of_day_occurance_s(&target);
    struct tm *result_tm = gmtime(&result);

    TEST_ASSERT_EQUAL_INT(2025, result_tm->tm_year);
    TEST_ASSERT_EQUAL_INT(0, result_tm->tm_mon);
    TEST_ASSERT_EQUAL_INT(1, result_tm->tm_mday);
    TEST_ASSERT_EQUAL_INT(16, result_tm->tm_hour);
    TEST_ASSERT_EQUAL_INT(05, result_tm->tm_min);
    // get_next_time_of_day_occrance
}

void test__get_next_time_of_day_occurance_s__day_overflow(void) {
    // set fake_time
    struct tm now = {
        .tm_year = 2024,
        .tm_mon = 0,
        .tm_mday = 31,
        .tm_hour = 23,
        .tm_min = 59,
    };

    struct tm target = {
        .tm_hour = 16,
        .tm_min = 05,
    };

    set_fake_time(&now);
    time_t result = get_next_time_of_day_occurance_s(&target);
    struct tm *result_tm = gmtime(&result);

    TEST_ASSERT_EQUAL_INT(2024, result_tm->tm_year);
    TEST_ASSERT_EQUAL_INT(1, result_tm->tm_mon);
    TEST_ASSERT_EQUAL_INT(1, result_tm->tm_mday);
    TEST_ASSERT_EQUAL_INT(16, result_tm->tm_hour);
    TEST_ASSERT_EQUAL_INT(05, result_tm->tm_min);
    // get_next_time_of_day_occrance
}

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test__get_next_time_of_day_occurance_s__day_overflow);
    RUN_TEST(test__get_next_time_of_day_occurance_s__month_overflow);
    return UNITY_END();
}
