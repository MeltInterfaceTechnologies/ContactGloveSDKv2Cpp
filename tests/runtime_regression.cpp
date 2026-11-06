// Runtime regressions against the SDK and its pinned oscpack implementation.
// UDP destinations are exclusively loopback. No hardware, haptics, or robot APIs.
#ifdef _WIN32
// Include Winsock before headers that might include windows.h.
#include <winsock2.h>
#include <process.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <contactglove/ContactGloveSDKv2.hpp>
#include "ip/NetworkingUtils.h"
#include "ip/TimerListener.h"
#include "ip/UdpSocket.h"
#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace ContactGloveSDKv2;
using namespace std::chrono_literals;
#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

class StopTimer final : public TimerListener {
public:
    explicit StopTimer(SocketReceiveMultiplexer& multiplexer) : multiplexer_(multiplexer) {}
    void TimerExpired() override { ++calls; multiplexer_.Break(); }
    unsigned calls = 0;
private:
    SocketReceiveMultiplexer& multiplexer_;
};

void pre_run_cancellation() {
    // The timer is also a deterministic fail-fast fallback in the UNPATCHED
    // implementation: resetting break_ at entry causes one unwanted callback.
    for (bool asynchronous : {false, true}) {
        SocketReceiveMultiplexer multiplexer;
        StopTimer timer(multiplexer);
        multiplexer.AttachPeriodicTimerListener(10, &timer);
        if (asynchronous) multiplexer.AsynchronousBreak();
        else multiplexer.Break();
        multiplexer.Run();
        REQUIRE(timer.calls == 0);

        // Cancellation is consumed at Run exit. Sequential reuse must process a
        // timer, including after the previous run left a pipe byte or signaled event.
        multiplexer.Run();
        REQUIRE(timer.calls == 1);
        if (asynchronous) multiplexer.AsynchronousBreak();
        else multiplexer.Break();
        multiplexer.Run();
        REQUIRE(timer.calls == 1);
        multiplexer.Run();
        REQUIRE(timer.calls == 2);
        multiplexer.DetachPeriodicTimerListener(&timer);
    }

    // The receiver cannot start until the asynchronous stop call has completed.
    // This reproduces the precise early-thread-start interleaving without sleeps.
    SocketReceiveMultiplexer multiplexer;
    StopTimer timer(multiplexer);
    multiplexer.AttachPeriodicTimerListener(10, &timer);
    std::atomic<bool> enter_run{false};
    std::thread receiver([&] {
        while (!enter_run.load(std::memory_order_acquire)) std::this_thread::yield();
        multiplexer.Run();
    });
    multiplexer.AsynchronousBreak();
    enter_run.store(true, std::memory_order_release);
    receiver.join();
    REQUIRE(timer.calls == 0);
    multiplexer.DetachPeriodicTimerListener(&timer);

    // A pre-Run stop must consume its wake notification as well as its flag.
    // Otherwise skipped reads accumulate one byte per Run on POSIX, eventually
    // filling the pipe and blocking AsynchronousBreak itself. This count exceeds
    // typical pipe capacities; CTest's timeout bounds a regressed implementation.
    constexpr int repeated_cancellations = 100000;
    for (int iteration = 0; iteration < repeated_cancellations; ++iteration) {
        multiplexer.AsynchronousBreak();
        multiplexer.Run();
    }
    std::cout << "PASS: deterministic pre-Run Break/AsynchronousBreak, sequential Run reuse, "
                 "100000 repeated cancellations\n";
}

#ifdef _WIN32
using SocketHandle = SOCKET;
using SocketAddressLength = int;
constexpr SocketHandle invalid_socket = INVALID_SOCKET;
unsigned process_id() { return static_cast<unsigned>(::_getpid()); }
void close_socket(SocketHandle socket) { ::closesocket(socket); }
#else
using SocketHandle = int;
using SocketAddressLength = socklen_t;
constexpr SocketHandle invalid_socket = -1;
unsigned process_id() { return static_cast<unsigned>(::getpid()); }
void close_socket(SocketHandle socket) { ::close(socket); }
#endif

class BoundUdp final {
public:
    explicit BoundUdp(bool receiver_reservation = false) {
        descriptor_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (descriptor_ == invalid_socket) throw std::runtime_error("socket creation failed");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bool bound = false;
        if (receiver_reservation) {
            // Probe a receive port below the usual Linux/macOS/Windows
            // ephemeral sender ranges. The SDK opens its sender before receiver;
            // releasing an ephemeral reservation could otherwise collide with
            // its own OS-selected sender during thousands of reconnects.
            for (unsigned attempt = 0; attempt < 1000 && !bound; ++attempt) {
                address.sin_port = htons(static_cast<std::uint16_t>(20000 + ((process_id() + attempt) % 10000)));
                bound = ::bind(descriptor_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
            }
        } else {
            bound = ::bind(descriptor_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
        }
        if (!bound) {
            release(); throw std::runtime_error("loopback port reservation failed");
        }
        SocketAddressLength size = sizeof(address);
        if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
            release(); throw std::runtime_error("port discovery failed");
        }
        port_ = ntohs(address.sin_port);
    }
    ~BoundUdp() { release(); }
    BoundUdp(const BoundUdp&) = delete;
    BoundUdp& operator=(const BoundUdp&) = delete;
    std::uint16_t port() const { return port_; }
    void release() {
        if (descriptor_ != invalid_socket) {
            close_socket(descriptor_);
            descriptor_ = invalid_socket;
        }
    }
    void send(std::uint16_t port, const std::vector<char>& packet) {
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        destination.sin_port = htons(port);
        // All fixtures fit in one small datagram. Winsock's length is int.
        REQUIRE(packet.size() <= 2048);
        const auto result = ::sendto(descriptor_, packet.data(), static_cast<int>(packet.size()), 0,
            reinterpret_cast<sockaddr*>(&destination), sizeof(destination));
        REQUIRE(result >= 0 && static_cast<std::size_t>(result) == packet.size());
    }
    std::vector<char> receive() {
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(descriptor_, &ready);
        timeval timeout{3, 0};
        REQUIRE(::select(static_cast<int>(descriptor_ + 1), &ready, nullptr, nullptr, &timeout) == 1);
        std::vector<char> packet(2048);
        const auto size = ::recvfrom(descriptor_, packet.data(), static_cast<int>(packet.size()), 0, nullptr, nullptr);
        REQUIRE(size > 0);
        packet.resize(static_cast<std::size_t>(size));
        return packet;
    }
private:
    SocketHandle descriptor_ = invalid_socket;
    std::uint16_t port_ = 0;
};

Host make_host(const BoundUdp& peer, const BoundUdp& reservation) {
    Host host;
    host.hostName = "runtime-regression-loopback";
    host.ipAddress = "127.0.0.1";
    host.port = peer.port();
    host.receivePort = reservation.port();
    host.sendPort = 0;
    return host;
}

std::int32_t osc_int32_bits(std::uint32_t word) {
    // Preserve the OSC int32 wire bits even when the unsigned word exceeds INT32_MAX.
    std::int32_t result;
    std::memcpy(&result, &word, sizeof(result));
    return result;
}

std::vector<char> raw_skeleton(Timestamp timestamp) {
    std::array<char, 2048> buffer{};
    osc::OutboundPacketStream packet(buffer.data(), buffer.size());
    const std::int32_t timestamp_hi = osc_int32_bits(static_cast<std::uint32_t>(timestamp >> 32));
    const std::int32_t timestamp_lo = osc_int32_bits(static_cast<std::uint32_t>(timestamp));
    packet << osc::BeginMessage("/DS2/HC/HandSkeletonRaw")
           << std::int32_t(2)                         // version
           << "runtime-regression-hand"              // id
           << std::int32_t(1)                         // isLeft
           << timestamp_hi << timestamp_lo           // generatedAt, high word first
           << static_cast<std::int32_t>(TrackingState::Tracked);
    for (int node = 0; node < 25; ++node)
        packet << (static_cast<float>(node) * 0.001f) << 0.0f << 0.0f;
    for (int node = 0; node < 19; ++node)
        packet << 1.0f << 0.0f << 0.0f << 0.0f;
    packet << osc::EndMessage;
    return {packet.Data(), packet.Data() + packet.Size()};
}

std::vector<char> bundle(const std::vector<char>& element) {
    std::vector<char> result(20 + element.size(), 0);
    const char header[16] = {'#', 'b', 'u', 'n', 'd', 'l', 'e', '\0',
                            0, 0, 0, 0, 0, 0, 0, 1};
    std::memcpy(result.data(), header, sizeof(header));
    const std::uint32_t length = htonl(static_cast<std::uint32_t>(element.size()));
    std::memcpy(result.data() + 16, &length, sizeof(length));
    if (!element.empty()) std::memcpy(result.data() + 20, element.data(), element.size());
    return result;
}

void timestamp_words() {
    // Literal expectations are independent of raw_skeleton's splitting logic.
    struct Case { Timestamp value; std::int32_t hi; std::int32_t lo; };
    const Case cases[] = {
        {0, 0, 0},
        {1, 0, 1},
        {0x0000000100000000ULL, 1, 0},
        {0x1234567823456789ULL, 0x12345678, 0x23456789},
        {0x80000000ffffffffULL, (std::numeric_limits<std::int32_t>::min)(), -1},
        {0xffffffff80000000ULL, -1, (std::numeric_limits<std::int32_t>::min)()},
        {0xffffffffffffffffULL, -1, -1},
    };
    BoundUdp peer, reservation(true);
    const auto host = make_host(peer, reservation);
    reservation.release();
    struct CallbackState {
        std::mutex mutex;
        std::condition_variable condition;
        std::vector<RawSkeletonData> frames;
    } state;
    SDK sdk;
    REQUIRE(sdk.RegisterCallbackForRawSkeletonStream([&](const RawSkeletonData& frame) {
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.frames.push_back(frame);
        }
        state.condition.notify_all();
    }) == SDKReturnCode::Success);
    REQUIRE(sdk.ConnectToHost(host) == SDKReturnCode::Success);
    std::size_t expected_count = 0;
    for (const auto& fixture : cases) {
        const auto packet = raw_skeleton(fixture.value);
        const osc::ReceivedPacket received(packet.data(), static_cast<int>(packet.size()));
        const osc::ReceivedMessage message(received);
        auto arg = message.ArgumentsBegin();
        REQUIRE(static_cast<std::uint32_t>((arg++)->AsInt32()) == 2u);
        REQUIRE(std::string((arg++)->AsString()) == "runtime-regression-hand");
        REQUIRE(static_cast<std::uint32_t>((arg++)->AsInt32()) == 1u);
        // Compare the protocol's 32 wire bits. Pinned oscpack's int32 typedef
        // is long on arm64, so its C++ return type need not be std::int32_t.
        REQUIRE(static_cast<std::uint32_t>((arg++)->AsInt32()) == static_cast<std::uint32_t>(fixture.hi));
        REQUIRE(static_cast<std::uint32_t>((arg++)->AsInt32()) == static_cast<std::uint32_t>(fixture.lo));
        REQUIRE(static_cast<std::uint32_t>((arg++)->AsInt32()) == static_cast<std::uint32_t>(TrackingState::Tracked));
        peer.send(static_cast<std::uint16_t>(host.receivePort), packet);
        ++expected_count;
        std::unique_lock<std::mutex> lock(state.mutex);
        REQUIRE(state.condition.wait_for(lock, 3s, [&] { return state.frames.size() >= expected_count; }));
        REQUIRE(state.frames.size() == expected_count);
        REQUIRE(state.frames.back().generatedAt == fixture.value);
        REQUIRE(state.frames.back().isLeft);
        REQUIRE(state.frames.back().trackingState == TrackingState::Tracked);
    }
    REQUIRE(sdk.Disconnect(host) == SDKReturnCode::Success);
    std::cout << "PASS: explicit OSC timestamp word order and 64-bit SDK callback reconstruction\n";
}

void malformed_datagrams() {
    BoundUdp peer, reservation(true);
    const auto host = make_host(peer, reservation);
    reservation.release();
    struct CallbackState {
        std::mutex mutex;
        std::condition_variable condition;
        std::vector<RawSkeletonData> frames;
    } state;
    SDK sdk; // Destroy/join receiver before the callback state goes out of scope.
    REQUIRE(sdk.RegisterCallbackForRawSkeletonStream([&](const RawSkeletonData& frame) {
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.frames.push_back(frame);
        }
        state.condition.notify_all();
    }) == SDKReturnCode::Success);
    REQUIRE(sdk.ConnectToHost(host) == SDKReturnCode::Success);

    auto barrier = [&](std::int32_t timestamp, bool in_bundle = false) {
        const auto packet = raw_skeleton(timestamp);
        peer.send(static_cast<std::uint16_t>(host.receivePort), in_bundle ? bundle(packet) : packet);
        std::unique_lock<std::mutex> lock(state.mutex);
        REQUIRE(state.condition.wait_for(lock, 3s, [&] {
            return !state.frames.empty() && state.frames.back().generatedAt == static_cast<Timestamp>(timestamp);
        }));
        REQUIRE(state.frames.size() == static_cast<std::size_t>(timestamp));
        REQUIRE(state.frames.back().id == "runtime-regression-hand");
        REQUIRE(state.frames.back().isLeft);
        REQUIRE(state.frames.back().trackingState == TrackingState::Tracked);
        REQUIRE(state.frames.back().transforms[1].rotation.w == 1.0f);
    };
    barrier(1); // Prove the registered callback and receiver work before injection.
    auto truncated_arguments = raw_skeleton(999);
    truncated_arguments.resize(truncated_arguments.size() - sizeof(float));
    const std::vector<char> unterminated_address{'/', 'b', 'a', 'd'}; // 2f626164
    const std::vector<char> truncated_bundle{'#', 'b', 'u', 'n', 'd', 'l', 'e', '\0'};
    const std::vector<std::vector<char>> malformed{
        unterminated_address,
        truncated_bundle,
        bundle(unterminated_address),
        bundle(truncated_bundle),
        bundle(std::vector<char>{'/', 'b', 'a'}),
        truncated_arguments,
        bundle(bundle(unterminated_address)),
    };
    std::int32_t timestamp = 1;
    for (const auto& packet : malformed) {
        peer.send(static_cast<std::uint16_t>(host.receivePort), packet);
        // Same-socket loopback UDP barrier: exactly one valid callback follows
        // each malformed packet. There is no sleep-based absence assertion.
        ++timestamp;
        barrier(timestamp, timestamp % 2 == 0);
    }
    REQUIRE(sdk.Disconnect(host) == SDKReturnCode::Success);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        REQUIRE(state.frames.size() == malformed.size() + 1);
    }
    std::cout << "PASS: seven malformed message/bundle fixtures discarded; subsequent actual SDK callbacks survive\n";
}

void rapid_sdk_lifecycle() {
    BoundUdp peer, reservation(true);
    const auto host = make_host(peer, reservation);
    reservation.release();
    SDK sdk;
    constexpr int sequential_iterations = 250;
    for (int iteration = 0; iteration < sequential_iterations; ++iteration) {
        REQUIRE(sdk.ConnectToHost(host) == SDKReturnCode::Success);
        REQUIRE(sdk.Disconnect(host) == SDKReturnCode::Success);
        REQUIRE(sdk.Disconnect(host) == SDKReturnCode::NotConnected);
    }
    constexpr int concurrent_iterations = 250;
    for (int iteration = 0; iteration < concurrent_iterations; ++iteration) {
        std::atomic<bool> start{false}, finished{false}, failed{false};
        SDKReturnCode connected = SDKReturnCode::SdkNotAvailable;
        std::thread connector([&] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            try { connected = sdk.ConnectToHost(host); }
            catch (...) { failed.store(true, std::memory_order_release); }
            finished.store(true, std::memory_order_release);
        });
        start.store(true, std::memory_order_release);
        while (!finished.load(std::memory_order_acquire)) {
            try {
                const auto result = sdk.Disconnect(host);
                if (result != SDKReturnCode::Success && result != SDKReturnCode::NotConnected)
                    failed.store(true, std::memory_order_release);
            } catch (...) { failed.store(true, std::memory_order_release); }
            std::this_thread::yield();
        }
        connector.join();
        const auto result = sdk.Disconnect(host);
        REQUIRE(result == SDKReturnCode::Success || result == SDKReturnCode::NotConnected);
        REQUIRE(connected == SDKReturnCode::Success);
        REQUIRE(!failed.load(std::memory_order_acquire));
    }
    // Destruction also uses AsynchronousBreak and may race receiver startup.
    for (int iteration = 0; iteration < 100; ++iteration) {
        SDK temporary;
        REQUIRE(temporary.ConnectToHost(host) == SDKReturnCode::Success);
    }
    std::cout << "PASS: 250 immediate, 250 concurrent Connect/Disconnect, 100 immediate SDK destructions\n";
}

void callback_shutdown() {
    BoundUdp peer, reservation(true), sender_reservation(true);
    auto host = make_host(peer, reservation);
    // Check release of BOTH fixed ports before the waiting reconnect binds.
    host.sendPort = sender_reservation.port();
    reservation.release();
    sender_reservation.release();
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false, release = false, connected = false;
    SDKReturnCode query = SDKReturnCode::SdkNotAvailable;
    SDKReturnCode disconnected = SDKReturnCode::SdkNotAvailable;
    SDKReturnCode reconnected = SDKReturnCode::SdkNotAvailable;
    SDK sdk;
    REQUIRE(sdk.RegisterCallbackForRawSkeletonStream([&](const RawSkeletonData&) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            entered = true;
            condition.notify_all();
            condition.wait(lock, [&] { return release; });
        }
        uint32_t count = 0;
        query = sdk.GetNumberofAvailableGloves(&count);
    }) == SDKReturnCode::Success);
    REQUIRE(sdk.ConnectToHost(host) == SDKReturnCode::Success);
    peer.receive(); // Initial Connect.
    peer.send(static_cast<std::uint16_t>(host.receivePort), raw_skeleton(1));
    {
        std::unique_lock<std::mutex> lock(mutex);
        REQUIRE(condition.wait_for(lock, 3s, [&] { return entered; }));
    }
    std::thread disconnector([&] { disconnected = sdk.Disconnect(host); });
    const auto packet = peer.receive();
    REQUIRE(std::string(packet.data()) == "/DS2/HC/Disconnect");
    // The packet proves Disconnect owns the shutdown before the getter or
    // reconnect runs. Keep the callback alive while reconnect is attempted.
    std::thread connector([&] {
        reconnected = sdk.ConnectToHost(host);
        std::lock_guard<std::mutex> lock(mutex);
        connected = true;
        condition.notify_all();
    });
    bool connected_early;
    {
        std::unique_lock<std::mutex> lock(mutex);
        connected_early = condition.wait_for(lock, 50ms, [&] { return connected; });
        release = true;
    }
    condition.notify_all();
    disconnector.join(); // CTest bounds a regressed lock-held join.
    connector.join();
    REQUIRE(!connected_early);
    REQUIRE(query == SDKReturnCode::Success);
    REQUIRE(disconnected == SDKReturnCode::Success);
    REQUIRE(reconnected == SDKReturnCode::Success);
    REQUIRE(std::string(peer.receive().data()) == "/DS2/HC/Connect");
    RawSkeletonData old;
    REQUIRE(sdk.GetRawSkeletonDataForGlove("runtime-regression-hand", &old) == SDKReturnCode::InvalidID);
    REQUIRE(sdk.Disconnect(host) == SDKReturnCode::Success);
    std::cout << "PASS: callback getter during Disconnect, same-port reconnect waits, old cache cleared\n";
}

void callback_destruction() {
    BoundUdp peer, reservation(true);
    const auto host = make_host(peer, reservation);
    reservation.release();
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false, release = false, destroying = false, destroyed = false;
    SDKReturnCode query = SDKReturnCode::SdkNotAvailable;
    auto sdk = std::make_unique<SDK>();
    auto* callback_sdk = sdk.get();
    sdk->RegisterCallbackForRawSkeletonStream([&](const RawSkeletonData&) {
        // Complete SDK access before announcing that destruction may start.
        // Only callback draining, not access to a destructing SDK, is tested.
        uint32_t count = 0;
        query = callback_sdk->GetNumberofAvailableGloves(&count);
        {
            std::unique_lock<std::mutex> lock(mutex);
            entered = true;
            condition.notify_all();
            condition.wait(lock, [&] { return release; });
        }
    });
    REQUIRE(sdk->ConnectToHost(host) == SDKReturnCode::Success);
    peer.send(static_cast<std::uint16_t>(host.receivePort), raw_skeleton(1));
    {
        std::unique_lock<std::mutex> lock(mutex);
        REQUIRE(condition.wait_for(lock, 3s, [&] { return entered; }));
    }
    std::thread destroyer([&] {
        {
            std::lock_guard<std::mutex> lock(mutex);
            destroying = true;
            condition.notify_all();
        }
        sdk.reset();
        std::lock_guard<std::mutex> lock(mutex);
        destroyed = true;
        condition.notify_all();
    });
    bool destroyed_early;
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&] { return destroying; });
        destroyed_early = condition.wait_for(lock, 50ms, [&] { return destroyed; });
        release = true;
    }
    condition.notify_all();
    destroyer.join();
    REQUIRE(!destroyed_early);
    REQUIRE(query == SDKReturnCode::Success);
    std::cout << "PASS: destruction waits for an active callback after its last SDK access\n";
}

void callback_exceptions() {
    BoundUdp peer, reservation(true);
    const auto host = make_host(peer, reservation);
    reservation.release();
    std::mutex mutex;
    std::condition_variable condition;
    int first = 0, second = 0;
    SDK sdk;
    sdk.RegisterCallbackForRawSkeletonStream([&](const RawSkeletonData&) {
        ++first;
        throw std::runtime_error("expected callback failure");
    });
    sdk.RegisterCallbackForRawSkeletonStream([&](const RawSkeletonData&) {
        std::lock_guard<std::mutex> lock(mutex);
        ++second;
        condition.notify_all();
    });
    REQUIRE(sdk.ConnectToHost(host) == SDKReturnCode::Success);
    for (int expected = 1; expected <= 2; ++expected) {
        peer.send(static_cast<std::uint16_t>(host.receivePort), raw_skeleton(expected));
        std::unique_lock<std::mutex> lock(mutex);
        REQUIRE(condition.wait_for(lock, 3s, [&] { return second == expected; }));
    }
    REQUIRE(sdk.Disconnect(host) == SDKReturnCode::Success);
    REQUIRE(first == 2 && second == 2);
    std::cout << "PASS: callback exceptions do not skip siblings or subsequent messages\n";
}
} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "--all";
    if (argc > 2 || (mode != "--all" && mode != "--pre-run" && mode != "--malformed" && mode != "--timestamp" && mode != "--rapid" && mode != "--callbacks")) {
        std::cerr << "usage: runtime_regression [--all|--pre-run|--malformed|--timestamp|--rapid|--callbacks]\n";
        return 2;
    }
    try {
        // Keep Winsock initialized across raw socket helpers and SDK threads.
        // This is a no-op on POSIX; oscpack recommends process-wide lifetime.
        NetworkInitializer networking;
        if (mode == "--all" || mode == "--pre-run") pre_run_cancellation();
        if (mode == "--all" || mode == "--malformed") malformed_datagrams();
        if (mode == "--all" || mode == "--timestamp") timestamp_words();
        if (mode == "--all" || mode == "--rapid") rapid_sdk_lifecycle();
        if (mode == "--all" || mode == "--callbacks") {
            callback_shutdown();
            callback_destruction();
            callback_exceptions();
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
