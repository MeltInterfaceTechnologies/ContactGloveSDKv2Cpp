#pragma once

/**
 * @file ContactGloveSDKv2.hpp
 * @brief Public header for ContactGlove C++/C/Rust SDK V2
 */

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if defined(CONTACTGLOVE_SDK_STATIC)
  #define CONTACTGLOVE_API
#elif defined(_WIN32)
  #ifdef CONTACTGLOVE_SDK_EXPORTS
    #define CONTACTGLOVE_API __declspec(dllexport)
  #else
    #define CONTACTGLOVE_API __declspec(dllimport)
  #endif
#else
  #define CONTACTGLOVE_API __attribute__((visibility("default")))
#endif

/**
 * @namespace ContactGloveSDKv2
 * @brief ContactGlove C++/C/Rust SDK V2 provides a C++ library, a C ABI, and Rust bindings for interacting with ContactGlove devices.
 * 
 * # Setup and integration
 *
 * See the [SDK guide](https://docs.melt-interface-technologies.com/en/docs/contact/sdks/contact-glove-v2-native)
 * for supported operating systems, CPU architectures, runtime and build requirements,
 * distribution installation, CMake/C/Rust integration, and troubleshooting.
 *
 * # Examples
 * 
 * ## Connecting to and disconnecting from DivingStation
 * Use `ConnectToHost` to connect to DivingStation.
 * 
 * The connection information passed to `ConnectToHost` consists of a host name, IP address,
 * DivingStation receive port, and SDK receive port. The host name is an arbitrary label used to
 * distinguish hosts. The IP address is assigned to the computer running DivingStation. The
 * DivingStation receive port accepts OSC requests from the SDK. It is normally 25791, but it can
 * be changed in DivingStation; use the corresponding value when it is changed. The SDK receive
 * port accepts data streams from DivingStation.
 * 
 * Use `Disconnect` to disconnect from DivingStation.
 * 
 * @verbatim
```cpp
#include <chrono>
#include <iostream>
#include <thread>

#include "contactglove/ContactGloveSDKv2.hpp"

int main() {
    ContactGloveSDKv2::SDK sdk;

    // Set the connection information.
    ContactGloveSDKv2::Host host{"localhost", "127.0.0.1", 25791, 25788};

    // Connect to DivingStation.
    sdk.ConnectToHost(host);

    // Perform application-specific work.

    // Disconnect from DivingStation.
    sdk.Disconnect(host);

    return(0);
}
```
@endverbatim
 * 
 * ## Querying data
 * 
 * The following functions retrieve information associated with a glove. Each returns the value
 * available when the function is called.
 * 
 * | Available value | Function |
 * | --------------- | -------- |
 * | Glove information | `GetDataForGlove` |
 * | Whether a glove supports haptics | `DoesGloveSupportHaptics` |
 * | Opening and bending angles for each finger joint | `GetErgonomicsDataForGlove` |
 * | Position and rotation of each finger joint | `GetSkeletonDataForGlove` |
 * | Raw sensor position and rotation | `GetRawSensorTransformsForGlove` |
 * | Input from the controller attached to the glove | `GetControllerDataForGlove` |
 * | IDs of all connected gloves | `GetIdsOfAvailableGloves` |
 * | Number of connected gloves | `GetNumberofAvailableGloves` |
 * 
 * Glove data arrives asynchronously. A successful `ConnectToHost` call starts the
 * local session; it does not confirm approval or data reception. Use a glove ID
 * received from DivingStation when querying data. When multiple gloves are present,
 * select the intended device using its ID or properties such as `isLeft`.
 *
 * The bundled `share/ContactGloveSDKv2/examples/basic_usage.cpp` demonstrates
 * callbacks and polling with DivingStation on the same PC (`127.0.0.1`). It polls
 * the first ID received and disconnects after approximately five seconds.
 *
 * For LAN connections that require approval, keep the same session alive while
 * waiting for approval and data. Calling `Disconnect` can discard pending or
 * granted approval; a successful connection request does not confirm approval.
 *
 * ## Receiving updates automatically
 * 
 * Register callback functions to receive automatic updates when glove-related values change.
 * Callbacks run on the SDK receiver thread and may query cached data. An exception thrown by
 * one callback is contained; the remaining callbacks and records are still delivered.
 * Do not connect, disconnect, or destroy this SDK from a callback. Schedule connection
 * management on another thread instead. Disconnect waits for active callbacks to finish.
 * Finish callbacks that access the SDK before starting its destruction.
 * 
 * | Update that triggers the callback | Function |
 * | --------------------------------- | -------- |
 * | Glove information | `RegisterCallbackForGloveDataStream` |
 * | Opening and bending angles for each finger joint | `RegisterCallbackForErgonomicsStream` |
 * | Position and rotation of each finger joint | `RegisterCallbackForSkeletonStream` |
 * | Raw sensor position and rotation | `RegisterCallbackForRawSensorTransformsStream` |
 * | Input from the controller attached to the glove | `RegisterCallbackForControllerDataStream` |
 * 
 * See the bundled `share/ContactGloveSDKv2/examples/basic_usage.cpp` for a
 * complete callback and polling example and its five-second lifetime.
 *
 *
 * # C ABI and Rust bindings
 *
 * `include/contactglove/ContactGloveSDKv2C.h` is the ContactGlove C++/C/Rust SDK V2 FFI header.
 * It can be used directly from C with no additional header. The C ABI v2 exposes opaque handles,
 * fixed-width numeric types, UTF-8 string views, caller-provided buffers, and subscription tokens.
 *
 * Although most C ABI data structures correspond to the C++ types documented here, their lifetime
 * and memory contracts differ. For polling results that include strings, the caller provides a
 * buffer in `cg_string_buffer_t`. Even when `CG_RESULT_BUFFER_TOO_SMALL` is returned, `length`
 * receives the required character count. Allocate sufficient storage and retry the same function.
 * Memory returned by the SDK does not need to be freed.
 *
 * Manage stream subscriptions with `cg_sdk_subscribe_*` and `cg_sdk_unsubscribe`. A callback view is
 * valid only during the callback invocation. After `cg_sdk_unsubscribe` succeeds, its callback is
 * not running. Disconnecting, destroying the SDK, or unsubscribing from within a callback is unsupported.
 *
 * The v2 updates expose device field `is_external_power_present` and the invalid value
 * `battery_percentage=-1`, `tracking_state` for ergonomics and skeleton data,
 * `profile_uuid` for retargeted skeleton data, `HandSkeletonRaw`, and retarget profile
 * information request and response messages. The Rust bindings expose the same query and callback
 * surface. The numeric C ABI and Rust `color` fields use the DeviceColor values
 * for the device display-color setting. Their numeric `tracking_state` fields use
 * TrackingState values and the same display/diagnostic-only restriction for
 * IkFailed and NotAttempted.
 * While connected, the SDK resends Connect every 30 seconds and reuses the same send socket
 * for Connect, Disconnect, and haptics requests.
 *
 * The official Rust workspace is included in `rust/` in distributions.
 * `contactglove-sdk-v2-sys` provides the C ABI layer, while `contactglove-sdk-v2` provides owned
 * types and RAII subscriptions.
 *
 * # Defined types and functions
 * 
 * The following types and functions are defined by this SDK.
 * 
 */
namespace ContactGloveSDKv2 {

/// @brief Integer type representing Unix time in milliseconds.
using Timestamp = uint64_t;


/// @brief Identifier of a glove device.
using GloveId = std::string;

/// @brief Number of ergonomics data elements per hand (five fingers times four joints).
constexpr uint32_t ERGONOMICS_DATA_COUNT = 20;

/// @brief Number of skeleton nodes per hand (wrist, four thumb nodes, and five nodes for each of four fingers).
constexpr uint32_t SKELETON_NODE_COUNT = 25;

/// @brief Number of skeleton transforms per hand.
constexpr uint32_t SKELETON_DATA_COUNT = SKELETON_NODE_COUNT;

/// @brief Number of raw sensor transforms per hand.
constexpr uint32_t RAW_SENSOR_TRANSFORM_COUNT = 5;

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/**
 * @brief Enumeration of device families.
 * 
 * ContactGlove3 is the primary target.
 */
enum class DeviceFamilyType : uint32_t {
    ContactGlove  = 0, ///< ContactGlove
    ContactSheet  = 1, ///< ContactSheet
    ContactGlove2 = 2, ///< ContactGlove2
    ContactGlove3 = 3, ///< ContactGlove3
};

/**
 * @brief Enumeration of device display-color settings.
 *
 * **Migrating the display-color type name**
 *
 * DeviceColor is the public display-color type. Replace references such as
 * `LedColor::Green` with `DeviceColor::Green` and rebuild.
 * `LedColor` remains a C++ compatibility alias of DeviceColor, so existing source
 * still compiles. Prefer DeviceColor in new code.
 * The `GloveData.color` member name and numeric color values stay the same.
 * The C ABI version remains 2. The C ABI and Rust integer `color` fields and the
 * OSC wire format are unchanged.
 *
 */
enum class DeviceColor : uint32_t {
    Magenta    = 0, ///< Magenta
    Red        = 1, ///< Red
    Orange     = 2, ///< Orange
    Yellow     = 3, ///< Yellow
    Green      = 4, ///< Green
    Cyan       = 5, ///< Cyan
    Blue       = 6, ///< Blue
    Achromatic = 7, ///< Achromatic
};

/** @brief Compatibility alias for DeviceColor. Prefer DeviceColor in new code. */
using LedColor = DeviceColor;

/**
 * @brief Enumeration of device connection states.
 */
enum class DeviceConnectedState : uint32_t {
    Unknown     = 0, ///< State unknown
    Connected   = 1, ///< Connected
    Unconnected = 2, ///< Not connected
    Pairing     = 3, ///< Connection attempt in progress
};

/**
 * @brief Tracking state of IK-derived data.
 *
 * NotAttempted means the required IK pass was not performed. This includes earlier
 * processing failures or missing input, and cases where pose calculation was deemed
 * unnecessary, such as update-interval control reusing an existing pose. It does not
 * itself indicate an IK failure.
 *
 * IkFailed and NotAttempted values may be used for display and diagnostics, but must
 * not be used for control, velocity estimation or interpolation. Only Tracked values
 * are valid for those uses.
 *
 * SkeletonData reports retarget IK; RawSkeletonData and ErgonomicsData report source
 * skeleton IK. Their states can differ at the same time. For retarget IK,
 * NotAttempted also includes a decision that pose calculation was unnecessary.
 */
enum class TrackingState : uint32_t {
    Tracked      = 0, ///< Valid result from the current IK pass
    IkFailed     = 1, ///< IK was attempted but failed
    NotAttempted = 2, ///< IK was not performed, including intentional update-interval skips
};

/**
 * @brief Return codes used by SDK functions.
 *
 * Indicates the result of a function call or the kind of error.
 */
enum class SDKReturnCode : uint32_t {
    Success         = 0, ///< Operation completed successfully
    SdkNotAvailable = 1, ///< The SDK or requested feature is unavailable
    NotConnected    = 2, ///< Not connected to DivingStation
    InvalidID       = 3, ///< The specified ID does not exist
    InvalidArgument = 4, ///< An argument is outside its valid range
};

/**
 * @brief Enumeration of button states.
 */
enum class ButtonStatus : uint32_t {
    Unpressed = 0, ///< Not pressed
    Touching  = 1, ///< Touch sensor detected contact
    Pressed   = 2, ///< Pressed
};

/**
 * @brief Enumeration identifying the joint represented by each ergonomics data element.
 *
 * Left-hand values are 0 through 19 and right-hand values are 20 through 39, but
 * `ErgonomicsData::data` always holds only one hand (20 elements). To use a value
 * as an index into `ErgonomicsData::data`, use
 * `static_cast<uint32_t>(type) % ERGONOMICS_DATA_COUNT`. Values are in degrees.
 *
 * Naming convention: `(Left|Right){finger name}{joint name}{motion type}`.
 */
enum class ErgonomicsDataType : uint32_t
{
    LeftThumbCMCFlex = 0,    ///< Left thumb CMC joint flexion angle
    LeftThumbMCPFlex = 1,    ///< Left thumb MCP joint flexion angle
    LeftThumbIPFlex = 2,     ///< Left thumb IP joint flexion angle
    LeftThumbMCPTilt = 3,    ///< Left thumb MCP joint tilt angle
    LeftIndexMCPFlex = 4,    ///< Left index finger MCP joint flexion angle
    LeftIndexPIPFlex = 5,    ///< Left index finger PIP joint flexion angle
    LeftIndexDIPFlex = 6,    ///< Left index finger DIP joint flexion angle
    LeftIndexMCPTilt = 7,    ///< Left index finger MCP joint tilt angle
    LeftMiddleMCPFlex = 8,   ///< Left middle finger MCP joint flexion angle
    LeftMiddlePIPFlex = 9,   ///< Left middle finger PIP joint flexion angle
    LeftMiddleDIPFlex = 10,  ///< Left middle finger DIP joint flexion angle
    LeftMiddleMCPTilt = 11,  ///< Left middle finger MCP joint tilt angle
    LeftRingMCPFlex = 12,    ///< Left ring finger MCP joint flexion angle
    LeftRingPIPFlex = 13,    ///< Left ring finger PIP joint flexion angle
    LeftRingDIPFlex = 14,    ///< Left ring finger DIP joint flexion angle
    LeftRingMCPTilt = 15,    ///< Left ring finger MCP joint tilt angle
    LeftLittleMCPFlex = 16,  ///< Left little finger MCP joint flexion angle
    LeftLittlePIPFlex = 17,  ///< Left little finger PIP joint flexion angle
    LeftLittleDIPFlex = 18,  ///< Left little finger DIP joint flexion angle
    LeftLittleMCPTilt = 19,  ///< Left little finger MCP joint tilt angle
    RightThumbCMCFlex = 20,  ///< Right thumb CMC joint flexion angle
    RightThumbMCPFlex = 21,  ///< Right thumb MCP joint flexion angle
    RightThumbIPFlex = 22,   ///< Right thumb IP joint flexion angle
    RightThumbMCPTilt = 23,  ///< Right thumb MCP joint tilt angle
    RightIndexMCPFlex = 24,  ///< Right index finger MCP joint flexion angle
    RightIndexPIPFlex = 25,  ///< Right index finger PIP joint flexion angle
    RightIndexDIPFlex = 26,  ///< Right index finger DIP joint flexion angle
    RightIndexMCPTilt = 27,  ///< Right index finger MCP joint tilt angle
    RightMiddleMCPFlex = 28, ///< Right middle finger MCP joint flexion angle
    RightMiddlePIPFlex = 29, ///< Right middle finger PIP joint flexion angle
    RightMiddleDIPFlex = 30, ///< Right middle finger DIP joint flexion angle
    RightMiddleMCPTilt = 31, ///< Right middle finger MCP joint tilt angle
    RightRingMCPFlex = 32,   ///< Right ring finger MCP joint flexion angle
    RightRingPIPFlex = 33,   ///< Right ring finger PIP joint flexion angle
    RightRingDIPFlex = 34,   ///< Right ring finger DIP joint flexion angle
    RightRingMCPTilt = 35,   ///< Right ring finger MCP joint tilt angle
    RightLittleMCPFlex = 36, ///< Right little finger MCP joint flexion angle
    RightLittlePIPFlex = 37, ///< Right little finger PIP joint flexion angle
    RightLittleDIPFlex = 38, ///< Right little finger DIP joint flexion angle
    RightLittleMCPTilt = 39, ///< Right little finger MCP joint tilt angle
};

/**
 * @brief Enumeration identifying the joint represented by each skeleton data element.
 *
 * Use this enum to index `SkeletonData::transforms`. It maps to
 * #SKELETON_NODE_COUNT elements for one hand.
 *
 * Naming convention: `Skeleton{finger}{joint name}` or `Skeleton{finger}Tip`.
 */
enum class SkeletonDataType : uint32_t
{
    SkeletonWrist = 0,      ///< Wrist
    SkeletonThumbCMC = 1,   ///< Thumb CMC joint
    SkeletonThumbMCP = 2,   ///< Thumb MCP joint
    SkeletonThumbIP = 3,    ///< Thumb IP joint
    SkeletonThumbTip = 4,   ///< Thumb tip
    SkeletonIndexCMC = 5,   ///< Index finger CMC joint
    SkeletonIndexMCP = 6,   ///< Index finger MCP joint
    SkeletonIndexPIP = 7,   ///< Index finger PIP joint
    SkeletonIndexDIP = 8,   ///< Index finger DIP joint
    SkeletonIndexTip = 9,   ///< Index finger tip
    SkeletonMiddleCMC = 10, ///< Middle finger CMC joint
    SkeletonMiddleMCP = 11, ///< Middle finger MCP joint
    SkeletonMiddlePIP = 12, ///< Middle finger PIP joint
    SkeletonMiddleDIP = 13, ///< Middle finger DIP joint
    SkeletonMiddleTip = 14, ///< Middle finger tip
    SkeletonRingCMC = 15,   ///< Ring finger CMC joint
    SkeletonRingMCP = 16,   ///< Ring finger MCP joint
    SkeletonRingPIP = 17,   ///< Ring finger PIP joint
    SkeletonRingDIP = 18,   ///< Ring finger DIP joint
    SkeletonRingTip = 19,   ///< Ring finger tip
    SkeletonLittleCMC = 20, ///< Little finger CMC joint
    SkeletonLittleMCP = 21, ///< Little finger MCP joint
    SkeletonLittlePIP = 22, ///< Little finger PIP joint
    SkeletonLittleDIP = 23, ///< Little finger DIP joint
    SkeletonLittleTip = 24, ///< Little finger tip
};

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

/**
 * @brief Structure holding glove device information.
 *
 * Corresponds to data received at OSC address `/DS2/HC/Device`.
 */
struct GloveData {
    std::string          id;                ///< Device ID
    std::string          pairId;            ///< ID of the paired glove; empty when no pair exists
    int32_t              batteryPercentage = -1; ///< Battery percentage [0 - 100]; -1 when invalid
    bool                 isHaptics         = false; ///< Whether haptics are supported
    bool                 isLeft            = false; ///< Whether this is a left-hand glove
    DeviceFamilyType     familyType        = DeviceFamilyType::ContactGlove; ///< Device family
    DeviceColor          color             = DeviceColor::Achromatic; ///< Device display-color setting
    DeviceConnectedState connectedState    = DeviceConnectedState::Unknown; ///< Connection state
    std::string          name;             ///< Device name
    bool                 isExternalPowerPresent = false; ///< Whether external power is detected
};

/**
 * @brief Structure describing a DivingStation host.
 *
 * Pass this structure to `ConnectToHost()` and `Disconnect()`. `port` is the OSC receive port
 * on DivingStation. `receivePort` is the OSC receive port on the SDK. `sendPort` is the OSC send
 * port on the SDK; when it is `0`, the operating system selects an available port.
 */
struct Host {
    std::string  hostName;            ///< Arbitrary label used to distinguish hosts, for example `"localhost"`
    std::string  ipAddress;           ///< IP address, for example `"127.0.0.1"`
    uint32_t     port        = 25791; ///< OSC receive port on DivingStation
    uint32_t     receivePort = 25788; ///< OSC receive port on the SDK
    uint32_t     sendPort    = 0;     ///< OSC send port on the SDK; 0 lets the OS select a port
};

/**
 * @brief Three-dimensional vector.
 */
struct Vec3 {
    float x = 0.0f; ///< X component
    float y = 0.0f; ///< Y component
    float z = 0.0f; ///< Z component
};

/**
 * @brief Quaternion representing rotation.
 */
struct Quaternion {
    float w = 1.0f; ///< Scalar component
    float x = 0.0f; ///< X component
    float y = 0.0f; ///< Y component
    float z = 0.0f; ///< Z component
};

/**
 * @brief Transform combining position and rotation.
 *
 * Used as each element of `SkeletonData::transforms`.
 */
struct Transform {
    Vec3       position; ///< Position
    Quaternion rotation; ///< Rotation
};

/**
 * @brief Structure holding controller input states.
 */
struct ControllerData {
    std::string  id;                                       ///< Glove ID
    bool         isLeft         = false;                   ///< Whether this is the left hand
    ButtonStatus aButton        = ButtonStatus::Unpressed; ///< A button state
    ButtonStatus bButton        = ButtonStatus::Unpressed; ///< B button state
    ButtonStatus sysButton      = ButtonStatus::Unpressed; ///< System button state
    ButtonStatus triggerButton  = ButtonStatus::Unpressed; ///< Trigger button state
    ButtonStatus joystickButton = ButtonStatus::Unpressed; ///< Joystick button state
    ButtonStatus trackpadButton = ButtonStatus::Unpressed; ///< Trackpad touch state
    float        trigger        = 0.0f; ///< Trigger value (0.0 to 1.0)
    float        gripValue      = 0.0f; ///< Grip value (0.0 to 1.0)
    float        gripForce      = 0.0f; ///< Grip force (0.0 to 1.0)
    float        joystickX      = 0.0f; ///< Joystick X-axis value (-1.0 to 1.0)
    float        joystickY      = 0.0f; ///< Joystick Y-axis value (-1.0 to 1.0)
    float        trackpadX      = 0.0f; ///< Trackpad X-axis value (-1.0 to 1.0)
    float        trackpadY      = 0.0f; ///< Trackpad Y-axis value (-1.0 to 1.0)
};

/**
 * @brief Structure holding finger opening and bending angle data.
 *
 * `data` has #ERGONOMICS_DATA_COUNT elements for one hand. Values are in degrees.
 * See #ErgonomicsDataType for the meaning of each index.
 */
struct ErgonomicsData {
    std::string id;                                  ///< Glove ID
    bool isLeft = false;                             ///< Whether this is the left hand
    Timestamp generatedAt = 0;                       ///< Frame generation time (Unix time in milliseconds)
    TrackingState trackingState = TrackingState::Tracked; ///< Tracking state of IK-derived data
    std::array<float, ERGONOMICS_DATA_COUNT> data{}; ///< Opening and bending angle values per joint (20 elements for one hand, in degrees)
};

/**
 * @brief Structure holding hand skeleton data (joint positions and rotations).
 *
 * `transforms` has #SKELETON_DATA_COUNT elements for one hand. See #SkeletonDataType
 * for the meaning of each index. Positions are in meters in a right-handed hand-model
 * coordinate system rooted at the wrist. +X points from the fingertips toward the wrist,
 * +Y from the palm toward the back of the hand, and +Z from little finger to thumb on
 * the right hand and from thumb to little finger on the left hand. Rotations are local
 * quaternions relative to the parent joint; wrist and fingertip rotations are identity
 * placeholders, not measured rotations. These conventions apply to skeleton transforms;
 * raw sensor rotations instead represent orientations in hand-model space.
 */
struct SkeletonData {
    std::string id;                                          ///< Glove ID
    std::string profileUuid;                                  ///< Stable UUID of the retarget profile
    bool isLeft = false;                                     ///< Whether this is the left hand
    Timestamp generatedAt = 0;                               ///< Frame generation time (Unix time in milliseconds)
    TrackingState trackingState = TrackingState::Tracked;     ///< Tracking state of retarget IK
    std::array<Transform, SKELETON_DATA_COUNT> transforms{}; ///< Position and rotation per node (25 elements)
};

/** @brief Hand skeleton data before retargeting. */
struct RawSkeletonData {
    std::string id;
    bool isLeft = false;
    Timestamp generatedAt = 0;
    TrackingState trackingState = TrackingState::Tracked;
    std::array<Transform, SKELETON_DATA_COUNT> transforms{};
};

/** @brief Response containing retarget profile information. */
struct RetargetProfileInfo {
    int32_t requestId = 0;
    std::string profileUuid;
    int32_t status = 0;
    std::string name;
    std::string description;
};

/**
 * @brief Structure holding raw sensor transform data for one hand.
 *
 * Corresponds to data received at OSC address `/DS2/HC/RawSensorTransforms`. Each transform is
 * expressed in the same right-handed model-rooted space as `/DS2/HC/HandSkeleton`. Its position
 * is the sensor center relative to the `wrist`, and its rotation is the orientation from the
 * sensor local frame to model-rooted space. `transforms` contains #RAW_SENSOR_TRANSFORM_COUNT
 * elements in thumb, index, middle, ring, and little finger order.
 */
struct RawSensorTransformsData {
    std::string id;                                                   ///< Glove ID
    bool isLeft = false;                                              ///< Whether this is the left hand
    Timestamp generatedAt = 0;                                        ///< Frame generation time (Unix time in milliseconds)
    std::array<Transform, RAW_SENSOR_TRANSFORM_COUNT> transforms{};   ///< Position and rotation per sensor (five elements for one hand)
};

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

/// @brief Callback function type invoked when glove data is updated.
using GloveDataStreamCallback_t      = std::function<void(const GloveData&)>;

/// @brief Callback function type invoked when controller data is updated.
using ControllerDataStreamCallback_t = std::function<void(const ControllerData&)>;

/// @brief Callback function type invoked when ergonomics data is updated.
using ErgonomicsStreamCallback_t     = std::function<void(const ErgonomicsData&)>;

/// @brief Callback function type invoked when skeleton data is updated.
using SkeletonStreamCallback_t       = std::function<void(const SkeletonData&)>;
using RawSkeletonStreamCallback_t    = std::function<void(const RawSkeletonData&)>;

/// @brief Callback function type invoked when raw sensor transform data is updated.
using RawSensorTransformsStreamCallback_t = std::function<void(const RawSensorTransformsData&)>;
using RetargetProfileInfoCallback_t  = std::function<void(const RetargetProfileInfo&)>;

// ---------------------------------------------------------------------------
// SDK class
// ---------------------------------------------------------------------------

/**
 * @brief Main class of the ContactGlove C++/C/Rust SDK V2.
 *
 **/
class CONTACTGLOVE_API SDK {
public:
    SDK();
    ~SDK();

    SDK(const SDK&)            = delete;
    SDK& operator=(const SDK&) = delete;

    // -----------------------------------------------------------------------
    // Connection
    // -----------------------------------------------------------------------

    /**
     * @brief Connect to DivingStation and start data streams.
     *
     * Calling again with the same endpoint and ports resends Connect
     * using the existing socket, including its OS-selected source port when sendPort is 0.
     * Use this for automatic retries while awaiting LAN approval or recovering from lost data.
     * Success means the request was started; it does not confirm LAN approval.
     * Disconnect explicitly ends the session and can revoke approval.
     *
     * @param p_Host Host information for the target DivingStation instance.
     * @return #SDKReturnCode::Success — Connection started successfully.
     * @return #SDKReturnCode::SdkNotAvailable — Failed to open a socket.
     * @return #SDKReturnCode::InvalidArgument — `p_Host.sendPort` is greater than 65535.
     * @return #SDKReturnCode::InvalidArgument — The receive port is already in use by
     *         a connection with a different IP address, destination port, or send port.
     */
    SDKReturnCode ConnectToHost(const Host& p_Host);

    /**
     * @brief Disconnect from DivingStation.
     *
     * @param p_Host Host information to disconnect. It must match the value passed to `ConnectToHost()`.
     * @return SDKReturnCode::Success — Disconnected successfully.
     * @return SDKReturnCode::NotConnected — No connection exists for the specified port.
     */
    SDKReturnCode Disconnect(const Host& p_Host);

    // -----------------------------------------------------------------------
    // Glove data queries
    // -----------------------------------------------------------------------

    /**
     * @brief Query whether a glove supports haptics.
     *
     * @param id ID of the glove to query.
     * @param p_IsHaptics Output location for whether haptics are supported; must not be null.
     * @return SDKReturnCode::Success — Value retrieved successfully.
     * @return SDKReturnCode::InvalidID — The specified glove does not exist.
     */
    SDKReturnCode DoesGloveSupportHaptics(const std::string& id, bool* p_IsHaptics = nullptr) const;

    /**
     * @brief Retrieve glove information by glove ID.
     *
     * @param id ID of the glove to retrieve.
     * @param p_GloveData Output location for the retrieved data; must not be null.
     * @return #SDKReturnCode::Success — Data retrieved successfully.
     * @return #SDKReturnCode::InvalidID — The specified glove does not exist.
     */
    SDKReturnCode GetDataForGlove(const std::string& id, GloveData* p_GloveData) const;

    /**
     * @brief Retrieve controller input data by glove ID.
     *
     * @param id ID of the glove to retrieve.
     * @param p_Controller Output location for the retrieved data; must not be null.
     * @return #SDKReturnCode::Success — Data retrieved successfully.
     * @return #SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_Controller` is null.
     */
    SDKReturnCode GetControllerDataForGlove(const std::string& id, ControllerData* p_Controller) const;

    /**
     * @brief Retrieve finger opening and bending angle data by glove ID.
     *
     * @param id ID of the glove to retrieve.
     * @param p_ErgonomicsData Output location for the retrieved data; must not be null.
     * @return #SDKReturnCode::Success — Data retrieved successfully.
     * @return #SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_ErgonomicsData` is null.
     */
    SDKReturnCode GetErgonomicsDataForGlove(const std::string& id, ErgonomicsData* p_ErgonomicsData) const;

    /**
     * @brief Retrieve joint position and rotation data by glove ID.
     *
     * @param id ID of the glove to retrieve.
     * @param p_SkeletonData Output location for the retrieved data; must not be null.
     * @return #SDKReturnCode::Success — Data retrieved successfully.
     * @return #SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_SkeletonData` is null.
     */
    SDKReturnCode GetSkeletonDataForGlove(const std::string& id, SkeletonData* p_SkeletonData) const;

    /** @brief Retrieve pre-retarget hand skeleton data by glove ID. */
    SDKReturnCode GetRawSkeletonDataForGlove(const std::string& id,
                                             RawSkeletonData* p_RawSkeletonData) const;

    /**
     * @brief Retrieve raw sensor transform data by glove ID.
     *
     * @param id ID of the glove to retrieve.
     * @param p_RawSensorTransforms Output location for the retrieved data; must not be null.
     * @return #SDKReturnCode::Success — Data retrieved successfully.
     * @return #SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_RawSensorTransforms` is null.
     */
    SDKReturnCode GetRawSensorTransformsForGlove(const std::string& id,
                                                 RawSensorTransformsData* p_RawSensorTransforms) const;

    /**
     * @brief Retrieve IDs of all currently recognized gloves.
     *
     * When the output array is too small, only the first `p_NumberOfIdsThatFitInArray` IDs are
     * written. Query the total count in advance with `GetNumberofAvailableGloves()`.
     *
     * @param p_IdsOfAvailableGloves Pointer to the first element of the output ID array; must not be null.
     * @param p_NumberOfIdsThatFitInArray Capacity of the output array in elements.
     * @return #SDKReturnCode::Success — Data retrieved successfully.
     * @return #SDKReturnCode::InvalidID — `p_IdsOfAvailableGloves` is null.
     */
    SDKReturnCode GetIdsOfAvailableGloves(GloveId* p_IdsOfAvailableGloves, uint32_t p_NumberOfIdsThatFitInArray) const;

    /**
     * @brief Retrieve the total number of currently recognized gloves.
     *
     * @param n Output location for the count; must not be null.
     * @return #SDKReturnCode::Success — Data retrieved successfully.
     * @return #SDKReturnCode::InvalidID — `n` is null.
     */
    SDKReturnCode GetNumberofAvailableGloves(uint32_t* n) const;

    // -----------------------------------------------------------------------
    // Haptics
    // -----------------------------------------------------------------------

    /**
     * @brief Vibrate a glove.
     *
     * @param id ID of the glove to vibrate.
     * @param frequency Vibration frequency in Hz (100.0 to 500.0).
     * @param amplitude Vibration amplitude (0.0 to 1.0).
     * @param duration Vibration duration in seconds (0.001 to 1.0).
     * @return #SDKReturnCode::Success — Request sent successfully.
     * @return #SDKReturnCode::InvalidID — The specified glove does not exist.
     * @return #SDKReturnCode::InvalidArgument — An argument is outside its valid range.
     */
    SDKReturnCode VibrateGlove(const std::string& id, float frequency, float amplitude, float duration);

    /** @brief Send a request for retarget profile information. */
    SDKReturnCode RequestRetargetProfileInfo(const std::string& profileUuid, int32_t requestId);

    // -----------------------------------------------------------------------
    // Stream callbacks
    // -----------------------------------------------------------------------

    /**
     * @brief Register a callback invoked when glove information is updated.
     *
     * The callback receives a `GloveData` argument.
     *
     * @param p_Callback Callback function to register.
     * @return #SDKReturnCode::Success — Registered successfully.
     */
    SDKReturnCode RegisterCallbackForGloveDataStream(GloveDataStreamCallback_t p_Callback);

    /**
     * @brief Register a callback invoked when controller data is updated.
     *
     * @param p_Callback Callback function to register.
     * @return #SDKReturnCode::Success — Registered successfully.
     */
    SDKReturnCode RegisterCallbackForControllerDataStream(ControllerDataStreamCallback_t p_Callback);

    /**
     * @brief Register a callback invoked when ergonomics data is updated.
     *
     * @param p_Callback Callback function to register.
     * @return #SDKReturnCode::Success — Registered successfully.
     */
    SDKReturnCode RegisterCallbackForErgonomicsStream(ErgonomicsStreamCallback_t p_Callback);

    /**
     * @brief Register a callback invoked when skeleton data is updated.
     *
     * @param p_Callback Callback function to register.
     * @return #SDKReturnCode::Success — Registered successfully.
     */
    SDKReturnCode RegisterCallbackForSkeletonStream(SkeletonStreamCallback_t p_Callback);

    SDKReturnCode RegisterCallbackForRawSkeletonStream(RawSkeletonStreamCallback_t p_Callback);

    /**
     * @brief Register a callback invoked when raw sensor transform data is updated.
     *
     * @param p_Callback Callback function to register.
     * @return #SDKReturnCode::Success — Registered successfully.
     */
    SDKReturnCode RegisterCallbackForRawSensorTransformsStream(RawSensorTransformsStreamCallback_t p_Callback);

    SDKReturnCode RegisterCallbackForRetargetProfileInfoStream(RetargetProfileInfoCallback_t p_Callback);

private:
    /// @cond INTERNAL
    class Impl;
    std::unique_ptr<Impl> impl_;
    /// @endcond
};

} // namespace ContactGloveSDKv2
