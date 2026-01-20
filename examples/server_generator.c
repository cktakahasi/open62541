#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define OPCUA_TCP_PORT 4840
#define LOAD_MIN 0.0
#define LOAD_MAX 1500.0      /* Max 1500 kW */
#define RPM_IDLE 1200.0
#define RPM_FULL 1800.0
#define VOLTAGE_NOMINAL 480.0
#define VOLTAGE_MIN 456.0    /* -5% voltage drop at full load */
#define FREQUENCY_NOMINAL 60.0
#define OIL_PRESSURE_MIN 2.0  /* bar at idle */
#define OIL_PRESSURE_MAX 5.5  /* bar at full load */
#define COOLANT_TEMP_IDLE 50.0
#define COOLANT_TEMP_MAX 90.0  /* Safety limit increased to 90°C */
#define FUEL_CONSUMPTION_RATE 0.22  /* L/kWh - more realistic */
#define BATTERY_VOLTAGE_NOMINAL 24.5
#define BATTERY_VOLTAGE_MIN 22.0    /* Under heavy load */

static volatile UA_Boolean running = true;
static time_t startTime;
static UA_Double currentRPM = 1800.0;
static UA_Double activePower = 500.0;
static UA_Double lastActivePower = 500.0;

static void
stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received ctrl-c");
    running = false;
}

/* Callback invoked when ActivePower is written by client */
static void
writeActivePower(UA_Server *server,
                 const UA_NodeId *sessionId, void *sessionContext,
                 const UA_NodeId *nodeId, void *nodeContext,
                 const UA_NumericRange *range,
                 const UA_DataValue *value) {
    if(value == NULL || value->value.type == NULL) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "writeActivePower: Invalid value");
        return;
    }

    if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        UA_Double newPower = *(UA_Double*)value->value.data;
        UA_Double oldPower = activePower;

        /* Clamp to valid range */
        if(newPower < LOAD_MIN) newPower = LOAD_MIN;
        if(newPower > LOAD_MAX) newPower = LOAD_MAX;

        activePower = newPower;
        lastActivePower = oldPower;

        time_t currentTime = time(NULL);
        time_t elapsed = currentTime - startTime;
        UA_UInt32 h = elapsed / 3600, m = (elapsed % 3600) / 60, s = elapsed % 60;

        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "================================================");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "*** ActivePower WRITE CALLBACK ***");
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Uptime: %02u:%02u:%02u", h, m, s);
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Power: %.1f -> %.1f kW", oldPower, newPower);
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "================================================");
    }
}

static void
updateOperatingTime(UA_Server *server) {
    time_t currentTime = time(NULL);
    time_t elapsed = currentTime - startTime;

    UA_UInt32 hours = (UA_UInt32)(elapsed / 3600);
    UA_Byte minutes = (UA_Byte)((elapsed % 3600) / 60);
    UA_Byte seconds = (UA_Byte)(elapsed % 60);

    UA_Variant value;
    
    UA_Variant_setScalar(&value, &hours, &UA_TYPES[UA_TYPES_UINT32]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Hours"),
                        value);

    UA_Variant_setScalar(&value, &minutes, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Minutes"),
                        value);

    UA_Variant_setScalar(&value, &seconds, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Seconds"),
                        value);

    /* Update Running status: true if power > 0 */
    UA_Boolean isRunning = (activePower > 0.0) ? true : false;
    UA_Variant_setScalar(&value, &isRunning, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.Running"),
                        value);
}

static void
updateGeneratorParameters(UA_Server *server) {
    /* Only calculate if power > 0 */
    if(activePower <= 0.0) {
        currentRPM = RPM_IDLE;
    } else {
        /* RPM scales linearly with active power */
        double powerPercent = activePower / LOAD_MAX;
        if(powerPercent > 1.0) powerPercent = 1.0;
        currentRPM = RPM_IDLE + (RPM_FULL - RPM_IDLE) * powerPercent;
    }

    UA_Variant value;

    /* Update Engine Speed */
    UA_Variant_setScalar(&value, &currentRPM, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.EngineSpeed"),
                        value);

    /* Voltage: IEC 60038 allows ±10% tolerance */
    double powerPercent = (activePower > 0.0) ? (activePower / LOAD_MAX) : 0.0;
    if(powerPercent > 1.0) powerPercent = 1.0;
    UA_Double voltage = VOLTAGE_NOMINAL - (powerPercent * (VOLTAGE_NOMINAL - VOLTAGE_MIN));
    
    UA_Variant_setScalar(&value, &voltage, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.Voltage"),
                        value);

    /* Frequency: ±0.5 Hz variation under load (simplified) */
    UA_Double frequency = FREQUENCY_NOMINAL - (powerPercent * 0.5);
    UA_Variant_setScalar(&value, &frequency, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.Frequency"),
                        value);

    /* Oil pressure increases non-linearly with RPM */
    UA_Double rpmPercent = (currentRPM - RPM_IDLE) / (RPM_FULL - RPM_IDLE);
    UA_Double oilPressure = OIL_PRESSURE_MIN + (rpmPercent * (OIL_PRESSURE_MAX - OIL_PRESSURE_MIN));
    
    UA_Variant_setScalar(&value, &oilPressure, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.OilPressure"),
                        value);

    /* Coolant temperature: thermal inertia (slow response) */
    static UA_Double coolantTemp = COOLANT_TEMP_IDLE;
    UA_Double targetTemp = COOLANT_TEMP_IDLE + (powerPercent * (COOLANT_TEMP_MAX - COOLANT_TEMP_IDLE));
    /* First-order response: ~30 second time constant */
    coolantTemp += (targetTemp - coolantTemp) * 0.033;  /* 1/30 per second */
    
    UA_Variant_setScalar(&value, &coolantTemp, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.CoolantTemperature"),
                        value);

    /* Fuel consumption with thermal efficiency curve */
    static UA_Double totalFuelConsumed = 0.0;
    if(activePower > 0.0) {
        /* Consumption increases at part load (worse efficiency) */
        UA_Double efficiencyFactor = 1.0 + (0.5 * (1.0 - powerPercent));
        totalFuelConsumed += (activePower / 3600.0) * FUEL_CONSUMPTION_RATE * efficiencyFactor;
    }
    UA_Double fuelLevel = 100.0 - (totalFuelConsumed / 1000.0) * 100.0;
    if(fuelLevel < 0.0) fuelLevel = 0.0;
    
    UA_Variant_setScalar(&value, &fuelLevel, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.FuelLevel"),
                        value);

    /* Battery voltage: drops with load, recovers at idle */
    static UA_Double batteryVoltage = BATTERY_VOLTAGE_NOMINAL;
    UA_Double targetBatteryVoltage = BATTERY_VOLTAGE_NOMINAL - (powerPercent * (BATTERY_VOLTAGE_NOMINAL - BATTERY_VOLTAGE_MIN));
    batteryVoltage += (targetBatteryVoltage - batteryVoltage) * 0.1;  /* Slower response */
    
    UA_Variant_setScalar(&value, &batteryVoltage, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
                        UA_NODEID_STRING(1, "DieselGenerator.1.BatteryVoltage"),
                        value);
}

static void
updateOperatingTimeCallback(UA_Server *server, void *data) {
    updateOperatingTime(server);
    updateGeneratorParameters(server);
}

int
main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    startTime = time(NULL);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);

    UA_ServerConfig_setMinimal(config, OPCUA_TCP_PORT, NULL);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
            "================================================");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
            "OPC UA Diesel Generator Server");
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
            "Port: %u", OPCUA_TCP_PORT);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
            "================================================\n");

    config->maxSubscriptions = 100;
    config->maxSubscriptionsPerSession = 10;
    config->publishingIntervalLimits.min = 100.0;
    config->publishingIntervalLimits.max = 3600000.0;

    /* Generator object */
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

    UA_VariableAttributes attr = UA_VariableAttributes_default;

    /* Active Power (kW) - WRITABLE */
    activePower = 500.0;
    attr.description = UA_LOCALIZEDTEXT("en-US", "Active power output (writable)");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Active Power");
    attr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    attr.writeMask = UA_ATTRIBUTEID_VALUE;
    attr.userWriteMask = UA_ATTRIBUTEID_VALUE;
    UA_Variant_setScalar(&attr.value, &activePower, &UA_TYPES[UA_TYPES_DOUBLE]);

    UA_NodeId activePowerNodeId = UA_NODEID_STRING(1, "DieselGenerator.1.ActivePower");
    UA_Server_addVariableNode(server, activePowerNodeId,
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "ActivePower"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);
    
    /* Set write callback for ActivePower */
    UA_ValueCallback valueCallback;
    valueCallback.onRead = NULL;
    valueCallback.onWrite = writeActivePower;
    UA_Server_setVariableNode_valueCallback(server, activePowerNodeId, valueCallback);

    /* Voltage (V) - READ ONLY */
    attr = UA_VariableAttributes_default;
    UA_Double voltage = 480.0;
    UA_Variant_setScalar(&attr.value, &voltage, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Output voltage");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Voltage");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.Voltage"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "Voltage"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Frequency (Hz) - READ ONLY */
    attr = UA_VariableAttributes_default;
    UA_Double frequency = 60.0;
    UA_Variant_setScalar(&attr.value, &frequency, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Output frequency");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Frequency");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.Frequency"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "Frequency"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Engine Speed (RPM) - READ ONLY */
    attr = UA_VariableAttributes_default;
    currentRPM = 1200.0;
    UA_Variant_setScalar(&attr.value, &currentRPM, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Engine rotational speed");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Engine Speed");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.EngineSpeed"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "EngineSpeed"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Oil Pressure (bar) - READ ONLY */
    attr = UA_VariableAttributes_default;
    UA_Double oilPressure = 2.0;
    UA_Variant_setScalar(&attr.value, &oilPressure, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Engine oil pressure");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Oil Pressure");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OilPressure"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "OilPressure"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Coolant Temperature (C) - READ ONLY */
    attr = UA_VariableAttributes_default;
    UA_Double coolantTemp = 50.0;
    UA_Variant_setScalar(&attr.value, &coolantTemp, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Engine coolant temperature");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Coolant Temperature");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.CoolantTemperature"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "CoolantTemperature"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Fuel Level (%) - READ ONLY */
    attr = UA_VariableAttributes_default;
    UA_Double fuelLevel = 100.0;
    UA_Variant_setScalar(&attr.value, &fuelLevel, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Fuel tank level percentage");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Fuel Level");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.FuelLevel"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "FuelLevel"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Battery Voltage (V) - READ ONLY */
    attr = UA_VariableAttributes_default;
    UA_Double batteryVoltage = 24.5;
    UA_Variant_setScalar(&attr.value, &batteryVoltage, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Starting battery voltage");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Battery Voltage");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.BatteryVoltage"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "BatteryVoltage"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Operating Time Object */
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
    attr = UA_VariableAttributes_default;
    UA_UInt32 hours = 0;
    UA_Variant_setScalar(&attr.value, &hours, &UA_TYPES[UA_TYPES_UINT32]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Uptime hours");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Hours");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Hours"),
                              operatingTimeId, parentRef,
                              UA_QUALIFIEDNAME(1, "Hours"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Operating Time - Minutes */
    attr = UA_VariableAttributes_default;
    UA_Byte minutes = 0;
    UA_Variant_setScalar(&attr.value, &minutes, &UA_TYPES[UA_TYPES_BYTE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Uptime minutes");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Minutes");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Minutes"),
                              operatingTimeId, parentRef,
                              UA_QUALIFIEDNAME(1, "Minutes"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Operating Time - Seconds */
    attr = UA_VariableAttributes_default;
    UA_Byte seconds = 0;
    UA_Variant_setScalar(&attr.value, &seconds, &UA_TYPES[UA_TYPES_BYTE]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Uptime seconds");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Seconds");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.OperatingTime.Seconds"),
                              operatingTimeId, parentRef,
                              UA_QUALIFIEDNAME(1, "Seconds"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Generator Running Status - READ ONLY */
    attr = UA_VariableAttributes_default;
    UA_Boolean isRunning = false;
    UA_Variant_setScalar(&attr.value, &isRunning, &UA_TYPES[UA_TYPES_BOOLEAN]);
    attr.description = UA_LOCALIZEDTEXT("en-US", "Generator operational status (true if power > 0)");
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Running");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_Server_addVariableNode(server,
                              UA_NODEID_STRING(1, "DieselGenerator.1.Running"),
                              generatorId, parentRef,
                              UA_QUALIFIEDNAME(1, "Running"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    /* Add repeated callback to update parameters every 1 second */
    UA_Server_addRepeatedCallback(server, updateOperatingTimeCallback, NULL, 1000, NULL);

    UA_StatusCode retval = UA_Server_run(server, &running);

    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}