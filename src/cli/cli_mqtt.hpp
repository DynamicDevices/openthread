/*
 *  Copyright (c) 2018, Vit Holasek
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

#ifndef CLI_MQTT_HPP_
#define CLI_MQTT_HPP_

#if OPENTHREAD_CONFIG_MQTTSN_ENABLE

#include <openthread/error.h>
#include <openthread/mqttsn.h>
#include "cli/cli_output.hpp"

namespace ot {
namespace Cli {

/**
 * This class implements the CLI CoAP server and client.
 *
 */
class Mqtt : private Output
{
public:
    typedef Utils::CmdLineParser::Arg Arg;

    /**
     * Constructor
     *
     * @param[in]  aInstance            The OpenThread Instance.
     * @param[in]  aOutputImplementer   An `OutputImplementer`.
     *
     */
    Mqtt(otInstance *aInstance, OutputImplementer &aOutputImplementer);

    /**
     * This method interprets a list of CLI arguments.
     *
     * @param[in]  aArgs  An array of command line arguments.
     *
     */
    otError Process(Arg aArgs[]);

private:
    using Command = CommandEntry<Mqtt>;

    template <CommandId kCommandId> otError Process(Arg aArgs[]);

    otError ParseTopic(const Arg &aArg, otMqttsnTopic *aTopic);

    static void HandleConnected(otMqttsnReturnCode aCode, void *aContext);
    void        HandleConnected(otMqttsnReturnCode aCode);
    static void HandleSubscribed(otMqttsnReturnCode aCode, const otMqttsnTopic *aTopic, otMqttsnQos aQos, void* aContext);
    void        HandleSubscribed(otMqttsnReturnCode aCode, const otMqttsnTopic *aTopic, otMqttsnQos aQos);
    static void HandleRegistered(otMqttsnReturnCode aCode, const otMqttsnTopic *aTopic, void* aContext);
    void        HandleRegistered(otMqttsnReturnCode aCode, const otMqttsnTopic *aTopic);
    static void HandlePublished(otMqttsnReturnCode aCode, void* aContext);
    void        HandlePublished(otMqttsnReturnCode aCode);
    static void HandleUnsubscribed(otMqttsnReturnCode aCode, void* aContext);
    void        HandleUnsubscribed(otMqttsnReturnCode aCode);
    static otMqttsnReturnCode HandlePublishReceived(const uint8_t* aPayload, int32_t aPayloadLength, const otMqttsnTopic *aTopicId, void* aContext);
    otMqttsnReturnCode        HandlePublishReceived(const uint8_t* aPayload, int32_t aPayloadLength, const otMqttsnTopic *aTopicId);
    static void HandleDisconnected(otMqttsnDisconnectType aType, void* aContext);
    void        HandleDisconnected(otMqttsnDisconnectType aType);
    static void HandleSearchgwResponse(const otIp6Address* aAddress, uint8_t aGatewayId, void* aContext);
    void        HandleSearchgwResponse(const otIp6Address* aAddress, uint8_t aGatewayId);

    void PrintFailedWithCode(const char *aCommandName, otMqttsnReturnCode aCode);
};

} // namespace Cli
} // namespace ot

#endif

#endif /* CLI_MQTT_HPP_ */
