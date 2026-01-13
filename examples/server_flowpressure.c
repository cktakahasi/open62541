/*
 * FPSO Water Injection System - Simple OPC UA Simulator
 *
 * Physical Model:
 *   Pressure (bar) = 10 + (FlowRate / 10)
 *   Temperature (°C) = 40 + (FlowRate / 20)
 *   InjectionRate (m³/h) = FlowRate * 0.9
 *
 * Flow Rate varies: 0 to 200 m³/h (sine wave)
 */

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#define OPCUA_TCP_PORT 4841

static volatile UA_Boolean running = true;
static time_t startTime;

static void
stopHandler(int sig) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received SIGINT - shutting down");
    running = false;
}

static void
updateWISData(UA_Server *server) {
    time_t currentTime = time(NULL);
    time_t elapsed = currentTime - startTime;

    UA_Variant value;

    /* FLOW RATE: varies from 0 to 200 m³/h using sine wave */
    UA_Double flowRate = 100.0 + 100.0 * sin((double)elapsed / 30.0);
    UA_Variant_setScalar(&value, &flowRate, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.WaterFlow"), value);

    /* PRESSURE: calculated from flow rate (simple linear model) */
    UA_Double pressure = 10.0 + (flowRate / 10.0);
    UA_Variant_setScalar(&value, &pressure, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.WaterPressure"), value);

    /* TEMPERATURE: calculated from flow rate */
    UA_Double temperature = 40.0 + (flowRate / 20.0);
    UA_Variant_setScalar(&value, &temperature, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.SystemTemperature"), value);

    /* INJECTION RATE: 90% of flow rate */
    UA_Double injectionRate = flowRate * 0.9;
    UA_Variant_setScalar(&value, &injectionRate, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.InjectionRate"), value);

    /* PUMP RUNNING: true when flow > 5 m³/h */
    UA_Boolean pumpRunning = (flowRate > 5.0);
    UA_Variant_setScalar(&value, &pumpRunning, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.PumpRunning"), value);

    /* RESERVOIR LEVEL: decreases with injection */
    UA_Double reservoirLevel = 100.0 - (elapsed / 3600.0);
    if(reservoirLevel < 0.0) reservoirLevel = 0.0;
    UA_Variant_setScalar(&value, &reservoirLevel, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.ReservoirLevel"), value);

    /* FILTER STATUS: 0=clean, 1=normal, 2=clogged (based on pressure) */
    UA_Byte filterStatus = 1;
    if(pressure > 30.0) filterStatus = 2;
    else if(pressure < 12.0) filterStatus = 0;
    UA_Variant_setScalar(&value, &filterStatus, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.FilterStatus"), value);

    /* OPERATING TIME */
    UA_UInt32 hours = (UA_UInt32)(elapsed / 3600);
    UA_Variant_setScalar(&value, &hours, &UA_TYPES[UA_TYPES_UINT32]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Hours"), value);

    UA_Byte minutes = (UA_Byte)((elapsed % 3600) / 60);
    UA_Variant_setScalar(&value, &minutes, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Minutes"), value);

    UA_Byte seconds = (UA_Byte)(elapsed % 60);
    UA_Variant_setScalar(&value, &seconds, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Seconds"), value);
}

static void
addWISVariables(UA_Server *server) {
    UA_StatusCode retval = UA_STATUSCODE_GOOD;

    /* Add WIS folder */
    UA_ObjectAttributes objAttr = UA_ObjectAttributes_default;
    objAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Water Injection System");
    retval |= UA_Server_addObjectNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(1, "WaterInjectionSystem"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                     objAttr, NULL, NULL);

    /* Add WIS.1 instance */
    objAttr = UA_ObjectAttributes_default;
    objAttr.displayName = UA_LOCALIZEDTEXT("en-US", "WIS Unit 1");
    retval |= UA_Server_addObjectNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                     UA_NODEID_STRING(1, "WaterInjectionSystem"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(1, "WIS.1"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                     objAttr, NULL, NULL);

    /* Helper macro to add double variable */
    #define ADD_DOUBLE(nodeid, displayname) { \
        UA_VariableAttributes varAttr = UA_VariableAttributes_default; \
        UA_Double val = 0.0; \
        varAttr.displayName = UA_LOCALIZEDTEXT("en-US", displayname); \
        UA_Variant_setScalar(&varAttr.value, &val, &UA_TYPES[UA_TYPES_DOUBLE]); \
        retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, nodeid), \
                                           UA_NODEID_STRING(1, "WaterInjectionSystem.1"), \
                                           UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), \
                                           UA_QUALIFIEDNAME(1, nodeid), \
                                           UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), \
                                           varAttr, NULL, NULL); \
    }

    ADD_DOUBLE("WaterInjectionSystem.1.WaterFlow", "Water Flow (m³/h)");
    ADD_DOUBLE("WaterInjectionSystem.1.WaterPressure", "Water Pressure (bar)");
    ADD_DOUBLE("WaterInjectionSystem.1.SystemTemperature", "Temperature (°C)");
    ADD_DOUBLE("WaterInjectionSystem.1.InjectionRate", "Injection Rate (m³/h)");
    ADD_DOUBLE("WaterInjectionSystem.1.ReservoirLevel", "Reservoir Level (%)");

    /* Add Boolean variable */
    UA_VariableAttributes varAttr = UA_VariableAttributes_default;
    UA_Boolean boolVal = false;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Pump Running");
    UA_Variant_setScalar(&varAttr.value, &boolVal, &UA_TYPES[UA_TYPES_BOOLEAN]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.PumpRunning"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "PumpRunning"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    /* Add Byte variable */
    varAttr = UA_VariableAttributes_default;
    UA_Byte byteVal = 1;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Filter Status");
    UA_Variant_setScalar(&varAttr.value, &byteVal, &UA_TYPES[UA_TYPES_BYTE]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.FilterStatus"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "FilterStatus"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    /* Add OperatingTime object */
    objAttr = UA_ObjectAttributes_default;
    objAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Operating Time");
    retval |= UA_Server_addObjectNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime"),
                                     UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_QUALIFIEDNAME(1, "OperatingTime"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                     objAttr, NULL, NULL);

    /* Add Hours, Minutes, Seconds */
    UA_UInt32 uintVal = 0;
    varAttr = UA_VariableAttributes_default;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Hours");
    UA_Variant_setScalar(&varAttr.value, &uintVal, &UA_TYPES[UA_TYPES_UINT32]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Hours"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "Hours"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    byteVal = 0;
    varAttr = UA_VariableAttributes_default;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Minutes");
    UA_Variant_setScalar(&varAttr.value, &byteVal, &UA_TYPES[UA_TYPES_BYTE]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Minutes"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "Minutes"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    varAttr = UA_VariableAttributes_default;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Seconds");
    UA_Variant_setScalar(&varAttr.value, &byteVal, &UA_TYPES[UA_TYPES_BYTE]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Seconds"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "Seconds"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to add WIS variables: %s", UA_StatusCode_name(retval));
    }
}

int
main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    startTime = time(NULL);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    UA_StatusCode retval = UA_ServerConfig_setMinimal(UA_Server_getConfig(server), OPCUA_TCP_PORT, NULL);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to set server config: %s", UA_StatusCode_name(retval));
        UA_Server_delete(server);
        return EXIT_FAILURE;
    }

    addWISVariables(server);

    retval = UA_Server_run_startup(server);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to start server: %s", UA_StatusCode_name(retval));
        UA_Server_delete(server);
        return EXIT_FAILURE;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "OPC UA Water Injection System Server started on opc.tcp://localhost:%d", OPCUA_TCP_PORT);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Simple Model: Pressure = 10 + (Flow/10), Temperature = 40 + (Flow/20)");

    while(running) {
        updateWISData(server);
        UA_Server_run_iterate(server, true);
        usleep(100000);
    }

    UA_Server_run_shutdown(server);
    UA_Server_delete(server);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Server stopped");
    return EXIT_SUCCESS;
}
