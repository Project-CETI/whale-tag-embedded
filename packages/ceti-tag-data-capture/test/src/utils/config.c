#include "unity.h"


#include "utils/config.h"

void test_config_strtobools_s(void){
    const char *input[] = {
        "True",
        "TRUE",
        "true",
        "tabby_cat",
        "false"
    };
    int expected[] = {
        1, 1, 1, 0 ,0
    };

    int result[sizeof(input)/sizeof(input[0])] = {};

    for(int i; i < sizeof(result)/sizeof(result[0]); i++){
        result[i] = strtobool_s(input[i], NULL);
    }
    
    TEST_ASSERT_EQUAL_INT_ARRAY(expected, result, sizeof(expected)/sizeof(expected[0]));

}

void test_config_strtotime_s(void){
    const char *input[] = {
        "1d", "2D", "3h", "4H", "5m", "6M", "7s", "8S", "9", "\t10 s", "bob"
    };
    time_t expected[] = {
        1*24*60*60, //days
        2*24*60*60, 
        3*60*60,    //hours
        4*60*60,    
        5*60,       //min
        6*60,
        7,          //sec
        8,
        9*60,       //default
        10,         //whitespace
        0, //error
    };
    time_t result[sizeof(input)/sizeof(input[0])] = {};

    for(int i; i < sizeof(result)/sizeof(result[0]); i++){
        result[i] = strtotime_s(input[i], NULL);
    }

    TEST_ASSERT_EQUAL_INT64_ARRAY(expected, result, sizeof(expected)/sizeof(expected[0]));
}

void test_config_parse_line(void){
    int result = 0;

    // ok line
    g_config.dive_pressure = 0.0;
    result = config_parse_line("P2=0.10 #Pressure Test");
    TEST_ASSERT_EQUAL_FLOAT(0.10, g_config.dive_pressure);
    TEST_ASSERT_EQUAL_INT(0, result);

    // comment
    result = config_parse_line("#Comment line");
    TEST_ASSERT_EQUAL_INT(0, result);

    //empty
    result = config_parse_line("");
    TEST_ASSERT_EQUAL_INT(0, result);

    //unknown key
    result = config_parse_line("UnknownKey = 10");
    TEST_ASSERT_EQUAL_INT(1, result);

    //unable to parse value
    result = config_parse_line("P2=asdf");
    TEST_ASSERT_EQUAL_INT(2, result);

    //missing operator
    result = config_parse_line("P2  0.10 #Pressure Test");
    TEST_ASSERT_EQUAL_INT(3, result);
}

void test_config_backwards_compatible(void){
    g_config = (TagConfig){}; //reset g_config to 0's
    config_read("resource/legacy-config.txt");
    TEST_ASSERT_EQUAL_FLOAT(0.05, g_config.surface_pressure);
    TEST_ASSERT_EQUAL_FLOAT(0.11, g_config.dive_pressure);
    TEST_ASSERT_EQUAL_FLOAT(6.5, g_config.release_voltage_v);
    TEST_ASSERT_EQUAL_FLOAT(6.3, g_config.critical_voltage_v);
    TEST_ASSERT_EQUAL_INT64(4*24*60*60, g_config.timeout_s);
    TEST_ASSERT_EQUAL_INT64(20*60, g_config.burn_interval_s);
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
    RUN_TEST(test_config_strtobools_s);
    RUN_TEST(test_config_strtotime_s);
    RUN_TEST(test_config_parse_line);
    RUN_TEST(test_config_backwards_compatible);
    return UNITY_END();
}