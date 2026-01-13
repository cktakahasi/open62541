/*
 * FPSO Water Injection OPC UA Client
 *
 * Simple OPC UA client that connects to an OPC UA server and
 * periodically reads flow rate, pressure, and temperature values.
 *
 * Intended for testing and validation of FPSO water injection
 * simulators and OPC UA integrations.
 */

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define OPCUA_ENDPOINT "opc.tcp://localhost:4841"

static volatile int running = 1;

static void
stopHandler(int sig) {
    running = 0;
}

int main(void)
{
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Client *client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));

    printf("Connecting to OPC UA server at %s\n", OPCUA_ENDPOINT);
    
    UA_StatusCode retval = UA_Client_connect(client, OPCUA_ENDPOINT);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        printf("Failed to connect to server\n");
        return EXIT_FAILURE;
    }

    printf("Connected to OPC UA server\n");
    printf("=============================================================\n");
    printf("%-20s %-15s %-15s %-15s\n", "Time", "Flow (m³/h)", "Pressure (bar)", "Temp (°C)");
    printf("=============================================================\n");

    int iteration = 0;
    while(running) {
        UA_Variant flowValue, pressureValue, tempValue;
        UA_Variant_init(&flowValue);
        UA_Variant_init(&pressureValue);
        UA_Variant_init(&tempValue);

        /* Read Water Flow */
        retval = UA_Client_readValueAttribute(client,
            UA_NODEID_STRING(1, "WaterInjectionSystem.1.WaterFlow"), &flowValue);

        /* Read Water Pressure */
        UA_StatusCode retval2 = UA_Client_readValueAttribute(client,
            UA_NODEID_STRING(1, "WaterInjectionSystem.1.WaterPressure"), &pressureValue);

        /* Read Temperature */
        UA_StatusCode retval3 = UA_Client_readValueAttribute(client,
            UA_NODEID_STRING(1, "WaterInjectionSystem.1.SystemTemperature"), &tempValue);

        if(retval == UA_STATUSCODE_GOOD && 
           retval2 == UA_STATUSCODE_GOOD && 
           retval3 == UA_STATUSCODE_GOOD) {
            
            UA_Double flow = *(UA_Double*)flowValue.data;
            UA_Double pressure = *(UA_Double*)pressureValue.data;
            UA_Double temp = *(UA_Double*)tempValue.data;
            
            printf("[%3d] %-20d %-15.2f %-15.2f %-15.2f\n", 
                   iteration, (int)((iteration * 100) / 1000), flow, pressure, temp);
            
            iteration++;
        } else {
            printf("Read error - Flow: 0x%08x, Pressure: 0x%08x, Temp: 0x%08x\n",
                   retval, retval2, retval3);
        }

        UA_Variant_clear(&flowValue);
        UA_Variant_clear(&pressureValue);
        UA_Variant_clear(&tempValue);

        sleep(1);
    }

    printf("=============================================================\n");
    printf("Disconnecting...\n");
    UA_Client_disconnect(client);
    UA_Client_delete(client);

    return EXIT_SUCCESS;
}
