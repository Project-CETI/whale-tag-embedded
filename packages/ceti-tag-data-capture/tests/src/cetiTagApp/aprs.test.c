#include <unity.h>

#include "cetiTagApp/aprs.h"

void test_callsign_try_from_str(void){
    int result = 0;
    APRSCallsign cs = {};
    
    APRSCallsign expected_cs =  {.callsign = "KC1TUJ", .ssid = 3};
    result = callsign_try_from_str(&cs, "KC1TUJ-3", NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING(expected_cs.callsign, cs.callsign);
    TEST_ASSERT_EQUAL_UINT8(expected_cs.ssid, cs.ssid);

    expected_cs =  (APRSCallsign){.callsign = "J75Y", .ssid = 0};
    result = callsign_try_from_str(&cs, " J75Y", NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING(expected_cs.callsign, cs.callsign);
    TEST_ASSERT_EQUAL_UINT8(expected_cs.ssid, cs.ssid);

    expected_cs =  (APRSCallsign){.callsign = "J75Z", .ssid = 0};
    result = callsign_try_from_str(&cs, " J75Z", NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_STRING(expected_cs.callsign, cs.callsign);
    TEST_ASSERT_EQUAL_UINT8(expected_cs.ssid, cs.ssid);

    result = callsign_try_from_str(&cs, "J75Y-16", NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);

    result = callsign_try_from_str(&cs, "_STA", NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_callsign_to_str(void){
    APRSCallsign cs1 = {
        .callsign = "KC1TUJ",
        .ssid = 12,
    };
    char result1[10];
    callsign_to_str(&cs1, result1);
    TEST_ASSERT_EQUAL_STRING("KC1TUJ-12", result1);

    APRSCallsign cs2 = {
        .callsign = "J75Z",
        .ssid = 0,
    };
    char result2[10];
    callsign_to_str(&cs2, result2);
    TEST_ASSERT_EQUAL_STRING("J75Z", result2);
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
    RUN_TEST(test_callsign_try_from_str);
    RUN_TEST(test_callsign_to_str);
    return UNITY_END();
}