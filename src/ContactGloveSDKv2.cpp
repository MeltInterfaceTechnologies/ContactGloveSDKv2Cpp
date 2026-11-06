#include "contactglove/ContactGloveSDKv2.hpp"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "osc/OscOutboundPacketStream.h"
#include "osc/OscPacketListener.h"
#include "osc/OscReceivedElements.h"
#include "ip/UdpSocket.h"

namespace ContactGloveSDKv2 {

namespace {
constexpr int32_t PROTOCOL_VERSION = 2;
constexpr std::size_t MAX_RECORDS_PER_MESSAGE = 8;
constexpr std::size_t MAX_CACHED_DEVICES = 16;
constexpr std::chrono::seconds KEEPALIVE_INTERVAL{30};
constexpr float HAPTICS_FREQUENCY_MIN_HZ = 100.0f;
constexpr float HAPTICS_FREQUENCY_MAX_HZ = 500.0f;
constexpr float HAPTICS_AMPLITUDE_MIN = 0.0f;
constexpr float HAPTICS_AMPLITUDE_MAX = 1.0f;
constexpr float QUATERNION_NORM_SQUARED_TOLERANCE = 0.01f;

template <typename Record>
struct CachedRecord {
    Record value;
    uint64_t updateSequence;
};

template <typename Record>
using RecordCache = std::unordered_map<std::string, CachedRecord<Record>>;

template <typename Record>
bool StoreAdmittedRecord(RecordCache<Record>& cache, const Record& record,
                         uint64_t update_sequence) {
    const auto existing = cache.find(record.id);
    if (existing != cache.end()) {
        existing->second = {record, update_sequence};
        return true;
    }

    if (cache.size() >= MAX_CACHED_DEVICES) return false;
    cache.emplace(record.id, CachedRecord<Record>{record, update_sequence});
    return true;
}

template <typename Record>
void StoreAdmittedRecords(RecordCache<Record>& cache, std::vector<Record>& records,
                          uint64_t& next_update_sequence) {
    auto accepted = records.begin();
    for (auto record = records.begin(); record != records.end(); ++record) {
        const uint64_t update_sequence = next_update_sequence + 1;
        if (StoreAdmittedRecord(cache, *record, update_sequence)) {
            next_update_sequence = update_sequence;
            if (accepted != record)
                *accepted = std::move(*record);
            ++accepted;
        }
    }
    records.erase(accepted, records.end());
}
constexpr float HAPTICS_DURATION_MIN_SECONDS = 0.001f;
constexpr float HAPTICS_DURATION_MAX_SECONDS = 1.0f;
constexpr float ANALOG_ONE_SIDED_MIN = 0.0f;
constexpr float ANALOG_ONE_SIDED_MAX = 1.0f;
constexpr float ANALOG_TWO_SIDED_MIN = -1.0f;
constexpr float ANALOG_TWO_SIDED_MAX = 1.0f;
constexpr std::array<uint32_t, 19> SKELETON_ROTATION_NODE_INDICES = {
    1, 2, 3,
    5, 6, 7, 8,
    10, 11, 12, 13,
    15, 16, 17, 18,
    20, 21, 22, 23,
};

bool IsValidButtonStatus(int32_t value) {
    return value >= static_cast<int32_t>(ButtonStatus::Unpressed) &&
           value <= static_cast<int32_t>(ButtonStatus::Pressed);
}

bool IsValidProtocolBool(int32_t value) {
    return value == 0 || value == 1;
}

bool IsValidBatteryPercentage(int32_t value) {
    return value == -1 || (value >= 0 && value <= 100);
}

bool IsValidDeviceFamilyType(int32_t value) {
    return value == static_cast<int32_t>(DeviceFamilyType::ContactGlove3);
}

bool IsValidDeviceColor(int32_t value) {
    return value >= static_cast<int32_t>(DeviceColor::Magenta) &&
           value <= static_cast<int32_t>(DeviceColor::Achromatic);
}

bool IsValidDeviceConnectedState(int32_t value) {
    return value >= static_cast<int32_t>(DeviceConnectedState::Unknown) &&
           value <= static_cast<int32_t>(DeviceConnectedState::Pairing);
}

bool IsValidTrackingState(int32_t value) {
    return value >= static_cast<int32_t>(TrackingState::Tracked) &&
           value <= static_cast<int32_t>(TrackingState::NotAttempted);
}

bool IsValidProfileStatus(int32_t value) {
    return value >= 0 && value <= 2;
}

bool IsValidProfileUuid(const std::string& value) {
    if (value.size() != 36) return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (c != '-') return false;
            continue;
        }
        const bool is_hex = (c >= '0' && c <= '9') ||
                            (c >= 'a' && c <= 'f') ||
                            (c >= 'A' && c <= 'F');
        if (!is_hex) return false;
    }
    return true;
}

bool IsFiniteInRange(float value, float min_value, float max_value) {
    return std::isfinite(value) && value >= min_value && value <= max_value;
}

bool IsValidQuaternion(const Quaternion& value) {
    if (!std::isfinite(value.w) || !std::isfinite(value.x) ||
        !std::isfinite(value.y) || !std::isfinite(value.z))
        return false;

    const float norm_squared = value.w * value.w + value.x * value.x +
                               value.y * value.y + value.z * value.z;
    return std::isfinite(norm_squared) &&
           std::fabs(norm_squared - 1.0f) <= QUATERNION_NORM_SQUARED_TOLERANCE;
}

bool IsValidControllerData(const ControllerData& data) {
    return IsFiniteInRange(data.trigger, ANALOG_ONE_SIDED_MIN, ANALOG_ONE_SIDED_MAX) &&
           IsFiniteInRange(data.gripValue, ANALOG_ONE_SIDED_MIN, ANALOG_ONE_SIDED_MAX) &&
           IsFiniteInRange(data.gripForce, ANALOG_ONE_SIDED_MIN, ANALOG_ONE_SIDED_MAX) &&
           IsFiniteInRange(data.joystickX, ANALOG_TWO_SIDED_MIN, ANALOG_TWO_SIDED_MAX) &&
           IsFiniteInRange(data.joystickY, ANALOG_TWO_SIDED_MIN, ANALOG_TWO_SIDED_MAX) &&
           IsFiniteInRange(data.trackpadX, ANALOG_TWO_SIDED_MIN, ANALOG_TWO_SIDED_MAX) &&
           IsFiniteInRange(data.trackpadY, ANALOG_TWO_SIDED_MIN, ANALOG_TWO_SIDED_MAX);
}

template <typename Callback, typename Record>
void DispatchCallbacks(const std::vector<Callback>& callbacks, const Record& record) {
    for (const auto& callback : callbacks) {
        try {
            callback(record);
        } catch (...) {
            // Match the C ABI/Rust boundary: one consumer must not prevent
            // other consumers or records in this message from being delivered.
        }
    }
}
}

class SDK::Impl {
public:
    struct Connection;

    class OscHandler : public osc::OscPacketListener {
    public:
        OscHandler(Impl& impl, Connection& connection) : impl_(impl), connection_(connection) {}
        void ProcessPacket(const char* data, int size,
                           const IpEndpointName& remoteEndpoint) override {
            // ReceivedPacket/ReceivedBundle/ReceivedMessage construction happens
            // outside ProcessMessage. Contain malformed framing at this boundary
            // so one invalid UDP datagram cannot terminate the receiver thread.
            try {
                osc::OscPacketListener::ProcessPacket(data, size, remoteEndpoint);
            } catch (const osc::Exception&) {
                // Ignore invalid OSC framing, just as ProcessMessage ignores
                // invalid protocol payloads. No callback is made for it.
            }
        }
    protected:
        void ProcessMessage(const osc::ReceivedMessage& msg,
                            const IpEndpointName&) override;
    private:
        Impl& impl_;
        Connection& connection_;
    };

    struct Connection {
        Host                                       host;
        std::unique_ptr<OscHandler>                handler;
        std::unique_ptr<UdpListeningReceiveSocket> socket;
        std::unique_ptr<UdpSocket>                 transmit_socket;
        std::thread                                thread;
        std::thread                                keepalive_thread;
        std::mutex                                 send_mtx;
        std::mutex                                 keepalive_mtx;
        std::condition_variable                    keepalive_cv;
        bool                                       stopping = false;
        bool                                       disconnecting = false; // Protected by conn_mtx.

        RecordCache<GloveData>               gloves;
        RecordCache<ControllerData>          controllers;
        RecordCache<ErgonomicsData>          ergonomics;
        RecordCache<SkeletonData>            skeletons;
        RecordCache<RawSensorTransformsData> raw_sensor_transforms;
        RecordCache<RawSkeletonData>         raw_skeletons;
    };

    std::vector<GloveDataStreamCallback_t>      glove_callbacks;
    std::vector<ControllerDataStreamCallback_t> controller_callbacks;
    std::vector<ErgonomicsStreamCallback_t>     ergonomics_callbacks;
    std::vector<SkeletonStreamCallback_t>       skeleton_callbacks;
    std::vector<RawSensorTransformsStreamCallback_t> raw_sensor_transforms_callbacks;
    std::vector<RawSkeletonStreamCallback_t>      raw_skeleton_callbacks;
    std::vector<RetargetProfileInfoCallback_t>    profile_info_callbacks;
    mutable std::mutex                          data_mtx;
    uint64_t                                    next_cache_update_sequence = 0;

    std::unordered_map<uint32_t, std::shared_ptr<Connection>> connections;
    std::mutex                               conn_mtx;
    std::condition_variable                  conn_cv;

    static void RemoveCachedDeviceRecords(Connection& connection, const std::string& id) {
        connection.gloves.erase(id);
        connection.controllers.erase(id);
        connection.ergonomics.erase(id);
        connection.skeletons.erase(id);
        connection.raw_sensor_transforms.erase(id);
        connection.raw_skeletons.erase(id);
    }

    static void ClearCachedRecords(Connection& connection) {
        connection.gloves.clear();
        connection.controllers.clear();
        connection.ergonomics.clear();
        connection.skeletons.clear();
        connection.raw_sensor_transforms.clear();
        connection.raw_skeletons.clear();
    }

    template <typename Record>
    const CachedRecord<Record>* FindMostRecentCachedRecord(
        const std::string& id, RecordCache<Record> Connection::*cache) const {
        const CachedRecord<Record>* latest = nullptr;
        for (const auto& entry : connections) {
            const auto found = (entry.second.get()->*cache).find(id);
            if (found != (entry.second.get()->*cache).end() &&
                (!latest || found->second.updateSequence > latest->updateSequence)) {
                latest = &found->second;
            }
        }
        return latest;
    }

    // ---------------------------------------------------------------------------
    // OSC send helpers
    // ---------------------------------------------------------------------------
    static void SendPacket(Connection& connection, const char* data, std::size_t size) {
        try {
            std::lock_guard<std::mutex> lock(connection.send_mtx);
            connection.transmit_socket->Send(data, size);
        } catch (...) {}
    }

    void SendToAllHosts(const char* data, std::size_t size) {
        std::lock_guard<std::mutex> lock(conn_mtx);
        for (auto& entry : connections)
            if (!entry.second->disconnecting)
                SendPacket(*entry.second, data, size);
    }

    static void SendConnect(Connection& connection) {
        char buf[64];
        osc::OutboundPacketStream p(buf, sizeof(buf));
        p << osc::BeginMessage("/DS2/HC/Connect")
          << static_cast<int32_t>(connection.host.receivePort)
          << osc::EndMessage;
        SendPacket(connection, p.Data(), p.Size());
    }

    // ---------------------------------------------------------------------------
    // Connection management
    // ---------------------------------------------------------------------------
    SDKReturnCode Connect(const Host& host) {
        if (host.sendPort > 65535) return SDKReturnCode::InvalidArgument;

        {
            std::unique_lock<std::mutex> lock(conn_mtx);
            // Retain the stopping connection's port reservation until both
            // threads and sockets are gone. Waiting releases conn_mtx so its
            // callbacks can still query cached data while Disconnect joins.
            conn_cv.wait(lock, [&] {
                const auto it = connections.find(host.receivePort);
                return it == connections.end() || !it->second->disconnecting;
            });
            const auto existing = connections.find(host.receivePort);
            if (existing != connections.end()) {
                auto& conn = *existing->second;
                if (conn.host.ipAddress != host.ipAddress ||
                    conn.host.port != host.port || conn.host.sendPort != host.sendPort)
                    return SDKReturnCode::InvalidArgument;
                // Retry registration without revoking pending/approved LAN sessions
                // or changing the OS-selected source port.
                SendConnect(conn);
                return SDKReturnCode::Success;
            }

            auto conn = std::make_shared<Connection>();
            conn->host = host;
            conn->handler = std::make_unique<OscHandler>(*this, *conn);
            try {
                conn->transmit_socket = std::make_unique<UdpSocket>();
                if (host.sendPort != 0) {
                    conn->transmit_socket->Bind(
                        IpEndpointName(IpEndpointName::ANY_ADDRESS, static_cast<int>(host.sendPort)));
                }
                conn->transmit_socket->Connect(
                    IpEndpointName(host.ipAddress.c_str(), static_cast<int>(host.port)));
                conn->socket = std::make_unique<UdpListeningReceiveSocket>(
                    IpEndpointName(IpEndpointName::ANY_ADDRESS, static_cast<int>(host.receivePort)),
                    conn->handler.get());
            } catch (...) {
                return SDKReturnCode::SdkNotAvailable;
            }
            connections.emplace(host.receivePort, conn);
            auto* sp = conn->socket.get();
            conn->thread = std::thread([sp]() { sp->Run(); });
            conn->keepalive_thread = std::thread([conn]() {
                std::unique_lock<std::mutex> lock(conn->keepalive_mtx);
                while (!conn->stopping) {
                    if (conn->keepalive_cv.wait_for(lock, KEEPALIVE_INTERVAL,
                                                   [&conn] { return conn->stopping; }))
                        break;
                    lock.unlock();
                    SendConnect(*conn);
                    lock.lock();
                }
            });

            // Keep the initial notification in the same critical section as
            // registration.  Otherwise Disconnect can erase this connection
            // before Connect re-finds it, or before the initial notification
            // is sent.
            SendConnect(*conn);
        }

        return SDKReturnCode::Success;
    }

    static void RequestStop(Connection& conn) {
        {
            std::lock_guard<std::mutex> lock(conn.keepalive_mtx);
            conn.stopping = true;
        }
        conn.keepalive_cv.notify_all();
        conn.socket->AsynchronousBreak();
    }

    static void FinishStop(Connection& conn) {
        if (conn.keepalive_thread.joinable()) conn.keepalive_thread.join();
        if (conn.thread.joinable()) conn.thread.join();
        // Release both fixed receive and send ports before allowing reconnect.
        conn.socket.reset();
        conn.transmit_socket.reset();
    }

    SDKReturnCode Disconnect(const Host& host) {
        std::unique_lock<std::mutex> lock(conn_mtx);
        auto it = connections.find(host.receivePort);
        if (it == connections.end()) return SDKReturnCode::NotConnected;
        const auto connection = it->second;
        if (connection->disconnecting) {
            // A concurrent caller owns this shutdown. Do not accidentally
            // disconnect a new session that reuses the same receive port.
            conn_cv.wait(lock, [&] {
                const auto current = connections.find(host.receivePort);
                return current == connections.end() || current->second != connection;
            });
            return SDKReturnCode::NotConnected;
        }
        auto& conn = *connection;
        conn.disconnecting = true;

        char buf[64];
        osc::OutboundPacketStream p(buf, sizeof(buf));
        p << osc::BeginMessage("/DS2/HC/Disconnect")
          << static_cast<int32_t>(host.receivePort)
          << osc::EndMessage;
        SendPacket(conn, p.Data(), p.Size());

        lock.unlock();
        RequestStop(conn);
        FinishStop(conn);
        lock.lock();
        {
            std::lock_guard<std::mutex> data_lock(data_mtx);
            ClearCachedRecords(conn);
        }
        connections.erase(host.receivePort);
        lock.unlock();
        conn_cv.notify_all();
        return SDKReturnCode::Success;
    }

    void Shutdown() {
        std::vector<std::shared_ptr<Connection>> pending;
        {
            std::lock_guard<std::mutex> lock(conn_mtx);
            for (auto& entry : connections) {
                entry.second->disconnecting = true;
                pending.push_back(entry.second);
            }
        }
        for (const auto& conn : pending) RequestStop(*conn);
        // Callbacks may query this Impl until their receiver has exited.
        // Keep its maps/mutexes alive without holding conn_mtx across join.
        for (const auto& conn : pending) FinishStop(*conn);
        {
            std::lock_guard<std::mutex> lock(conn_mtx);
            connections.clear();
        }
    }

    ~Impl() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// OSC message parsing
// ---------------------------------------------------------------------------
void SDK::Impl::OscHandler::ProcessMessage(const osc::ReceivedMessage& msg,
                                            const IpEndpointName&) {
    const char* addr = msg.AddressPattern();

    // ------------------------------------------------------------------
    // Helper lambdas (shared across all parsers)
    // ------------------------------------------------------------------
    auto arg       = msg.ArgumentsBegin();
    const auto end = msg.ArgumentsEnd();

    auto read_str = [&](std::string& out) -> bool {
        if (arg == end || arg->TypeTag() != osc::STRING_TYPE_TAG) return false;
        out = arg->AsStringUnchecked(); ++arg; return true;
    };
    auto read_i32 = [&](int32_t& out) -> bool {
        if (arg == end || arg->TypeTag() != osc::INT32_TYPE_TAG) return false;
        out = arg->AsInt32Unchecked(); ++arg; return true;
    };
    auto read_bool = [&](bool& out) -> bool {
        int32_t value = 0;
        if (!read_i32(value) || !IsValidProtocolBool(value)) return false;
        out = value == 1;
        return true;
    };
    auto read_button = [&](ButtonStatus& out) -> bool {
        int32_t value = 0;
        if (!read_i32(value) || !IsValidButtonStatus(value)) return false;
        out = static_cast<ButtonStatus>(value);
        return true;
    };
    auto read_f32 = [&](float& out) -> bool {
        if (arg == end || arg->TypeTag() != osc::FLOAT_TYPE_TAG) return false;
        const float value = arg->AsFloatUnchecked();
        ++arg;
        if (!std::isfinite(value)) return false;
        out = value;
        return true;
    };
    auto read_timestamp_hi_lo = [&](Timestamp& out) -> bool {
        int32_t hi = 0;
        int32_t lo = 0;
        if (!read_i32(hi)) return false;
        if (!read_i32(lo)) return false;
        const auto combined =
            (static_cast<uint64_t>(static_cast<uint32_t>(hi)) << 32) |
            static_cast<uint32_t>(lo);
        out = static_cast<Timestamp>(combined);
        return true;
    };

    try {
        // Skip unsupported protocol versions.
        int32_t version = 0;
        if (!read_i32(version)) return;
        if (version != PROTOCOL_VERSION) return;

        // ------------------------------------------------------------------
        // /DS2/HC/Device
        // Per device: string id, string pair_id, int battery, int is_haptics,
        //   int is_left, int device_type, int color, int connected_state,
        //   string name, int is_external_power_present
        // ------------------------------------------------------------------
        if (std::strcmp(addr, "/DS2/HC/Device") == 0) {
            std::vector<GloveData> updated;
            while (arg != end) {
                if (updated.size() >= MAX_RECORDS_PER_MESSAGE) return;

                GloveData g;
                int32_t tmp = 0;
                bool ok = true;
                ok = ok && read_str(g.id);
                ok = ok && read_str(g.pairId);
                ok = ok && read_i32(g.batteryPercentage);
                ok = ok && IsValidBatteryPercentage(g.batteryPercentage);
                ok = ok && read_bool(g.isHaptics);
                ok = ok && read_bool(g.isLeft);
                ok = ok && read_i32(tmp) && IsValidDeviceFamilyType(tmp);
                g.familyType = static_cast<DeviceFamilyType>(tmp);
                ok = ok && read_i32(tmp) && IsValidDeviceColor(tmp);
                g.color = static_cast<DeviceColor>(tmp);
                ok = ok && read_i32(tmp) && IsValidDeviceConnectedState(tmp);
                g.connectedState = static_cast<DeviceConnectedState>(tmp);
                ok = ok && read_str(g.name);
                ok = ok && read_bool(g.isExternalPowerPresent);
                if (!ok) return;
                updated.push_back(std::move(g));
            }
            if (updated.empty()) return;

            std::vector<GloveData> callback_records;
            std::vector<GloveDataStreamCallback_t> cbs;
            {
                std::lock_guard<std::mutex> lock(impl_.data_mtx);
                for (auto& record : updated) {
                    if (record.connectedState == DeviceConnectedState::Unconnected) {
                        // DivingStation keeps reporting configured but disconnected
                        // devices. They must not occupy a bounded active-device cache.
                        Impl::RemoveCachedDeviceRecords(connection_, record.id);
                        callback_records.push_back(std::move(record));
                        continue;
                    }

                    const uint64_t update_sequence = impl_.next_cache_update_sequence + 1;
                    if (StoreAdmittedRecord(connection_.gloves, record, update_sequence)) {
                        impl_.next_cache_update_sequence = update_sequence;
                        callback_records.push_back(std::move(record));
                    }
                }
                cbs = impl_.glove_callbacks;
            }
            for (const auto& g : callback_records)
                DispatchCallbacks(cbs, g);
            return;
        }

        // ------------------------------------------------------------------
        // /DS2/HC/Controller
        // Per controller: string id, int is_left,
        //   int a, int b, int sys,
        //   int trigger_button, int joystick_button, int trackpad_button,
        //   float trigger, float grip_value, float grip_force,
        //   float joystick_x, float joystick_y,
        //   float trackpad_x, float trackpad_y
        // ------------------------------------------------------------------
        if (std::strcmp(addr, "/DS2/HC/Controller") == 0) {
            std::vector<ControllerData> updated;
            while (arg != end) {
                if (updated.size() >= MAX_RECORDS_PER_MESSAGE) return;

                ControllerData c;
                int32_t tmp = 0;
                bool ok = true;
                ok = ok && read_str(c.id);
                ok = ok && read_bool(c.isLeft);
                ok = ok && read_button(c.aButton);
                ok = ok && read_button(c.bButton);
                ok = ok && read_button(c.sysButton);
                ok = ok && read_button(c.triggerButton);
                ok = ok && read_button(c.joystickButton);
                ok = ok && read_button(c.trackpadButton);
                ok = ok && read_f32(c.trigger);
                ok = ok && read_f32(c.gripValue);
                ok = ok && read_f32(c.gripForce);
                ok = ok && read_f32(c.joystickX);
                ok = ok && read_f32(c.joystickY);
                ok = ok && read_f32(c.trackpadX);
                ok = ok && read_f32(c.trackpadY);
                ok = ok && IsValidControllerData(c);
                if (!ok) return;
                updated.push_back(std::move(c));
            }
            if (updated.empty()) return;

            std::vector<ControllerDataStreamCallback_t> cbs;
            {
                std::lock_guard<std::mutex> lock(impl_.data_mtx);
                StoreAdmittedRecords(connection_.controllers, updated,
                                     impl_.next_cache_update_sequence);
                cbs = impl_.controller_callbacks;
            }
            for (const auto& c : updated)
                DispatchCallbacks(cbs, c);
            return;
        }

        // ------------------------------------------------------------------
        // /DS2/HC/HandSkeleton
        // Per device: string id, string profile_uuid, int is_left,
        //   int generated_at_ms_hi, int generated_at_ms_lo, int tracking_state,
        //   then 25 positions and 19 local quaternions.
        // ------------------------------------------------------------------
        if (std::strcmp(addr, "/DS2/HC/HandSkeleton") == 0) {
            SkeletonData s;
            int32_t tmp = 0;
            bool ok = true;
            ok = ok && read_str(s.id);
            ok = ok && read_str(s.profileUuid);
            ok = ok && IsValidProfileUuid(s.profileUuid);
            ok = ok && read_bool(s.isLeft);
            ok = ok && read_timestamp_hi_lo(s.generatedAt);
            ok = ok && read_i32(tmp);
            ok = ok && IsValidTrackingState(tmp);
            s.trackingState = static_cast<TrackingState>(tmp);
            for (auto& transform : s.transforms) {
                ok = ok && read_f32(transform.position.x);
                ok = ok && read_f32(transform.position.y);
                ok = ok && read_f32(transform.position.z);
            }
            for (auto node_index : SKELETON_ROTATION_NODE_INDICES) {
                auto& rotation = s.transforms[node_index].rotation;
                ok = ok && read_f32(rotation.w);
                ok = ok && read_f32(rotation.x);
                ok = ok && read_f32(rotation.y);
                ok = ok && read_f32(rotation.z);
            }
            for (const auto& transform : s.transforms)
                ok = ok && IsValidQuaternion(transform.rotation);
            if (!ok || arg != end) return;

            std::vector<SkeletonStreamCallback_t> cbs;
            {
                std::lock_guard<std::mutex> lock(impl_.data_mtx);
                const uint64_t update_sequence = impl_.next_cache_update_sequence + 1;
                if (!StoreAdmittedRecord(connection_.skeletons, s, update_sequence)) return;
                impl_.next_cache_update_sequence = update_sequence;
                cbs = impl_.skeleton_callbacks;
            }
            DispatchCallbacks(cbs, s);
            return;
        }

        // ------------------------------------------------------------------
        // /DS2/HC/Ergonomics
        // Per device: string id, int is_left, int generated_at_ms_hi,
        //   int generated_at_ms_lo, int tracking_state,
        //   float[ERGONOMICS_DATA_COUNT] joint values in degrees.
        // ------------------------------------------------------------------
        if (std::strcmp(addr, "/DS2/HC/Ergonomics") == 0) {
            std::vector<ErgonomicsData> updated;
            while (arg != end) {
                if (updated.size() >= MAX_RECORDS_PER_MESSAGE) return;

                ErgonomicsData e;
                int32_t tmp = 0;

                bool ok = true;
                ok = ok && read_str(e.id);
                ok = ok && read_bool(e.isLeft);
                ok = ok && read_timestamp_hi_lo(e.generatedAt);
                ok = ok && read_i32(tmp);
                ok = ok && IsValidTrackingState(tmp);
                e.trackingState = static_cast<TrackingState>(tmp);
                e.data.fill(0.0f);
                for (uint32_t i = 0; i < ERGONOMICS_DATA_COUNT; ++i) {
                    ok = ok && read_f32(e.data[i]);
                }
                if (!ok) return;
                updated.push_back(std::move(e));
            }
            if (updated.empty()) return;

            std::vector<ErgonomicsStreamCallback_t> cbs;
            {
                std::lock_guard<std::mutex> lock(impl_.data_mtx);
                StoreAdmittedRecords(connection_.ergonomics, updated,
                                     impl_.next_cache_update_sequence);
                cbs = impl_.ergonomics_callbacks;
            }
            for (const auto& e : updated)
                DispatchCallbacks(cbs, e);
            return;
        }

        // ------------------------------------------------------------------
        // /DS2/HC/HandSkeletonRaw
        // One record per message: string id, int is_left,
        //   int generated_at_ms_hi, int generated_at_ms_lo, int tracking_state,
        //   then 25 positions and 19 local quaternions.
        // ------------------------------------------------------------------
        if (std::strcmp(addr, "/DS2/HC/HandSkeletonRaw") == 0) {
            RawSkeletonData r;
            int32_t tmp = 0;
            bool ok = true;
            ok = ok && read_str(r.id);
            ok = ok && read_bool(r.isLeft);
            ok = ok && read_timestamp_hi_lo(r.generatedAt);
            ok = ok && read_i32(tmp);
            ok = ok && IsValidTrackingState(tmp);
            r.trackingState = static_cast<TrackingState>(tmp);
            for (auto& transform : r.transforms) {
                ok = ok && read_f32(transform.position.x);
                ok = ok && read_f32(transform.position.y);
                ok = ok && read_f32(transform.position.z);
            }
            for (auto node_index : SKELETON_ROTATION_NODE_INDICES) {
                auto& rotation = r.transforms[node_index].rotation;
                ok = ok && read_f32(rotation.w);
                ok = ok && read_f32(rotation.x);
                ok = ok && read_f32(rotation.y);
                ok = ok && read_f32(rotation.z);
            }
            for (const auto& transform : r.transforms)
                ok = ok && IsValidQuaternion(transform.rotation);
            if (!ok || arg != end) return;

            std::vector<RawSkeletonStreamCallback_t> cbs;
            {
                std::lock_guard<std::mutex> lock(impl_.data_mtx);
                const uint64_t update_sequence = impl_.next_cache_update_sequence + 1;
                if (!StoreAdmittedRecord(connection_.raw_skeletons, r, update_sequence)) return;
                impl_.next_cache_update_sequence = update_sequence;
                cbs = impl_.raw_skeleton_callbacks;
            }
            DispatchCallbacks(cbs, r);
            return;
        }

        // ------------------------------------------------------------------
        // /DS2/HC/HandSkeleton/Profile
        // int request_id, string profile_uuid, int status, string name,
        // string description
        // ------------------------------------------------------------------
        if (std::strcmp(addr, "/DS2/HC/HandSkeleton/Profile") == 0) {
            RetargetProfileInfo info;
            bool ok = read_i32(info.requestId) && read_str(info.profileUuid) &&
                      read_i32(info.status) && IsValidProfileStatus(info.status) &&
                      read_str(info.name) && read_str(info.description);
            if (ok && info.status == 0)
                ok = IsValidProfileUuid(info.profileUuid);
            if (ok && info.status == 1)
                ok = IsValidProfileUuid(info.profileUuid) && info.name.empty() && info.description.empty();
            if (ok && info.status == 2)
                ok = info.profileUuid.empty() && info.name.empty() && info.description.empty();
            if (!ok || arg != end) return;
            std::vector<RetargetProfileInfoCallback_t> cbs;
            {
                std::lock_guard<std::mutex> lock(impl_.data_mtx);
                cbs = impl_.profile_info_callbacks;
            }
            DispatchCallbacks(cbs, info);
            return;
        }

        // ------------------------------------------------------------------
        // /DS2/HC/RawSensorTransforms
        // Per device: string id, int is_left,
        //   int generated_at_ms_hi, int generated_at_ms_lo,
        //   then 5 raw sensor transforms: position xyz and rotation wxyz.
        // ------------------------------------------------------------------
        if (std::strcmp(addr, "/DS2/HC/RawSensorTransforms") == 0) {
            std::vector<RawSensorTransformsData> updated;
            while (arg != end) {
                if (updated.size() >= MAX_RECORDS_PER_MESSAGE) return;

                RawSensorTransformsData r;
                int32_t tmp = 0;

                bool ok = true;
                ok = ok && read_str(r.id);
                ok = ok && read_bool(r.isLeft);
                ok = ok && read_timestamp_hi_lo(r.generatedAt);
                for (auto& transform : r.transforms) {
                    ok = ok && read_f32(transform.position.x);
                    ok = ok && read_f32(transform.position.y);
                    ok = ok && read_f32(transform.position.z);
                    ok = ok && read_f32(transform.rotation.w);
                    ok = ok && read_f32(transform.rotation.x);
                    ok = ok && read_f32(transform.rotation.y);
                    ok = ok && read_f32(transform.rotation.z);
                    ok = ok && IsValidQuaternion(transform.rotation);
                }
                if (!ok) return;
                updated.push_back(std::move(r));
            }
            if (updated.empty()) return;

            std::vector<RawSensorTransformsStreamCallback_t> cbs;
            {
                std::lock_guard<std::mutex> lock(impl_.data_mtx);
                StoreAdmittedRecords(connection_.raw_sensor_transforms, updated,
                                     impl_.next_cache_update_sequence);
                cbs = impl_.raw_sensor_transforms_callbacks;
            }
            for (const auto& r : updated)
                DispatchCallbacks(cbs, r);
            return;
        }

    } catch (...) {}
}

// ---------------------------------------------------------------------------
// SDK public API
// ---------------------------------------------------------------------------
SDK::SDK()  : impl_(std::make_unique<Impl>()) {}
SDK::~SDK() {
    // Drain callbacks while impl_ still names a live implementation. Some
    // unique_ptr implementations clear their pointer before invoking the
    // deleter; waiting only in ~Impl would make callback getters dereference
    // a null impl_ on those standard libraries (notably libc++).
    impl_->Shutdown();
}

SDKReturnCode SDK::ConnectToHost(const Host& p_Host) {
    return impl_->Connect(p_Host);
}

SDKReturnCode SDK::Disconnect(const Host& p_Host) {
    return impl_->Disconnect(p_Host);
}

SDKReturnCode SDK::DoesGloveSupportHaptics(const std::string& id,
                                    bool*               p_IsHaptics) const {
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    const auto* record = impl_->FindMostRecentCachedRecord(id, &Impl::Connection::gloves);
    if (!record) return SDKReturnCode::InvalidID;
    if (p_IsHaptics) *p_IsHaptics = record->value.isHaptics;
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetDataForGlove(const std::string& id,
                                    GloveData*          p_GloveData) const {
    if (!p_GloveData) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    const auto* record = impl_->FindMostRecentCachedRecord(id, &Impl::Connection::gloves);
    if (!record) return SDKReturnCode::InvalidID;
    *p_GloveData = record->value;
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetControllerDataForGlove(const std::string& id,
                                              ControllerData*    p_Controller) const {
    if (!p_Controller) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    const auto* record = impl_->FindMostRecentCachedRecord(id, &Impl::Connection::controllers);
    if (!record) return SDKReturnCode::InvalidID;
    *p_Controller = record->value;
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetErgonomicsDataForGlove(const std::string& id,
                                              ErgonomicsData*    p_ErgonomicsData) const {
    if (!p_ErgonomicsData) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    const auto* record = impl_->FindMostRecentCachedRecord(id, &Impl::Connection::ergonomics);
    if (!record) return SDKReturnCode::InvalidID;
    *p_ErgonomicsData = record->value;
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetSkeletonDataForGlove(const std::string& id,
                                            SkeletonData*      p_SkeletonData) const {
    if (!p_SkeletonData) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    const auto* record = impl_->FindMostRecentCachedRecord(id, &Impl::Connection::skeletons);
    if (!record) return SDKReturnCode::InvalidID;
    *p_SkeletonData = record->value;
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetRawSkeletonDataForGlove(const std::string& id,
                                               RawSkeletonData* p_RawSkeletonData) const {
    if (!p_RawSkeletonData) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    const auto* record = impl_->FindMostRecentCachedRecord(id, &Impl::Connection::raw_skeletons);
    if (!record) return SDKReturnCode::InvalidID;
    *p_RawSkeletonData = record->value;
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetRawSensorTransformsForGlove(const std::string& id,
                                                   RawSensorTransformsData* p_RawSensorTransforms) const {
    if (!p_RawSensorTransforms) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    const auto* record = impl_->FindMostRecentCachedRecord(
        id, &Impl::Connection::raw_sensor_transforms);
    if (!record) return SDKReturnCode::InvalidID;
    *p_RawSensorTransforms = record->value;
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetIdsOfAvailableGloves(GloveId*  p_IdsOfAvailableGloves,
                                            uint32_t  p_NumberOfIdsThatFitInArray) const {
    if (!p_IdsOfAvailableGloves) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    std::unordered_set<std::string> ids;
    for (const auto& connection : impl_->connections)
        for (const auto& glove : connection.second->gloves)
            ids.insert(glove.first);
    uint32_t i = 0;
    for (const auto& id : ids) {
        if (i >= p_NumberOfIdsThatFitInArray) break;
        p_IdsOfAvailableGloves[i++] = id;
    }
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::GetNumberofAvailableGloves(uint32_t* n) const {
    if (!n) return SDKReturnCode::InvalidID;
    std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
    std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
    std::unordered_set<std::string> ids;
    for (const auto& connection : impl_->connections)
        for (const auto& glove : connection.second->gloves)
            ids.insert(glove.first);
    *n = static_cast<uint32_t>(ids.size());
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::VibrateGlove(const std::string& id,
                                 float              frequency,
                                 float              amplitude,
                                 float              duration) {
    if (!std::isfinite(frequency) ||
        !std::isfinite(amplitude) ||
        !std::isfinite(duration) ||
        frequency < HAPTICS_FREQUENCY_MIN_HZ ||
        frequency > HAPTICS_FREQUENCY_MAX_HZ ||
        amplitude < HAPTICS_AMPLITUDE_MIN ||
        amplitude > HAPTICS_AMPLITUDE_MAX ||
        duration < HAPTICS_DURATION_MIN_SECONDS ||
        duration > HAPTICS_DURATION_MAX_SECONDS) {
        return SDKReturnCode::InvalidArgument;
    }

    // Verify the glove exists
    {
        std::lock_guard<std::mutex> connection_lock(impl_->conn_mtx);
        std::lock_guard<std::mutex> data_lock(impl_->data_mtx);
        if (!impl_->FindMostRecentCachedRecord(id, &Impl::Connection::gloves))
            return SDKReturnCode::InvalidID;
    }

    // Build OSC address: /DS2/HC/{id}/Haptics/Body
    const std::string oscAddr = "/DS2/HC/" + id + "/Haptics/Body";

    char buf[256];
    osc::OutboundPacketStream p(buf, sizeof(buf));
    p << osc::BeginMessage(oscAddr.c_str())
      << PROTOCOL_VERSION
      << frequency
      << amplitude
      << duration
      << osc::EndMessage;

    impl_->SendToAllHosts(p.Data(), p.Size());
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RequestRetargetProfileInfo(const std::string& profileUuid, int32_t requestId) {
    if (!IsValidProfileUuid(profileUuid)) return SDKReturnCode::InvalidArgument;
    char buf[256];
    osc::OutboundPacketStream p(buf, sizeof(buf));
    p << osc::BeginMessage("/DS2/HC/HandSkeleton/Profile/Get")
      << PROTOCOL_VERSION
      << requestId
      << profileUuid.c_str()
      << osc::EndMessage;
    impl_->SendToAllHosts(p.Data(), p.Size());
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RegisterCallbackForGloveDataStream(GloveDataStreamCallback_t p_Callback) {
    std::lock_guard<std::mutex> lock(impl_->data_mtx);
    impl_->glove_callbacks.push_back(std::move(p_Callback));
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RegisterCallbackForControllerDataStream(ControllerDataStreamCallback_t p_Callback) {
    std::lock_guard<std::mutex> lock(impl_->data_mtx);
    impl_->controller_callbacks.push_back(std::move(p_Callback));
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RegisterCallbackForErgonomicsStream(ErgonomicsStreamCallback_t p_Callback) {
    std::lock_guard<std::mutex> lock(impl_->data_mtx);
    impl_->ergonomics_callbacks.push_back(std::move(p_Callback));
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RegisterCallbackForSkeletonStream(SkeletonStreamCallback_t p_Callback) {
    std::lock_guard<std::mutex> lock(impl_->data_mtx);
    impl_->skeleton_callbacks.push_back(std::move(p_Callback));
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RegisterCallbackForRawSkeletonStream(RawSkeletonStreamCallback_t p_Callback) {
    std::lock_guard<std::mutex> lock(impl_->data_mtx);
    impl_->raw_skeleton_callbacks.push_back(std::move(p_Callback));
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RegisterCallbackForRawSensorTransformsStream(RawSensorTransformsStreamCallback_t p_Callback) {
    std::lock_guard<std::mutex> lock(impl_->data_mtx);
    impl_->raw_sensor_transforms_callbacks.push_back(std::move(p_Callback));
    return SDKReturnCode::Success;
}

SDKReturnCode SDK::RegisterCallbackForRetargetProfileInfoStream(RetargetProfileInfoCallback_t p_Callback) {
    std::lock_guard<std::mutex> lock(impl_->data_mtx);
    impl_->profile_info_callbacks.push_back(std::move(p_Callback));
    return SDKReturnCode::Success;
}

} // namespace ContactGloveSDKv2
