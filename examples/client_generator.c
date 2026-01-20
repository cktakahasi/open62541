#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>

#include "common.h"

#include <signal.h>
#include <stdlib.h>
#include <time.h>

static volatile UA_Boolean running = true;

static void
stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received ctrl-c");
    running = false;
}

static UA_StatusCode
writeActivePower(UA_Client *client, UA_Double powerValue) {
    UA_Variant value;
    UA_Variant_setScalar(&value, &powerValue, &UA_TYPES[UA_TYPES_DOUBLE]);
    
    UA_StatusCode retval = UA_Client_writeValueAttribute(client,
                                                        UA_NODEID_STRING(1, "DieselGenerator.1.ActivePower"),
                                                        &value);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to write ActivePower: %s", UA_StatusCode_name(retval));
    }
    
    return retval;
}

static void
readGeneratorData(UA_Client *client) {
    UA_Variant value;
    UA_StatusCode retval;

    /* Read Active Power */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.ActivePower"),
                                          &value);
    UA_Double activePower = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        activePower = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Engine Speed */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.EngineSpeed"),
                                          &value);
    UA_Double speed = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        speed = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Voltage */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.Voltage"),
                                          &value);
    UA_Double voltage = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        voltage = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Frequency */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.Frequency"),
                                          &value);
    UA_Double frequency = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        frequency = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Oil Pressure */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.OilPressure"),
                                          &value);
    UA_Double oilPressure = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        oilPressure = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Coolant Temperature */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.CoolantTemperature"),
                                          &value);
    UA_Double coolantTemp = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        coolantTemp = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Fuel Level */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.FuelLevel"),
                                          &value);
    UA_Double fuelLevel = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        fuelLevel = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Battery Voltage */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.BatteryVoltage"),
                                          &value);
    UA_Double batteryVoltage = 0.0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        batteryVoltage = *(UA_Double *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Operating Time - Hours */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Hours"),
                                          &value);
    UA_UInt32 hours = 0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_UINT32])) {
        hours = *(UA_UInt32 *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Operating Time - Minutes */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Minutes"),
                                          &value);
    UA_Byte minutes = 0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_BYTE])) {
        minutes = *(UA_Byte *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Operating Time - Seconds */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Seconds"),
                                          &value);
    UA_Byte seconds = 0;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_BYTE])) {
        seconds = *(UA_Byte *)value.data;
    }
    UA_Variant_clear(&value);

    /* Read Running Status */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.Running"),
                                          &value);
    UA_Boolean isRunning = false;
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        isRunning = *(UA_Boolean *)value.data;
    }
    UA_Variant_clear(&value);

    /* Display all values */
    printf("Uptime: %02u:%02u:%02u | Running: %s | Power: %.1f kW | RPM: %.0f\n",
           hours, minutes, seconds, isRunning ? "YES" : "NO", activePower, speed);
    printf("Voltage: %.1f V | Frequency: %.1f Hz | OilPres: %.2f bar\n",
           voltage, frequency, oilPressure);
    printf("Coolant: %.1f°C | Fuel: %.1f%% | Battery: %.2f V\n",
           coolantTemp, fuelLevel, batteryVoltage);
    printf("================================================\n");
}

int
main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Client *client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));

    UA_StatusCode retval = UA_Client_connect(client, "opc.tcp://localhost:4840");
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Could not connect to server: %s",
                     UA_StatusCode_name(retval));
        UA_Client_delete(client);
        return EXIT_FAILURE;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "================================================");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Connected to Diesel Generator OPC UA Server");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "================================================\n");

    time_t startTime = time(NULL);
    int totalSeconds = 0;

    while(running) {
        time_t currentTime = time(NULL);
        totalSeconds = (int)(currentTime - startTime);

        /* Simulate different load scenarios */
        if(totalSeconds == 5) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        ">>> Setting ActivePower to 250 kW (at %d sec)", totalSeconds);
            writeActivePower(client, 250.0);
        }

        if(totalSeconds == 10) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        ">>> Setting ActivePower to 750 kW (at %d sec)", totalSeconds);
            writeActivePower(client, 750.0);
        }

        if(totalSeconds == 15) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        ">>> Setting ActivePower to 1250 kW (at %d sec)", totalSeconds);
            writeActivePower(client, 1250.0);
        }

        if(totalSeconds == 20) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        ">>> Setting ActivePower to 500 kW (at %d sec)", totalSeconds);
            writeActivePower(client, 500.0);
        }

        if(totalSeconds == 25) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        ">>> Setting ActivePower to 0 kW (Shutdown) (at %d sec)", totalSeconds);
            writeActivePower(client, 0.0);
        }

        if(totalSeconds == 30) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        ">>> Restarting cycle\n");
            startTime = time(NULL);
            totalSeconds = 0;
        }

        readGeneratorData(client);
        sleep_ms(1000);
    }

    printf("\n");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "================================================");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Disconnecting from server");

    UA_Client_disconnect(client);
    UA_Client_delete(client);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Client stopped");
    return EXIT_SUCCESS;
}