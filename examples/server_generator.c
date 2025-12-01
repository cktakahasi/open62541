#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdlib.h>
#include <time.h>

static volatile UA_Boolean running = true;
static time_t startTime;
static UA_Double currentRPM = 1800.0;

static void
stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received ctrl-c");
    running = false;
}

static void
updateOperatingTime(UA_Server *server) {
    time_t currentTime = time(NULL);
    time_t elapsed = currentTime - startTime;

    UA_UInt32 hours = (UA_UInt32)(elapsed / 3600);
    UA_Byte minutes = (UA_Byte)((elapsed % 3600) / 60);
    UA_Byte seconds = (UA_Byte)(elapsed % 60);

    UA_Variant value;
    
    /* Update Hours */
    UA_Variant_setScalar(&value, &hours, &UA_TYPES[UA_TYPES_UINT32]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Hours"),
                        value);

    /* Update Minutes */
    UA_Variant_setScalar(&value, &minutes, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Minutes"),
                        value);

    /* Update Seconds */
    UA_Variant_setScalar(&value, &seconds, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Seconds"),
                        value);
}

static void
updateRPM(UA_Server *server) {
    /* Increment RPM by 1 each second */
    currentRPM += 1.0;
    
    /* Reset to 1800 when reaching 2000 */
    if (currentRPM > 2000.0) {
        currentRPM = 1800.0;
    }

    UA_Variant value;
    UA_Variant_setScalar(&value, &currentRPM, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.EngineSpeed"),
                        value);
}

static void
updateOperatingTimeCallback(UA_Server *server, void *data) {
    updateOperatingTime(server);
    updateRPM(server);
}

int
main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    startTime = time(NULL);
    currentRPM = 1800.0;

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
    UA_Variant_setScalar(&attr.value, &currentRPM, &UA_TYPES[UA_TYPES_DOUBLE]);
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

    /* Coolant Temperature (C) */
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

    /* Create Operating Time Object */
    UA_ObjectAttributes timeOAttr = UA_ObjectAttributes_default;
    timeOAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Operating Time");
    timeOAttr.description = UA_LOCALIZEDTEXT("en-US", "Server uptime since start");

    UA_NodeId operatingTimeId = UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime");
    UA_QualifiedName operatingTimeName = UA_QUALIFIEDNAME(1, "OperatingTime");

    UA_Server_addObjectNode(server, operatingTimeId,
                            generatorId, parentRef,
                            operatingTimeName,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            timeOAttr, NULL, NULL);

    /* Operating Time - Hours */
    UA_UInt32 hours = 0;
    UA_Variant_setScalar(&attr.value, &hours, &UA_TYPES[UA_TYPES_UINT32]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Uptime hours");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Hours");
    attr.dataType = UA_TYPES[UA_TYPES_UINT32].typeId;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Hours"),
                              operatingTimeId, parentRef,
                              UA_QUALIFIEDNAME(1, "Hours"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Operating Time - Minutes */
    UA_Byte minutes = 0;
    UA_Variant_setScalar(&attr.value, &minutes, &UA_TYPES[UA_TYPES_BYTE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Uptime minutes");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Minutes");
    attr.dataType = UA_TYPES[UA_TYPES_BYTE].typeId;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Minutes"),
                              operatingTimeId, parentRef,
                              UA_QUALIFIEDNAME(1, "Minutes"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Operating Time - Seconds */
    UA_Byte seconds = 0;
    UA_Variant_setScalar(&attr.value, &seconds, &UA_TYPES[UA_TYPES_BYTE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Uptime seconds");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Seconds");

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Seconds"),
                              operatingTimeId, parentRef,
                              UA_QUALIFIEDNAME(1, "Seconds"),
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

    /* Add repeated callback to update operating time and RPM every second */
    UA_Server_addRepeatedCallback(server, updateOperatingTimeCallback, NULL, 1000, NULL);

    UA_StatusCode retval = UA_Server_run(server, &running);

    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
