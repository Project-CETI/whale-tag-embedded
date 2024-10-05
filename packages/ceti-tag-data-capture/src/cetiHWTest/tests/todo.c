//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"

#include "../tui.h"

TestState test_ToDo(FILE *pResultsFiles) {
    // display message
    printf(YELLOW("TODO: IMPLEMENT THIS TEST!!!!!!!\n"));

    // get user input
    char input = '\0';
    while ((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)) {
        ;
    }

    if (input == 27)
        return TEST_STATE_TERMINATE;

    return TEST_STATE_PASSED;
}
