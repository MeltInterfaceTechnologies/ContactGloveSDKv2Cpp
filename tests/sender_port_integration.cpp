#include "contactglove/ContactGloveSDKv2.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    using namespace ContactGloveSDKv2;

    SDK sdk;
    Host host;
    host.hostName = "sender-port-test";
    host.ipAddress = "127.0.0.1";
    host.port = 25811;
    host.receivePort = 25812;
    host.sendPort = argc > 1 ? static_cast<uint32_t>(std::stoul(argv[1])) : 25813;

    if (sdk.ConnectToHost(host) != SDKReturnCode::Success) {
        std::cerr << "ConnectToHost failed\n";
        return 1;
    }

    // Emulate delayed LAN approval across several data timeouts. The server
    // withholds data until the sixth registration request from this endpoint.
    for (int retry = 0; retry < 5; ++retry) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (sdk.ConnectToHost(host) != SDKReturnCode::Success) return 5;
    }
    Host different = host;
    different.port += 1;
    if (sdk.ConnectToHost(different) != SDKReturnCode::InvalidArgument) return 6;
    different = host;
    different.sendPort = host.sendPort == 0 ? 25813 : 0;
    if (sdk.ConnectToHost(different) != SDKReturnCode::InvalidArgument) return 7;
    different = host;
    different.ipAddress = "127.0.0.2";
    if (sdk.ConnectToHost(different) != SDKReturnCode::InvalidArgument) return 8;

    // The test server sends a Device message after it sees Connect.
    GloveData glove;
    bool received_device = false;
    for (int i = 0; i < 40; ++i) {
        if (sdk.GetDataForGlove("sender-port-test", &glove) == SDKReturnCode::Success) {
            received_device = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!received_device) {
        std::cerr << "Device message was not received\n";
        sdk.Disconnect(host);
        return 2;
    }
    if (sdk.VibrateGlove("sender-port-test", 160.0f, 0.5f, 0.1f) != SDKReturnCode::Success) {
        std::cerr << "VibrateGlove failed\n";
        sdk.Disconnect(host);
        return 3;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (sdk.Disconnect(host) != SDKReturnCode::Success) {
        std::cerr << "Disconnect failed\n";
        return 4;
    }
    return 0;
}
