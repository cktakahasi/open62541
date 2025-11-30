#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>

#include "common.h"

#include <signal.h>
#include <stdlib.h>

static volatile UA_Boolean running = true;

static void
stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received ctrl-c");
    running = false;
}

static void
readGeneratorData(UA_Client *client) {
    UA_Variant value;
    UA_StatusCode retval;

    /* Read Active Power */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.ActivePower"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double activePower = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Active Power: %.2f kW", activePower);
    }
    UA_Variant_clear(&value);

    /* Read Voltage */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.Voltage"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double voltage = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Voltage: %.2f V", voltage);
    }
    UA_Variant_clear(&value);

    /* Read Frequency */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.Frequency"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double frequency = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Frequency: %.2f Hz", frequency);
    }
    UA_Variant_clear(&value);

    /* Read Engine Speed */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.EngineSpeed"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double speed = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Engine Speed: %.2f RPM", speed);
    }
    UA_Variant_clear(&value);

    /* Read Oil Pressure */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.OilPressure"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double oilPressure = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Oil Pressure: %.2f bar", oilPressure);
    }
    UA_Variant_clear(&value);

    /* Read Coolant Temperature */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.CoolantTemperature"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double coolantTemp = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Coolant Temperature: %.2f C", coolantTemp);
    }
    UA_Variant_clear(&value);

    /* Read Fuel Level */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.FuelLevel"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double fuelLevel = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Fuel Level: %.2f %%", fuelLevel);
    }
    UA_Variant_clear(&value);

    /* Read Battery Voltage */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.BatteryVoltage"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double batteryVoltage = *(UA_Double *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Battery Voltage: %.2f V", batteryVoltage);
    }
    UA_Variant_clear(&value);

    /* Read Running Hours */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.RunningHours"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_UINT32])) {
        UA_UInt32 runningHours = *(UA_UInt32 *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Running Hours: %u h", runningHours);
    }
    UA_Variant_clear(&value);

    /* Read Running Status */
    retval = UA_Client_readValueAttribute(client,
                                          UA_NODEID_STRING(1, "DieselGenerator.1.Running"),
                                          &value);
    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        UA_Boolean isRunning = *(UA_Boolean *)value.data;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Running: %s", isRunning ? "true" : "false");
    }
    UA_Variant_clear(&value);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "---");
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
                "Connected to opc.tcp://localhost:4840");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Reading Diesel Generator data...\n");

    while(running) {
        readGeneratorData(client);
        sleep_ms(2000);
    }

    UA_Client_disconnect(client);
    UA_Client_delete(client);
    return EXIT_SUCCESS;
}