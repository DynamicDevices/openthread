/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <openthread-core-config.h>
#include <openthread/config.h>

#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/logging.h>
#include <openthread/platform/logging.h>

#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/tasklet.h"
#include "openthread/ip6.h"
#include "openthread/mqttsn.h"
#include "openthread/dataset.h"
#include "openthread/link.h"
#include "openthread-system.h"

#include "cli/cli_config.h"
#include "common/code_utils.hpp"

#include "lib/platform/reset_util.h"

#define NETWORK_NAME "OTBR4444"
#define PANID 0x4444
#define EXTPANID {0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44}
#define DEFAULT_CHANNEL 15
#define MASTER_KEY {0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44}

// TODO: Need to work out how to find this automatically ? Do we only support IPv6?
//
// Currently I have  MQTT-SMGateway build for udp6 running on a Raspberry Pi OTBR. This sets up wpan0 for the mesh and I have to set "MulticastIPv6If=wpan0" for this to work
// Then I have taken one of the wpan0 IPv6 addresses as the gateway address....
#define GATEWAY_PORT 10000
#define GATEWAY_ADDRESS "fe80::50a3:4a72:b06a:b3cd"

#define CLIENT_ID "THREAD"
#define CLIENT_PORT 10000

#define TOPIC_NAME "sensors"

static const uint8_t sExpanId[] = EXTPANID;
static const uint8_t sMasterKey[] = MASTER_KEY;

// Maximal awake time
static uint64_t sNextPublishAt = 0xffffffff;
#define PUBLISH_INTERVAL_MS 10000

/**
 * This function initializes the CLI app.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
extern void otAppCliInit(otInstance *aInstance);

#if OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
OT_TOOL_WEAK void *otPlatCAlloc(size_t aNum, size_t aSize) { return calloc(aNum, aSize); }

OT_TOOL_WEAK void otPlatFree(void *aPtr) { free(aPtr); }
#endif

void otTaskletsSignalPending(otInstance *aInstance) { OT_UNUSED_VARIABLE(aInstance); }

static void HandlePublished(otMqttsnReturnCode aCode, void* aContext)
{
    OT_UNUSED_VARIABLE(aCode);
    OT_UNUSED_VARIABLE(aContext);

    // Handle published
    otLogWarnPlat("Published");
}

const otMqttsnTopic _aTopic;

static void HandleRegistered(otMqttsnReturnCode aCode, const otMqttsnTopic* aTopic, void* aContext)
{
    // Handle registered
    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        otLogWarnPlat("HandleRegistered - OK");
        memcpy(&_aTopic, aTopic, sizeof(otMqttsnTopic));
    }
    else
    {
        otLogWarnPlat("HandleRegistered - Error");
    }
}

static void MqttsnConnect(otInstance *instance);

static void HandleConnected(otMqttsnReturnCode aCode, void* aContext)
{
    // Handle connected
    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        otLogWarnPlat("HandleConnected -Accepted");

        otLogWarnPlat("Registering Topic");

        // Obtain target topic ID
        otMqttsnRegister(instance, TOPIC_NAME, HandleRegistered, (void *)instance);
    }
    else
    {
        otLogWarnPlat("HandleConnected - Error");
    }
}

static void MqttsnConnect(otInstance *instance)
{
    otIp6Address address;
    otIp6AddressFromString(GATEWAY_ADDRESS, &address);

    otLogWarnPlat("Connect to %s:%d", GATEWAY_ADDRESS, GATEWAY_PORT);

    // Set MQTT-SN client configuration settings
    otMqttsnConfig config;
    config.mClientId = CLIENT_ID;
    config.mKeepAlive = 30;
    config.mCleanSession = true;
    config.mPort = GATEWAY_PORT;
    config.mAddress = &address;
    config.mRetransmissionCount = 3;
    config.mRetransmissionTimeout = 10;

    // Register connected callback
    otMqttsnSetConnectedHandler(instance, HandleConnected, (void *)instance);
    // Connect to the MQTT broker (gateway)
    otMqttsnConnect(instance, &config);
}

static void StateChanged(otChangedFlags aFlags, void *aContext)
{
    otLogWarnPlat("State Changed");

    otInstance *instance = (otInstance *)aContext;
    // when thread role changed
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otLogWarnPlat("*** State Changed - Role");

        otDeviceRole role = otThreadGetDeviceRole(instance);

        // If role changed to any of active roles and MQTT-SN client is not connected then connect
//        if ((role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER)
//            && otMqttsnGetState(instance) == kStateDisconnected)
//        {
//           sNextAwakeAt = ot::TimerMilli::GetNow().GetValue() + PUBLISH_INTERVAL_MS;
//        }
    }
}

#if OPENTHREAD_POSIX && !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
static otError ProcessExit(void *aContext, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aContext);
    OT_UNUSED_VARIABLE(aArgsLength);
    OT_UNUSED_VARIABLE(aArgs);

    exit(EXIT_SUCCESS);
}

#if OPENTHREAD_EXAMPLES_SIMULATION
extern otError ProcessNodeIdFilter(void *aContext, uint8_t aArgsLength, char *aArgs[]);
#endif

static const otCliCommand kCommands[] = {
    {"exit", ProcessExit},
#if OPENTHREAD_EXAMPLES_SIMULATION
    /*
     * The CLI command `nodeidfilter` only works for simulation in real time.
     * The usage of the command `nodeidfilter`:
     *     - `nodeidfilter deny <nodeid>`:  It denies the connection to a specified node.
     *     - `nodeidfilter clear`:          It restores the filter state to default.
     */
    {"nodeidfilter", ProcessNodeIdFilter},
#endif
};
#endif // OPENTHREAD_POSIX && !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)

int main(int argc, char *argv[])
{
    otInstance *instance;

    OT_SETUP_RESET_JUMP(argv);

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    size_t   otInstanceBufferLength = 0;
    uint8_t *otInstanceBuffer       = NULL;
#endif

pseudo_reset:

    otSysInit(argc, argv);

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    // Call to query the buffer size
    (void)otInstanceInit(NULL, &otInstanceBufferLength);

    // Call to allocate the buffer
    otInstanceBuffer = (uint8_t *)malloc(otInstanceBufferLength);
    assert(otInstanceBuffer);

    // Initialize OpenThread with the buffer
    instance = otInstanceInit(otInstanceBuffer, &otInstanceBufferLength);
#else
    instance = otInstanceInitSingle();
#endif
    assert(instance);

    otAppCliInit(instance);

#if OPENTHREAD_POSIX && !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
    otCliSetUserCommands(kCommands, OT_ARRAY_LENGTH(kCommands), instance);
#endif

    //
    // MQTT-SN
    //
    otError error = OT_ERROR_NONE;
    otExtendedPanId extendedPanid;
    otNetworkKey masterKey;

    // Set default network settings
    // Set network name
    otLogWarnPlat("Setting Network Name to %s", NETWORK_NAME);
    error = otThreadSetNetworkName(instance, NETWORK_NAME);
    // Set extended PANID
    memcpy(extendedPanid.m8, sExpanId, sizeof(sExpanId));
    otLogWarnPlat("Setting Extended PANID");
    error = otThreadSetExtendedPanId(instance, &extendedPanid);
    // Set PANID
    otLogWarnPlat("Setting PANID to 0x%04X", PANID);
    error = otLinkSetPanId(instance, PANID);
    // Set channel
    otLogWarnPlat("Setting Channel to %d", DEFAULT_CHANNEL);
    error = otLinkSetChannel(instance, DEFAULT_CHANNEL);
    // Set masterkey
    otLogWarnPlat("Setting Network Key");
    memcpy(masterKey.m8, sMasterKey, sizeof(sMasterKey));
    error = otThreadSetNetworkKey(instance, &masterKey);

    // Register notifier callback to receive thread role changed events
    error = otSetStateChangedCallback(instance, StateChanged, instance);

    // Start thread network
    otIp6SetSlaacEnabled(instance, true);
    error = otIp6SetEnabled(instance, true);
    error = otThreadSetEnabled(instance, true);

    // Start MQTT-SN client
    otLogWarnPlat("Starting MQTT-SN on port %d", CLIENT_PORT);
    error = otMqttsnStart(instance, CLIENT_PORT);

    // Delay until we start connecting / publishing with MQTT-SN
    sNextPublishAt = otInstanceGetUptime(instance) + PUBLISH_INTERVAL_MS;

    while (!otSysPseudoResetWasRequested())
    {
        otTaskletsProcess(instance);
        otSysProcessDrivers(instance);

        // Publish when scheduled time passed
        if (otInstanceGetUptime(instance) > sNextPublishAt)
        {
	   if(otMqttsnGetState(instance) == kStateDisconnected || otMqttsnGetState(instance)  == kStateLost)
           {
              MqttsnConnect(instance);
           }
           else
           {
             static int count = 0;

             otLogWarnPlat("Client state %d", otMqttsnGetState(instance));

             // Publish message to the registered topic
             otLogWarnPlat("Publishing...");
             const char* strdata = "{\"id\":%s, \"count\":%d, \"status\":%s, \"batt\":%d, \"lat\":1.234, \"lon\",5.678, \"height\":1.23, \"temp\":24.0}";
             const char data[128];
             sprintf(data, strdata, "1234", count++, "P1", 100);
             int32_t length = strlen(data);

             otError err = otMqttsnPublish(instance, (const uint8_t*)data, length, kQos1, false, &_aTopic,
               HandlePublished, NULL);
             otLogWarnPlat("Publishing rsp %d", err);
           }

           sNextPublishAt = otInstanceGetUptime(instance) + PUBLISH_INTERVAL_MS;
        }
    }

    otInstanceFinalize(instance);
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    free(otInstanceBuffer);
#endif

    goto pseudo_reset;

    return 0;
}

// TODO: Need to work out how to define APP output on the command line

//#if OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_APP
void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    va_list ap;

    va_start(ap, aFormat);
    otCliPlatLogv(aLogLevel, aLogRegion, aFormat, ap);
    va_end(ap);
}
//#endif
