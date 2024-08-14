//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"

#include "../tui.h"
#include "../../cetiTagApp/cetiTag.h"

#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>

// Function to check the status of a network interface
static int checkInterfaceStatus(const char *interface) {
    int sock;
    struct ifreq ifr;

    // Create a socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        // perror("socket");
        // exit(EXIT_FAILURE);
        return 0;
    }

    // Set the interface name
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    // Get the interface flags
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        // perror("ioctl");
        close(sock);
        return 0;
        // exit(EXIT_FAILURE);
    }

    // Check if the interface is up
    close(sock);
    return (ifr.ifr_flags & IFF_UP) ? 1 : 0;
}

// Tests for WiFi and Eth0 connections
TestState test_internet(FILE *pResultsFile){
    int wlan0_pass = 0;
    int eth0_pass = 0;
    char input = 0;

    printf("Instructions: connect ethernet and wifi\n\n");

    while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0) && !(eth0_pass && wlan0_pass)){
        //test connection
        if(!wlan0_pass)
            wlan0_pass |= checkInterfaceStatus("wlan0");
        
        if (!eth0_pass)
            eth0_pass |= checkInterfaceStatus("eth0");

        //display progress
        printf("\e[4;1H\e[0Kwlan0: %s\n", wlan0_pass ? GREEN(PASS) : YELLOW("Waiting for connection..."));
        printf("\e[7;1H\e[0K eth0: %s\n", eth0_pass ? GREEN(PASS) : YELLOW("Waiting for connection..."));
        fflush(stdin);
    }

    //record results
    fprintf(pResultsFile, "[%s]: wlan0\n", wlan0_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: eth0\n", eth0_pass ? "PASS" : "FAIL");

    //wait for user to advance screen
    if(input == 0){
        do{
            ;
        }while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0));
    }
    return (input == 27) ? TEST_STATE_TERMINATE
         : (eth0_pass && wlan0_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}