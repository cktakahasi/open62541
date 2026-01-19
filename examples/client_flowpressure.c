/*
 * FPSO Water Injection System - OPC UA Client
 *
 * Connects to OPC UA server and reads/writes WIS data.
 * Demonstrates:
 *   - Reading all WIS measurements
 *   - Writing PumpEnable command
 *   - Continuous monitoring with periodic updates
 *
 * Usage:
 *   ./client_flowpressure
 *   Press Ctrl+C to disconnect
 */

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>
#include <string.h>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define OPCUA_ENDPOINT "opc.tcp://localhost:4841"

static volatile int running = 1;

static void
stopHandler(int sig) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "Received SIGINT - disconnecting");
    running = 0;
}

/* Helper function to read a double value from server */
static UA_Double
readDouble(UA_Client *client, const char *nodeId) {
    UA_Variant value;
    UA_Variant_init(&value);
    
    char nodeId_copy[256];
    strncpy(nodeId_copy, nodeId, sizeof(nodeId_copy) - 1);
    nodeId_copy[sizeof(nodeId_copy) - 1] = '\0';
    
    UA_StatusCode retval = UA_Client_readValueAttribute(client,
        UA_NODEID_STRING(1, nodeId_copy), &value);
    
    UA_Double result = 0.0;
    if(retval == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        result = *(UA_Double*)value.data;
    } else if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                       "Failed to read %s: %s", nodeId, UA_StatusCode_name(retval));
    }
    
    UA_Variant_clear(&value);
    return result;
}

/* Helper function to read a boolean value from server */
static UA_Boolean
readBoolean(UA_Client *client, const char *nodeId) {
    UA_Variant value;
    UA_Variant_init(&value);
    
    char nodeId_copy[256];
    strncpy(nodeId_copy, nodeId, sizeof(nodeId_copy) - 1);
    nodeId_copy[sizeof(nodeId_copy) - 1] = '\0';
    
    UA_StatusCode retval = UA_Client_readValueAttribute(client,
        UA_NODEID_STRING(1, nodeId_copy), &value);
    
    UA_Boolean result = false;
    if(retval == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        result = *(UA_Boolean*)value.data;
    } else if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                       "Failed to read %s: %s", nodeId, UA_StatusCode_name(retval));
    }
    
    UA_Variant_clear(&value);
    return result;
}

/* Helper function to read a byte value from server */
static UA_Byte
readByte(UA_Client *client, const char *nodeId) {
    UA_Variant value;
    UA_Variant_init(&value);
    
    char nodeId_copy[256];
    strncpy(nodeId_copy, nodeId, sizeof(nodeId_copy) - 1);
    nodeId_copy[sizeof(nodeId_copy) - 1] = '\0';
    
    UA_StatusCode retval = UA_Client_readValueAttribute(client,
        UA_NODEID_STRING(1, nodeId_copy), &value);
    
    UA_Byte result = 0;
    if(retval == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_BYTE])) {
        result = *(UA_Byte*)value.data;
    } else if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                       "Failed to read %s: %s", nodeId, UA_StatusCode_name(retval));
    }
    
    UA_Variant_clear(&value);
    return result;
}

/* Helper function to read an unsigned int value from server */
static UA_UInt32
readUInt32(UA_Client *client, const char *nodeId) {
    UA_Variant value;
    UA_Variant_init(&value);
    
    char nodeId_copy[256];
    strncpy(nodeId_copy, nodeId, sizeof(nodeId_copy) - 1);
    nodeId_copy[sizeof(nodeId_copy) - 1] = '\0';
    
    UA_StatusCode retval = UA_Client_readValueAttribute(client,
        UA_NODEID_STRING(1, nodeId_copy), &value);
    
    UA_UInt32 result = 0;
    if(retval == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_UINT32])) {
        result = *(UA_UInt32*)value.data;
    } else if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                       "Failed to read %s: %s", nodeId, UA_StatusCode_name(retval));
    }
    
    UA_Variant_clear(&value);
    return result;
}

/* Helper function to write a boolean value to server */
static UA_StatusCode
writeBoolean(UA_Client *client, const char *nodeId, UA_Boolean value) {
    UA_Variant writeValue;
    UA_Variant_setScalar(&writeValue, &value, &UA_TYPES[UA_TYPES_BOOLEAN]);
    
    char nodeId_copy[256];
    strncpy(nodeId_copy, nodeId, sizeof(nodeId_copy) - 1);
    nodeId_copy[sizeof(nodeId_copy) - 1] = '\0';
    
    return UA_Client_writeValueAttribute(client,
                                        UA_NODEID_STRING(1, nodeId_copy),
                                        &writeValue);
}

int
main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Client *client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));

    /* Connect to server */
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "Connecting to OPC UA server at %s", OPCUA_ENDPOINT);
    
    UA_StatusCode retval = UA_Client_connect(client, OPCUA_ENDPOINT);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                     "Failed to connect to server: %s", UA_StatusCode_name(retval));
        UA_Client_delete(client);
        return EXIT_FAILURE;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "Connected to OPC UA server");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "================================================");

    /* Print header */
    printf("\n");
    printf("%-8s %-12s %-12s %-12s %-12s %-12s %-12s %-8s\n",
           "Uptime", "Flow(m³/h)", "Pressure(bar)", "Temp(°C)", "Inject(m³/h)", "Pump", "ResvLvl(%)", "Filter");
    printf("%-8s %-12s %-12s %-12s %-12s %-12s %-12s %-8s\n",
           "--------", "----------", "------------", "--------", "----------", "----", "---------", "------");

    /* Main loop - read values continuously */
    while(running) {
        /* Read measurements */
        UA_Double waterFlow = readDouble(client, "WaterInjectionSystem.1.WaterFlow");
        UA_Double waterPressure = readDouble(client, "WaterInjectionSystem.1.WaterPressure");
        UA_Double temperature = readDouble(client, "WaterInjectionSystem.1.SystemTemperature");
        UA_Double injectionRate = readDouble(client, "WaterInjectionSystem.1.InjectionRate");
        UA_Double reservoirLevel = readDouble(client, "WaterInjectionSystem.1.ReservoirLevel");
        
        /* Read status */
        UA_Boolean pumpRunning = readBoolean(client, "WaterInjectionSystem.1.PumpRunning");
        UA_Byte filterStatus = readByte(client, "WaterInjectionSystem.1.FilterStatus");
        
        /* Read operating time from server */
        UA_UInt32 hours = readUInt32(client, "WaterInjectionSystem.1.OperatingTime.Hours");
        UA_Byte minutes = readByte(client, "WaterInjectionSystem.1.OperatingTime.Minutes");
        UA_Byte seconds = readByte(client, "WaterInjectionSystem.1.OperatingTime.Seconds");

        /* Calculate total seconds from server time */
        UA_UInt32 totalSeconds = (hours * 3600) + (minutes * 60) + seconds;

        /* Determine filter status string */
        const char *filterStatusStr = "Normal";
        if(filterStatus == 0) filterStatusStr = "Clean";
        else if(filterStatus == 2) filterStatusStr = "Clogged";

        /* Print data - single uptime column from server */
        printf("%02u:%02u:%02u %-12.2f %-12.2f %-12.2f %-12.2f %-12s %-12.2f %-8s\n",
               hours, minutes, seconds,
               waterFlow,
               waterPressure,
               temperature,
               injectionRate,
               pumpRunning ? "ON" : "OFF",
               reservoirLevel,
               filterStatusStr);
        fflush(stdout);

        /* Control pump at specific times (based on server uptime) */
        if(totalSeconds == 10) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                        ">>> Turning pump OFF (at %02u:%02u:%02u)", hours, minutes, seconds);
            retval = writeBoolean(client, "WaterInjectionSystem.1.PumpEnable", false);
            if(retval != UA_STATUSCODE_GOOD) {
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                             "Failed to write PumpEnable: %s", UA_StatusCode_name(retval));
            }
        }
        
        if(totalSeconds == 20) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                        ">>> Turning pump ON (at %02u:%02u:%02u)", hours, minutes, seconds);
            retval = writeBoolean(client, "WaterInjectionSystem.1.PumpEnable", true);
            if(retval != UA_STATUSCODE_GOOD) {
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                             "Failed to write PumpEnable: %s", UA_StatusCode_name(retval));
            }
        }

        sleep(1);
    }

    printf("\n");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "================================================");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "Disconnecting from server");

    UA_Client_disconnect(client);
    UA_Client_delete(client);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "Client stopped");
    return EXIT_SUCCESS;
}
