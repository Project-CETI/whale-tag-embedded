#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unity.h>


#include "cetiTagApp/cetiTag.h"
#include "cetiTagApp/state_machine.h"
#include "cetiTagApp/utils/config.h"

/* dependencies */
CetiPressureSample fake_pressure_sample = {};
CetiBatterySample  fake_battery_sample = {};

CetiPressureSample *g_pressure = &fake_pressure_sample;
CetiBatterySample *shm_battery = &fake_battery_sample;


int g_stateMachine_thread_tid;
int g_exit = 0;
int g_stopAcquisition = 0; 
int g_stopLogging = 0; 

TagConfig g_config = {
    .audio = {
        .filter_type = CONFIG_DEFAULT_AUDIO_FILTER_TYPE,
        .sample_rate = CONFIG_DEFAULT_AUDIO_SAMPLE_RATE,
        .bit_depth = CONFIG_DEFAULT_AUDIO_SAMPLE_RATE,
    },
    .surface_pressure   = CONFIG_DEFAULT_SURFACE_PRESSURE_BAR, // depth_m is roughly 10*pressure_bar
    .dive_pressure      = CONFIG_DEFAULT_DIVE_PRESSURE_BAR, // depth_m is roughly 10*pressure_bar
    .release_voltage_v  = CONFIG_DEFAULT_RELEASE_VOLTAGE_V,
    .critical_voltage_v = CONFIG_DEFAULT_CRITICAL_VOLTAGE_V,
    .timeout_s          = CONFIG_DEFAULT_TIMEOUT_S,
    .burn_interval_s    = CONFIG_DEFAULT_BURN_INTERVAL_S,
    .recovery           = {
        .enabled = CONFIG_DEFAULT_RECOVERY_ENABLED,
        .freq_MHz = CONFIG_DEFAULT_RECOVERY_FREQUENCY_MHZ,
        .callsign = {
            .callsign = CONFIG_DEFAULT_RECOVERY_CALLSIGN,
            .ssid = CONFIG_DEFAULT_RECOVERY_SSID,
        },
        .recipient = {
            .callsign = CONFIG_DEFAULT_RECOVERY_RECIPIENT_CALLSIGN,
            .ssid = CONFIG_DEFAULT_RECOVERY_RECIPIENT_SSID,
        },
    },
};

int64_t get_global_time_us() {
  struct timeval current_timeval;
  int64_t current_time_us;

  gettimeofday(&current_timeval, NULL);
  current_time_us = (int64_t)(current_timeval.tv_sec * 1000000LL) +
                    (int64_t)(current_timeval.tv_usec);
  return current_time_us;
}

int getRtcCount() {
    return get_global_time_us()/1000;
}

/******************************************* TESTS *******************************************/
#define FUZZY_COUNT 10000

// ST_CONFIG -> ST_START
void test__updateStateMachine_ST_CONFIG(void){
    //test should always transition to ST_START
    stateMachine_set_state(ST_CONFIG);
    updateStateMachine();
    TEST_ASSERT_EQUAL(ST_START, stateMachine_get_state());
}

// ST_CONFIG -> ST_RECORD_SURFACE
void test__updateStateMachine_ST_START_lowPressure(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_START);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 2.0*g_config.dive_pressure - g_config.dive_pressure);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_RECORD_SURFACE, stateMachine_get_state());
    }
}

// ST_CONFIG -> ST_RECORD_DIVING
void test__updateStateMachine_ST_START_highPressure(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_START);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 1000.0 + g_config.dive_pressure + 0.00000001);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_RECORD_DIVING, stateMachine_get_state());
    }
}

// ST_RECORD_DIVING -> ST_RECORD_SURFACE
void test__updateStateMachine_ST_RECORD_DIVING_lowPressure_okBattery_okTime(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_DIVING);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 2.0*g_config.surface_pressure - g_config.surface_pressure);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_RECORD_SURFACE, stateMachine_get_state());
    }
}

// ST_RECORD_DIVING -> ST_RECORD_DIVING
void test__updateStateMachine_ST_RECORD_DIVING_highPressure_okBattery_okTime(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_DIVING);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 1000.0 + g_config.surface_pressure + 0.00000001);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_RECORD_DIVING, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_RECORD_DIVING_lowBattery_okTime(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_DIVING);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 4.0*g_config.surface_pressure - g_config.surface_pressure);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * (g_config.release_voltage_v/2));
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * (g_config.release_voltage_v/2));
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_BRN_ON, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_RECORD_DIVING_timeup(void){
    g_config.timeout_s = 1;
    stateMachine_set_state(ST_START);
    updateStateMachine();
    sleep(1); //to ensure timeout
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_DIVING);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 4.0*g_config.surface_pressure - g_config.surface_pressure);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * 4.2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * 4.2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_BRN_ON, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_RECORD_SURFACE_lowPressure_okBattery_okTime(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_SURFACE);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 2.0*g_config.dive_pressure - g_config.dive_pressure);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_RECORD_SURFACE, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_RECORD_SURFACE_highPressure_okBattery_okTime(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_SURFACE);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 1000.0 + g_config.dive_pressure + 0.00000001);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.release_voltage_v/2) + g_config.release_voltage_v/2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_RECORD_DIVING, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_RECORD_SURFACE_lowBattery_okTime(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_SURFACE);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 4.0*g_config.dive_pressure - g_config.dive_pressure);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * (g_config.release_voltage_v/2));
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * (g_config.release_voltage_v/2));
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_BRN_ON, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_RECORD_SURFACE_timeup(void){
    g_config.timeout_s = 1;
    stateMachine_set_state(ST_START);
    updateStateMachine();
    sleep(1); //to ensure timeout
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RECORD_SURFACE);
        fake_pressure_sample.pressure_bar = ((double)rand()/(double)RAND_MAX * 4.0*g_config.dive_pressure - g_config.dive_pressure);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * 4.2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * 4.2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_BRN_ON, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_BRN_ON_noTimeup_okBattery(void){
    g_config.burn_interval_s = 3;
    stateMachine_update_rtc_count();
    stateMachine_set_state(ST_BRN_ON);
    fake_battery_sample.cell_voltage_v[0] = 4.2;
    fake_battery_sample.cell_voltage_v[1] = 4.2;
    stateMachine_update_rtc_count();
    updateStateMachine();
    TEST_ASSERT_EQUAL(ST_BRN_ON, stateMachine_get_state());
}

void test__updateStateMachine_ST_BRN_ON_timeup_okBattery(void){
    g_config.burn_interval_s = 2;
    stateMachine_update_rtc_count();
    stateMachine_set_state(ST_BRN_ON);
    fake_battery_sample.cell_voltage_v[0] = 4.2;
    fake_battery_sample.cell_voltage_v[1] = 4.2;
    sleep(2);
    stateMachine_update_rtc_count();
    updateStateMachine();
    TEST_ASSERT_EQUAL(ST_RETRIEVE, stateMachine_get_state());
}

void test__updateStateMachine_ST_RETRIEVE_okBattery(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RETRIEVE);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.critical_voltage_v/2) + g_config.critical_voltage_v/2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * (4.2 - g_config.critical_voltage_v/2) + g_config.critical_voltage_v/2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_RETRIEVE, stateMachine_get_state());
    }
}

void test__updateStateMachine_ST_RETRIEVE_criticalBattery(void){
    for(int i = 0; i < FUZZY_COUNT; i++){
        stateMachine_set_state(ST_RETRIEVE);
        fake_battery_sample.cell_voltage_v[0] = ((double)rand()/(double)RAND_MAX * g_config.critical_voltage_v/2);
        fake_battery_sample.cell_voltage_v[1] = ((double)rand()/(double)RAND_MAX * g_config.critical_voltage_v/2);
        updateStateMachine();
        TEST_ASSERT_EQUAL(ST_SHUTDOWN, stateMachine_get_state());
    }
}

void test_strtomissionstate(void) {
    //normal identifiers
    TEST_ASSERT_EQUAL(ST_CONFIG, strtomissionstate("CONFIG", NULL));
    TEST_ASSERT_EQUAL(ST_START, strtomissionstate("START", NULL));
    TEST_ASSERT_EQUAL(ST_DEPLOY, strtomissionstate("DEPLOY", NULL));
    TEST_ASSERT_EQUAL(ST_RECORD_DIVING, strtomissionstate("RECORD_DIVING", NULL));
    TEST_ASSERT_EQUAL(ST_RECORD_SURFACE, strtomissionstate("RECORD_SURFACE", NULL));
    TEST_ASSERT_EQUAL(ST_BRN_ON, strtomissionstate("BRN_ON", NULL));
    TEST_ASSERT_EQUAL(ST_RETRIEVE, strtomissionstate("RETRIEVE", NULL));
    TEST_ASSERT_EQUAL(ST_SHUTDOWN, strtomissionstate("SHUTDOWN", NULL));
    TEST_ASSERT_EQUAL(ST_UNKNOWN, strtomissionstate("asdfasdlsdfk", NULL));

    //numbers
    TEST_ASSERT_EQUAL(ST_CONFIG, strtomissionstate("0", NULL));
    TEST_ASSERT_EQUAL(ST_START, strtomissionstate("1", NULL));
    TEST_ASSERT_EQUAL(ST_DEPLOY, strtomissionstate("2", NULL));
    TEST_ASSERT_EQUAL(ST_RECORD_DIVING, strtomissionstate("3", NULL));
    TEST_ASSERT_EQUAL(ST_RECORD_SURFACE, strtomissionstate("4", NULL));
    TEST_ASSERT_EQUAL(ST_BRN_ON, strtomissionstate("5", NULL));
    TEST_ASSERT_EQUAL(ST_RETRIEVE, strtomissionstate("6", NULL));
    TEST_ASSERT_EQUAL(ST_SHUTDOWN, strtomissionstate("7", NULL));
    TEST_ASSERT_EQUAL(ST_UNKNOWN, strtomissionstate("21", NULL));

    //whitespace
    TEST_ASSERT_EQUAL(ST_CONFIG, strtomissionstate("  CONFIG", NULL));

    //consecutive
    const char *end_ptr = NULL;
    TEST_ASSERT_EQUAL(ST_CONFIG, strtomissionstate("CONFIG 2 CONFIG", &end_ptr));
    TEST_ASSERT_EQUAL(ST_DEPLOY, strtomissionstate(end_ptr, &end_ptr));
    TEST_ASSERT_EQUAL(ST_CONFIG, strtomissionstate(end_ptr, &end_ptr));
    TEST_ASSERT_EQUAL(ST_UNKNOWN, strtomissionstate(end_ptr, &end_ptr)); //empty string
}

void setUp(void) {
    // set stuff up here
    srand(time(NULL));
    
    //Update burnwire to far in the future
    g_config.timeout_s = 0xFFFFFFFF;
    stateMachine_set_state(ST_START);
    updateStateMachine();
}

void tearDown(void) {
    // clean stuff up here
}

int main(void) {
    UNITY_BEGIN();
    printf("Running state transition tests\n");
    RUN_TEST(test__updateStateMachine_ST_CONFIG);
    RUN_TEST(test__updateStateMachine_ST_START_lowPressure);
    RUN_TEST(test__updateStateMachine_ST_START_highPressure);
    RUN_TEST(test__updateStateMachine_ST_RECORD_DIVING_timeup);
    RUN_TEST(test__updateStateMachine_ST_RECORD_DIVING_lowBattery_okTime);
    RUN_TEST(test__updateStateMachine_ST_RECORD_DIVING_highPressure_okBattery_okTime);
    RUN_TEST(test__updateStateMachine_ST_RECORD_DIVING_lowPressure_okBattery_okTime);
    RUN_TEST(test__updateStateMachine_ST_RECORD_SURFACE_timeup);
    RUN_TEST(test__updateStateMachine_ST_RECORD_SURFACE_lowBattery_okTime);
    RUN_TEST(test__updateStateMachine_ST_RECORD_SURFACE_highPressure_okBattery_okTime);
    RUN_TEST(test__updateStateMachine_ST_RECORD_SURFACE_lowPressure_okBattery_okTime);
    RUN_TEST(test__updateStateMachine_ST_BRN_ON_noTimeup_okBattery);
    RUN_TEST(test__updateStateMachine_ST_BRN_ON_timeup_okBattery);
    RUN_TEST(test__updateStateMachine_ST_RETRIEVE_okBattery);
    RUN_TEST(test__updateStateMachine_ST_RETRIEVE_criticalBattery);

    printf("\nState string parsing tests\n");
    RUN_TEST(test_strtomissionstate);

    return UNITY_END();
}