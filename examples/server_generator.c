#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdlib.h>

static volatile UA_Boolean running = true;

static void
stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received ctrl-c");
    running = false;
}

int
main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    /* Create Diesel Generator Object */
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Diesel Generator 1");
    oAttr.description = UA_LOCALIZEDTEXT("en-US", "Diesel engine generator unit");

    UA_NodeId generatorId = UA_NODEID_STRING(1, "DieselGenerator.1");
    UA_QualifiedName generatorName = UA_QUALIFIEDNAME(1, "DieselGenerator1");
    UA_NodeId parentRef = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);

    UA_Server_addObjectNode(server, generatorId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            parentRef, generatorName,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            oAttr, NULL, NULL);

    /* Active Power (kW) */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Double activePower = 1250.0;
    UA_Variant_setScalar(&attr.value, &activePower, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Active power output");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Active Power");
    attr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.ActivePower"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "ActivePower"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Voltage (V) */
    UA_Double voltage = 480.0;
    UA_Variant_setScalar(&attr.value, &voltage, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Output voltage");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Voltage");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.Voltage"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "Voltage"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Frequency (Hz) */
    UA_Double frequency = 60.0;
    UA_Variant_setScalar(&attr.value, &frequency, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Output frequency");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Frequency");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.Frequency"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "Frequency"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Engine Speed (RPM) */
    UA_Double speed = 1800.0;
    UA_Variant_setScalar(&attr.value, &speed, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Engine rotational speed");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Engine Speed");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.EngineSpeed"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "EngineSpeed"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Oil Pressure (bar) */
    UA_Double oilPressure = 4.5;
    UA_Variant_setScalar(&attr.value, &oilPressure, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Engine oil pressure");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Oil Pressure");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OilPressure"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "OilPressure"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Coolant Temperature (°C) */
    UA_Double coolantTemp = 85.0;
    UA_Variant_setScalar(&attr.value, &coolantTemp, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Engine coolant temperature");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Coolant Temperature");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.CoolantTemperature"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "CoolantTemperature"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Fuel Level (%) */
    UA_Double fuelLevel = 75.0;
    UA_Variant_setScalar(&attr.value, &fuelLevel, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Fuel tank level percentage");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Fuel Level");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.FuelLevel"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "FuelLevel"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Battery Voltage (V) */
    UA_Double batteryVoltage = 24.5;
    UA_Variant_setScalar(&attr.value, &batteryVoltage, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Starting battery voltage");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Battery Voltage");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.BatteryVoltage"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "BatteryVoltage"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Running Hours */
    UA_UInt32 runningHours = 12450;
    UA_Variant_setScalar(&attr.value, &runningHours, &UA_TYPES[UA_TYPES_UINT32]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Total running hours");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Running Hours");
    attr.dataType = UA_TYPES[UA_TYPES_UINT32].typeId;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.RunningHours"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "RunningHours"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Generator Status */
    UA_Boolean isRunning = true;
    UA_Variant_setScalar(&attr.value, &isRunning, &UA_TYPES[UA_TYPES_BOOLEAN]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Generator operational status");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Running");
    attr.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.Running"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "Running"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    UA_StatusCode retval = UA_Server_run(server, &running);

    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
