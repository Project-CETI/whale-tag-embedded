//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef TESTS_H
#define TESTS_H

#include <stdio.h>
#include <unistd.h>

typedef enum {
    TEST_STATE_RUNNING,
    TEST_STATE_PASSED,
    TEST_STATE_FAILED,
    TEST_STATE_SKIPPED,
    TEST_STATE_TERMINATE,
} TestState;

typedef TestState(TestUpdateMethod)(FILE *file);

TestState test_ToDo(FILE *pResultsFiles);

TestState test_audio(FILE *pResultsFile);
TestState test_batteries(FILE *pResultsFile);
TestState test_ecg(FILE *pResultsFile);
TestState test_imu(FILE *pResultsFile);
TestState test_internet(FILE *pResultsFile);
TestState test_light(FILE *pResultsFile);
TestState test_pressure(FILE *pResultsFile);
TestState test_recovery(FILE *pResultsFile);
TestState test_temperature(FILE *pResultsFile);

#endif // TESTS_H
