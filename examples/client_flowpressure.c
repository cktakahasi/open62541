/*
 * FPSO Water Injection OPC UA Client
 *
 * Simple OPC UA client that connects to an OPC UA server and
 * periodically reads flow rate and pressure values.
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

static volatile UA_Boolean running = true;

static void stopHandler(int sig)
{
    (void)sig;
    running = false;
}

static void readDouble(UA_Client *client, const char *nodeIdStr,
                                                const char *label)
{

    UA_Variant value;
    UA_Variant_init(&value);

    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(1, nodeIdStr);

    UA_StatusCode retval =
        UA_Client_readValueAttribute(client, nodeId, &value);

    if(retval == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&value,
           &UA_TYPES[UA_TYPES_DOUBLE])) {

        UA_Double v = *(UA_Double *)value.data;
        printf("%-10s : %.2f\n", label, v);
    } else {
        printf("%-10s : read error (0x%08x)\n",
               label, retval);
    }

    UA_NodeId_clear(&nodeId);
    UA_Variant_clear(&value);
}

int main(void)
{
    signal(SIGINT,  stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Client *client = UA_Client_new();
    UA_ClientConfig *config = UA_Client_getConfig(client);
    UA_ClientConfig_setDefault(config);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
        "Connecting to OPC UA server at %s", OPCUA_ENDPOINT);

    UA_StatusCode retval =
        UA_Client_connect(client, OPCUA_ENDPOINT);

    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
            "Failed to connect (0x%08x)", retval);
        UA_Client_delete(client);
        return EXIT_FAILURE;
    }

    printf("Connected to OPC UA server\n\n");

    while(running) {
        readDouble(client, "FlowRate", "FlowRate");
        readDouble(client, "Pressure", "Pressure");
        printf("--------------\n");
        sleep(1);
    }

    UA_Client_disconnect(client);
    UA_Client_delete(client);
    return EXIT_SUCCESS;
}
