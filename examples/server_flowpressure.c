/*
 * FPSO Water Injection OPC UA Simulator
 *
 * Simulates a simple but physically consistent relationship
 * between flow rate and pressure for an FPSO water injection system.
 *
 * Model:
 *   P = P_static + K * Q^2 + noise
 *
 * Author: example
 * License: MIT
 */

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define OPCUA_TCP_PORT 4841

/* ------------------------------------------------------------------------- */
/* Global control                                                            */
/* ------------------------------------------------------------------------- */

static volatile UA_Boolean running = true;

static void
stopHandler(int sig) {
    (void)sig;
    running = false;
}

/* ------------------------------------------------------------------------- */
/* FPSO process model                                                        */
/* ------------------------------------------------------------------------- */

/* Operating ranges (typical offshore values) */
static double flowRate  = 850.0;  /* m3/h */
static double pressure  = 220.0;  /* bar */

static const double FLOW_MIN  = 700.0;
static const double FLOW_MAX  = 1000.0;
static const double FLOW_STEP = 2.0;     /* m3/h per second */

static const double P_STATIC = 150.0;    /* bar */
static const double K_LOSS   = 0.00009;  /* hydraulic loss coefficient */

static int flowDirection = 1;

/* ------------------------------------------------------------------------- */
/* Utility                                                                   */
/* ------------------------------------------------------------------------- */

static double
noise(double amplitude) {
    return ((double)rand() / RAND_MAX - 0.5) * amplitude;
}

/* ------------------------------------------------------------------------- */
/* Process update                                                            */
/* ------------------------------------------------------------------------- */

static void
updateProcess(UA_Server *server) {
    UA_Variant value;
    UA_Variant_init(&value);

    /* Smooth flow variation */
    flowRate += flowDirection * FLOW_STEP;

    if(flowRate >= FLOW_MAX || flowRate <= FLOW_MIN)
        flowDirection *= -1;

    /* Hydraulic model */
    pressure = P_STATIC + K_LOSS * flowRate * flowRate;

    /* Operational noise */
    flowRate += noise(1.0);   /* ±0.5 m3/h */
    pressure += noise(0.8);   /* ±0.4 bar */

    /* Write FlowRate */
    UA_Variant_setScalar(&value, &flowRate,
                         &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
        UA_NODEID_STRING(1, "FlowRate"),
        value);

    /* Write Pressure */
    UA_Variant_setScalar(&value, &pressure,
                         &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server,
        UA_NODEID_STRING(1, "Pressure"),
        value);
}

/* Periodic callback wrapper */
static void
processCallback(UA_Server *server, void *data) {
    (void)data;
    updateProcess(server);
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int
main(void) {
    UA_StatusCode retval;

    signal(SIGINT,  stopHandler);
    signal(SIGTERM, stopHandler);

    srand((unsigned int)time(NULL));

    /* Create server */
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);

    /* Fixed TCP port */
    retval = UA_ServerConfig_setMinimal(config, OPCUA_TCP_PORT, NULL);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
            "Failed to bind OPC UA server to TCP port %u (0x%08x)",
            OPCUA_TCP_PORT, retval);
        UA_Server_delete(server);
        return EXIT_FAILURE;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
        "OPC UA server listening on tcp://0.0.0.0:%u",
        OPCUA_TCP_PORT);

    /* --------------------------------------------------------------------- */
    /* FlowRate variable                                                      */
    /* --------------------------------------------------------------------- */

    UA_VariableAttributes flowAttr = UA_VariableAttributes_default;
    UA_Variant_setScalar(&flowAttr.value,
                         &flowRate,
                         &UA_TYPES[UA_TYPES_DOUBLE]);

    flowAttr.displayName =
        UA_LOCALIZEDTEXT("en-US", "FlowRate (m3/h)");
    flowAttr.description =
        UA_LOCALIZEDTEXT("en-US", "Injected water flow rate");

    UA_Server_addVariableNode(
        server,
        UA_NODEID_STRING(1, "FlowRate"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, "FlowRate"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        flowAttr,
        NULL,
        NULL
    );

    /* --------------------------------------------------------------------- */
    /* Pressure variable                                                      */
    /* --------------------------------------------------------------------- */

    UA_VariableAttributes pressureAttr = UA_VariableAttributes_default;
    UA_Variant_setScalar(&pressureAttr.value,
                         &pressure,
                         &UA_TYPES[UA_TYPES_DOUBLE]);

    pressureAttr.displayName =
        UA_LOCALIZEDTEXT("en-US", "Pressure (bar)");
    pressureAttr.description =
        UA_LOCALIZEDTEXT("en-US", "Injection header pressure");

    UA_Server_addVariableNode(
        server,
        UA_NODEID_STRING(1, "Pressure"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, "Pressure"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        pressureAttr,
        NULL,
        NULL
    );

    /* --------------------------------------------------------------------- */
    /* Periodic process update                                                */
    /* --------------------------------------------------------------------- */

    UA_Server_addRepeatedCallback(
        server,
        processCallback,
        NULL,
        1000,   /* 1 second */
        NULL
    );

    UA_LOG_INFO(UA_Log_Stdout,
        UA_LOGCATEGORY_SERVER,
        "FPSO OPC UA Injection Simulator running");

    /* THIS call actually opens the TCP socket */
    retval = UA_Server_run(server, &running);

    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
