#include "contactglove/ContactGloveSDKv2.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>

#include "ip/UdpSocket.h"
#include "osc/OscOutboundPacketStream.h"

namespace {

using namespace ContactGloveSDKv2;
static_assert(std::is_same_v<LedColor, DeviceColor>, "Old source uses the same display-color type");

constexpr int32_t kProtocolVersion = 2;
constexpr int kReceivePort = 30250;
constexpr int kDivingStationPort = 30251;
constexpr int kFirstCachePort = 30260;
constexpr int kSecondCachePort = 30262;
constexpr char kProfileUuid[] = "12345678-1234-4abc-8def-1234567890ab";

struct DeviceFields {
    int32_t batteryPercentage = 50;
    int32_t isHaptics = 1;
    int32_t isLeft = 1;
    int32_t familyType = static_cast<int32_t>(DeviceFamilyType::ContactGlove3);
    int32_t color = static_cast<int32_t>(DeviceColor::Green);
    int32_t connectedState = static_cast<int32_t>(DeviceConnectedState::Connected);
    int32_t isExternalPowerPresent = 0;
};

void SendDevice(UdpTransmitSocket& socket, const char* id, const DeviceFields& fields) {
    char buffer[512];
    osc::OutboundPacketStream packet(buffer, sizeof(buffer));
    packet << osc::BeginMessage("/DS2/HC/Device")
           << kProtocolVersion
           << id
           << ""
           << fields.batteryPercentage
           << fields.isHaptics
           << fields.isLeft
           << fields.familyType
           << fields.color
           << fields.connectedState
           << "ContactGlove3"
           << fields.isExternalPowerPresent
           << osc::EndMessage;
    socket.Send(packet.Data(), packet.Size());
}

void AppendSkeletonValues(osc::OutboundPacketStream& packet) {
    for (int index = 0; index < 25 * 3; ++index)
        packet << 0.0f;
    for (int index = 0; index < 19; ++index)
        packet << 1.0f << 0.0f << 0.0f << 0.0f;
}

void SendSkeleton(UdpTransmitSocket& socket, const char* id, bool appendTrailingArgument) {
    char buffer[2048];
    osc::OutboundPacketStream packet(buffer, sizeof(buffer));
    packet << osc::BeginMessage("/DS2/HC/HandSkeleton")
           << kProtocolVersion
           << id
           << kProfileUuid
           << 1
           << 0
           << 1
           << static_cast<int32_t>(TrackingState::Tracked);
    AppendSkeletonValues(packet);
    if (appendTrailingArgument) packet << 0;
    packet << osc::EndMessage;
    socket.Send(packet.Data(), packet.Size());
}

void SendRawSkeleton(UdpTransmitSocket& socket, const char* id, bool appendTrailingArgument) {
    char buffer[2048];
    osc::OutboundPacketStream packet(buffer, sizeof(buffer));
    packet << osc::BeginMessage("/DS2/HC/HandSkeletonRaw")
           << kProtocolVersion
           << id
           << 1
           << 0
           << 1
           << static_cast<int32_t>(TrackingState::Tracked);
    AppendSkeletonValues(packet);
    if (appendTrailingArgument) packet << 0;
    packet << osc::EndMessage;
    socket.Send(packet.Data(), packet.Size());
}

template <typename Getter>
bool WaitForSuccess(Getter&& getter) {
    for (int attempt = 0; attempt < 25; ++attempt) {
        if (getter() == SDKReturnCode::Success) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

template <typename Getter>
bool IsStillRejected(Getter&& getter) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return getter() == SDKReturnCode::InvalidID;
}

bool RejectsInvalidDevice(SDK& sdk, UdpTransmitSocket& socket, const char* id,
                          const DeviceFields& fields) {
    SendDevice(socket, id, fields);
    GloveData data;
    return IsStillRejected([&] { return sdk.GetDataForGlove(id, &data); });
}

} // namespace

int main() {
    SDK sdk;
    std::atomic<bool> received_unconnected_callback{false};
    sdk.RegisterCallbackForGloveDataStream([&](const GloveData& record) {
        if (record.id == "device-cap-0" &&
            record.connectedState == DeviceConnectedState::Unconnected) {
            received_unconnected_callback.store(true, std::memory_order_release);
        }
    });

    Host host;
    host.hostName = "protocol-v2-validation";
    host.ipAddress = "127.0.0.1";
    host.port = kDivingStationPort;
    host.receivePort = kReceivePort;

    if (sdk.ConnectToHost(host) != SDKReturnCode::Success) {
        std::cerr << "ConnectToHost failed\n";
        return 1;
    }

    UdpTransmitSocket sender(IpEndpointName("127.0.0.1", kReceivePort));
    DeviceFields valid;
    SendDevice(sender, "device-valid", valid);
    GloveData glove;
    if (!WaitForSuccess([&] { return sdk.GetDataForGlove("device-valid", &glove); })) {
        std::cerr << "valid Device record was not received\n";
        return 2;
    }

    DeviceFields invalid = valid;
    invalid.batteryPercentage = -2;
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-negative-battery", invalid)) return 3;
    invalid = valid;
    invalid.batteryPercentage = 101;
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-battery", invalid)) return 4;
    invalid = valid;
    invalid.isHaptics = 2;
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-haptics", invalid)) return 5;
    invalid = valid;
    invalid.isLeft = -1;
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-left", invalid)) return 6;
    invalid = valid;
    invalid.familyType = static_cast<int32_t>(DeviceFamilyType::ContactGlove2);
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-family", invalid)) return 7;
    invalid = valid;
    invalid.color = 8;
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-color", invalid)) return 8;
    invalid = valid;
    invalid.connectedState = 4;
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-state", invalid)) return 9;
    invalid = valid;
    invalid.isExternalPowerPresent = 2;
    if (!RejectsInvalidDevice(sdk, sender, "device-invalid-external-power", invalid)) return 10;

    SendSkeleton(sender, "skeleton-valid", false);
    SkeletonData skeleton;
    if (!WaitForSuccess([&] { return sdk.GetSkeletonDataForGlove("skeleton-valid", &skeleton); })) {
        std::cerr << "valid HandSkeleton record was not received\n";
        return 11;
    }
    SendSkeleton(sender, "skeleton-trailing", true);
    SkeletonData trailingSkeleton;
    if (!IsStillRejected([&] {
            return sdk.GetSkeletonDataForGlove("skeleton-trailing", &trailingSkeleton);
        }))
        return 12;

    SendRawSkeleton(sender, "raw-skeleton-valid", false);
    RawSkeletonData rawSkeleton;
    if (!WaitForSuccess([&] {
            return sdk.GetRawSkeletonDataForGlove("raw-skeleton-valid", &rawSkeleton);
        })) {
        std::cerr << "valid HandSkeletonRaw record was not received\n";
        return 13;
    }
    SendRawSkeleton(sender, "raw-skeleton-trailing", true);
    RawSkeletonData trailingRawSkeleton;
    if (!IsStillRejected([&] {
            return sdk.GetRawSkeletonDataForGlove("raw-skeleton-trailing", &trailingRawSkeleton);
        }))
        return 14;

    if (sdk.Disconnect(host) != SDKReturnCode::Success) {
        std::cerr << "Disconnect failed\n";
        return 15;
    }

    Host first_cache_host;
    first_cache_host.hostName = "first-cache-session";
    first_cache_host.ipAddress = "127.0.0.1";
    first_cache_host.port = kDivingStationPort;
    first_cache_host.receivePort = kFirstCachePort;
    if (sdk.ConnectToHost(first_cache_host) != SDKReturnCode::Success) return 16;
    UdpTransmitSocket first_cache_sender(IpEndpointName("127.0.0.1", kFirstCachePort));
    for (int index = 0; index < 16; ++index)
        SendDevice(first_cache_sender, ("device-cap-" + std::to_string(index)).c_str(), valid);
    if (!WaitForSuccess([&] {
            return sdk.GetDataForGlove("device-cap-15", &glove);
        }))
        return 17;

    SendSkeleton(first_cache_sender, "device-cap-0", false);
    if (!WaitForSuccess([&] {
            return sdk.GetSkeletonDataForGlove("device-cap-0", &skeleton);
        }))
        return 18;

    SendDevice(first_cache_sender, "device-cap-overflow", valid);
    if (!IsStillRejected([&] {
            return sdk.GetDataForGlove("device-cap-overflow", &glove);
        }))
        return 19;

    DeviceFields disconnected = valid;
    disconnected.connectedState = static_cast<int32_t>(DeviceConnectedState::Unconnected);
    SendDevice(first_cache_sender, "device-cap-0", disconnected);
    if (!IsStillRejected([&] {
            return sdk.GetDataForGlove("device-cap-0", &glove);
        }) || sdk.GetSkeletonDataForGlove("device-cap-0", &skeleton) != SDKReturnCode::InvalidID ||
        !received_unconnected_callback.load(std::memory_order_acquire))
        return 20;

    SendDevice(first_cache_sender, "device-cap-overflow", valid);
    if (!WaitForSuccess([&] {
            return sdk.GetDataForGlove("device-cap-overflow", &glove);
        }))
        return 21;

    Host second_cache_host;
    second_cache_host.hostName = "second-cache-session";
    second_cache_host.ipAddress = "127.0.0.1";
    second_cache_host.port = kDivingStationPort;
    second_cache_host.receivePort = kSecondCachePort;
    if (sdk.ConnectToHost(second_cache_host) != SDKReturnCode::Success) return 22;
    UdpTransmitSocket second_cache_sender(IpEndpointName("127.0.0.1", kSecondCachePort));
    SendDevice(second_cache_sender, "device-second-session", valid);
    if (!WaitForSuccess([&] {
            return sdk.GetDataForGlove("device-second-session", &glove);
        }))
        return 23;

    if (sdk.Disconnect(first_cache_host) != SDKReturnCode::Success) return 24;
    if (sdk.GetDataForGlove("device-cap-overflow", &glove) != SDKReturnCode::InvalidID)
        return 25;
    if (sdk.GetDataForGlove("device-second-session", &glove) != SDKReturnCode::Success)
        return 26;

    if (sdk.Disconnect(second_cache_host) != SDKReturnCode::Success) return 27;
    if (sdk.GetDataForGlove("device-second-session", &glove) != SDKReturnCode::InvalidID)
        return 28;

    if (sdk.ConnectToHost(first_cache_host) != SDKReturnCode::Success) return 29;
    SendDevice(first_cache_sender, "device-after-reconnect", valid);
    if (!WaitForSuccess([&] {
            return sdk.GetDataForGlove("device-after-reconnect", &glove);
        }))
        return 30;
    if (sdk.Disconnect(first_cache_host) != SDKReturnCode::Success) return 31;
    return 0;
}
