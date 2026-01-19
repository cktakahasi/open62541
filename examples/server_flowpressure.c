/*
 * FPSO Water Injection System - Simple OPC UA Simulator
 *
 * Physical Model:
 *   Pressure (bar) = 10 + (FlowRate / 10)
 *   Temperature (°C) = 40 + (FlowRate / 20)
 *   InjectionRate (m³/h) = FlowRate * 0.9
 *
 * Flow Rate controlled by PumpEnable (writable setpoint)
 * 
 * Usage:
 *   ./server_flowpressure
 *   Press Ctrl+C to shutdown
 */

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <stdio.h>

#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#define OPCUA_TCP_PORT 4841
#define UPDATE_INTERVAL_MS 100000  /* 100ms */
#define FLOW_RATE_MAX 200.0
#define FLOW_RATE_MIN 0.0
#define SINE_PERIOD 30.0
#define RESERVOIR_DEPLETION_RATE 2.0  /* % per hour when pump is ON */

/* Global state */
static volatile UA_Boolean running = true;
static time_t startTime;
static UA_Boolean pumpEnable = true;
static UA_Boolean lastPumpEnable = true;  /* Track previous state */

static void
stopHandler(int sig) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, 
                "Received SIGINT - shutting down gracefully");
    running = false;
}

/* Callback invoked when PumpEnable is written by client */
static void
writePumpEnable(UA_Server *server,
                const UA_NodeId *sessionId, void *sessionContext,
                const UA_NodeId *nodeId, void *nodeContext,
                const UA_NumericRange *range,
                const UA_DataValue *value) {
    if(value == NULL || value->value.type == NULL) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "writePumpEnable: Invalid value");
        return;
    }
    
    if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        UA_Boolean newState = *(UA_Boolean*)value->value.data;
        UA_Boolean oldState = pumpEnable;
        
        pumpEnable = newState;
        lastPumpEnable = newState;
        
        /* Log state change with timestamp */
        time_t currentTime = time(NULL);
        time_t elapsed = currentTime - startTime;
        
        UA_UInt32 hours = elapsed / 3600;
        UA_UInt32 minutes = (elapsed % 3600) / 60;
        UA_UInt32 seconds = elapsed % 60;
        
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "================================================");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "*** PumpEnable WRITE CALLBACK TRIGGERED ***");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Uptime: %02u:%02u:%02u", hours, minutes, seconds);
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Old state: %s", oldState ? "ON" : "OFF");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "New state: %s", newState ? "ON" : "OFF");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "================================================");
    } else {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "writePumpEnable: Received non-boolean value");
    }
}

/* Update all WIS variables based on current state and time */
static void
updateWISData(UA_Server *server) {
    static time_t lastUpdateTime = 0;
    time_t currentTime = time(NULL);
    time_t elapsed = currentTime - startTime;

    UA_Variant value;

    /* Check if pump state changed since last update */
    if(pumpEnable != lastPumpEnable) {
        UA_UInt32 hours = elapsed / 3600;
        UA_UInt32 minutes = (elapsed % 3600) / 60;
        UA_UInt32 seconds = elapsed % 60;
        
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "================================================");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "*** Pump state DETECTED in updateWISData ***");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Uptime: %02u:%02u:%02u", hours, minutes, seconds);
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Old state: %s", lastPumpEnable ? "ON" : "OFF");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "New state: %s", pumpEnable ? "ON" : "OFF");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "================================================");
        
        lastPumpEnable = pumpEnable;
    }

    /* Calculate FLOW RATE based on pump state */
    UA_Double flowRate = FLOW_RATE_MIN;
    if(pumpEnable) {
        flowRate = (FLOW_RATE_MAX / 2.0) + 
                   (FLOW_RATE_MAX / 2.0) * sin(2.0 * M_PI * (double)elapsed / SINE_PERIOD);
        if(flowRate < FLOW_RATE_MIN) flowRate = FLOW_RATE_MIN;
    }
    
    UA_Variant_setScalar(&value, &flowRate, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.WaterFlow"), value);

    /* PRESSURE: only when pump is ON */
    UA_Double pressure = 10.0;
    if(pumpEnable) {
        pressure = 10.0 + (flowRate / 10.0);
    }
    UA_Variant_setScalar(&value, &pressure, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.WaterPressure"), value);

    /* TEMPERATURE: only when pump is ON */
    UA_Double temperature = 40.0;
    if(pumpEnable) {
        temperature = 40.0 + (flowRate / 20.0);
    }
    UA_Variant_setScalar(&value, &temperature, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.SystemTemperature"), value);

    /* INJECTION RATE: 90% of flow rate, ZERO when pump is OFF */
    UA_Double injectionRate = 0.0;
    if(pumpEnable) {
        injectionRate = flowRate * 0.9;
    }
    UA_Variant_setScalar(&value, &injectionRate, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.InjectionRate"), value);

    /* PUMP RUNNING: TRUE only when pump is ON and flow > threshold */
    UA_Boolean pumpRunning = pumpEnable && (flowRate > 5.0);
    UA_Variant_setScalar(&value, &pumpRunning, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.PumpRunning"), value);

    /* RESERVOIR LEVEL: decreases faster when pump is ON 
     * Depletion rate: RESERVOIR_DEPLETION_RATE % per hour
     * Start at 100%, goes to 0% 
     */
    UA_Double reservoirLevel = 100.0;
    if(pumpEnable) {
        /* Calculate depletion: (elapsed_time_in_hours) * DEPLETION_RATE */
        double elapsedHours = (double)elapsed / 3600.0;
        reservoirLevel = 100.0 - (elapsedHours * RESERVOIR_DEPLETION_RATE);
        if(reservoirLevel < 0.0) reservoirLevel = 0.0;
    } else {
        /* When pump is OFF, reservoir stays at 100% */
        reservoirLevel = 100.0;
    }
    UA_Variant_setScalar(&value, &reservoirLevel, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.ReservoirLevel"), value);

    /* FILTER STATUS: depends on pressure (only changes when pump ON) */
    UA_Byte filterStatus = 1;  /* Normal */
    if(pumpEnable) {
        if(pressure > 30.0) {
            filterStatus = 2;  /* Clogged */
        } else if(pressure < 12.0) {
            filterStatus = 0;  /* Clean */
        }
    } else {
        filterStatus = 0;  /* Clean when pump is OFF */
    }
    UA_Variant_setScalar(&value, &filterStatus, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.FilterStatus"), value);

    /* OPERATING TIME: always counts */
    UA_UInt32 hours = (UA_UInt32)(elapsed / 3600);
    UA_Variant_setScalar(&value, &hours, &UA_TYPES[UA_TYPES_UINT32]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.OperatingTime.Hours"), value);

    UA_Byte minutes = (UA_Byte)((elapsed % 3600) / 60);
    UA_Variant_setScalar(&value, &minutes, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.OperatingTime.Minutes"), value);

    UA_Byte seconds = (UA_Byte)(elapsed % 60);
    UA_Variant_setScalar(&value, &seconds, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, UA_NODEID_STRING(1, (char*)"WaterInjectionSystem.1.OperatingTime.Seconds"), value);
}

/* Initialize OPC UA namespace and add all WIS variables */
static void
addWISVariables(UA_Server *server) {
    UA_StatusCode retval = UA_STATUSCODE_GOOD;

    /* Create root folder: WaterInjectionSystem */
    UA_ObjectAttributes objAttr = UA_ObjectAttributes_default;
    objAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Water Injection System");
    retval |= UA_Server_addObjectNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(1, "WaterInjectionSystem"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                     objAttr, NULL, NULL);

    /* Create instance: WIS Unit 1 */
    objAttr = UA_ObjectAttributes_default;
    objAttr.displayName = UA_LOCALIZEDTEXT("en-US", "WIS Unit 1");
    retval |= UA_Server_addObjectNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                     UA_NODEID_STRING(1, "WaterInjectionSystem"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(1, "WIS.1"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                     objAttr, NULL, NULL);

    /* ===== WRITABLE COMMAND ===== */
    
    /* PumpEnable - writable boolean command */
    UA_VariableAttributes varAttr = UA_VariableAttributes_default;
    UA_Boolean boolVal = true;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Pump Enable");
    varAttr.description = UA_LOCALIZEDTEXT("en-US", "Turn pump ON/OFF (writable)");
    varAttr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    UA_Variant_setScalar(&varAttr.value, &boolVal, &UA_TYPES[UA_TYPES_BOOLEAN]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.PumpEnable"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "PumpEnable"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);
    
    /* Attach write callback to PumpEnable */
    UA_ValueCallback valueCallback;
    valueCallback.onRead = NULL;
    valueCallback.onWrite = writePumpEnable;
    UA_Server_setVariableNode_valueCallback(server, 
                                           UA_NODEID_STRING(1, "WaterInjectionSystem.1.PumpEnable"),
                                           valueCallback);

    /* ===== READ-ONLY STATUS ===== */
    
    /* PumpRunning - read-only status indicator */
    varAttr = UA_VariableAttributes_default;
    boolVal = false;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Pump Running");
    varAttr.description = UA_LOCALIZEDTEXT("en-US", "Current pump status (read-only)");
    varAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    UA_Variant_setScalar(&varAttr.value, &boolVal, &UA_TYPES[UA_TYPES_BOOLEAN]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.PumpRunning"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "PumpRunning"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    /* ===== MEASURED VALUES (Double) ===== */
    
    UA_Double doubleValue = 0.0;
    
    struct {
        const char *nodeId;
        const char *displayName;
    } doubleVars[] = {
        {"WaterFlow", "Water Flow (m³/h)"},
        {"WaterPressure", "Water Pressure (bar)"},
        {"SystemTemperature", "System Temperature (°C)"},
        {"InjectionRate", "Injection Rate (m³/h)"},
        {"ReservoirLevel", "Reservoir Level (%)"}
    };
    
    for(int i = 0; i < 5; i++) {
        varAttr = UA_VariableAttributes_default;
        varAttr.displayName = UA_LOCALIZEDTEXT("en-US", (char*)doubleVars[i].displayName);
        varAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
        UA_Variant_setScalar(&varAttr.value, &doubleValue, &UA_TYPES[UA_TYPES_DOUBLE]);
        
        char nodeId[64];
        snprintf(nodeId, sizeof(nodeId), "WaterInjectionSystem.1.%s", doubleVars[i].nodeId);
        
        retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, nodeId),
                                           UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                           UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                           UA_QUALIFIEDNAME(1, (char*)doubleVars[i].nodeId),
                                           UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                           varAttr, NULL, NULL);
    }

    /* FilterStatus - byte enumeration */
    varAttr = UA_VariableAttributes_default;
    UA_Byte byteValue = 1;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Filter Status");
    varAttr.description = UA_LOCALIZEDTEXT("en-US", "0=Clean, 1=Normal, 2=Clogged");
    varAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    UA_Variant_setScalar(&varAttr.value, &byteValue, &UA_TYPES[UA_TYPES_BYTE]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.FilterStatus"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "FilterStatus"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    /* ===== OPERATING TIME ===== */
    
    objAttr = UA_ObjectAttributes_default;
    objAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Operating Time");
    retval |= UA_Server_addObjectNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime"),
                                     UA_NODEID_STRING(1, "WaterInjectionSystem.1"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_QUALIFIEDNAME(1, "OperatingTime"),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                     objAttr, NULL, NULL);

    /* Hours (UInt32) */
    varAttr = UA_VariableAttributes_default;
    UA_UInt32 uintValue = 0;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Hours");
    varAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    UA_Variant_setScalar(&varAttr.value, &uintValue, &UA_TYPES[UA_TYPES_UINT32]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Hours"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "Hours"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    /* Minutes (Byte) */
    byteValue = 0;
    varAttr = UA_VariableAttributes_default;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Minutes");
    varAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    UA_Variant_setScalar(&varAttr.value, &byteValue, &UA_TYPES[UA_TYPES_BYTE]);
    retval |= UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime.Minutes"),
                                       UA_NODEID_STRING(1, "WaterInjectionSystem.1.OperatingTime"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "Minutes"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                       varAttr, NULL, NULL);

    /* Seconds (Byte) */
    varAttr = UA_VariableAttributes_default;
    varAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Seconds");
    varAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    UA_Variant_setScalar(&varAttr.value, &byteValue, &UA_TYPES[UA_TYPES_BYTE]);
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

    /* Create and configure server */
    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    UA_StatusCode retval = UA_ServerConfig_setMinimal(UA_Server_getConfig(server), OPCUA_TCP_PORT, NULL);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to set server config: %s", UA_StatusCode_name(retval));
        UA_Server_delete(server);
        return EXIT_FAILURE;
    }

    /* Add WIS variables to namespace */
    addWISVariables(server);

    /* Start server */
    retval = UA_Server_run_startup(server);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to start server: %s", UA_StatusCode_name(retval));
        UA_Server_delete(server);
        return EXIT_FAILURE;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "================================================");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "FPSO Water Injection System OPC UA Server");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Endpoint: opc.tcp://localhost:%d", OPCUA_TCP_PORT);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Physical Model:");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "  Pressure = 10 + (Flow / 10) bar");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "  Temperature = 40 + (Flow / 20) °C");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "  InjectionRate = Flow * 0.9 m³/h");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "================================================");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Press Ctrl+C to shutdown");

    /* Main server loop */
    while(running) {
        updateWISData(server);
        UA_Server_run_iterate(server, true);
        usleep(UPDATE_INTERVAL_MS);
    }

    /* Shutdown */
    UA_Server_run_shutdown(server);
    UA_Server_delete(server);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Server stopped");
    return EXIT_SUCCESS;
}
