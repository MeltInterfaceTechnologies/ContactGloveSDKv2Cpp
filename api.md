

# ContactGloveSDKv2

ContactGlove C++/C/Rust SDK V2 provides a C++ library, a C ABI, and Rust bindings for interacting with ContactGlove devices.

## Setup and integration
See the [SDK guide](https://docs.melt-interface-technologies.com/en/docs/contact/sdks/contact-glove-v2-native) for supported operating systems, CPU architectures, runtime and build requirements, distribution installation, CMake/C/Rust integration, and troubleshooting.

## Examples

### Connecting to and disconnecting from DivingStation
Use `ConnectToHost` to connect to DivingStation.

The connection information passed to `ConnectToHost` consists of a host name, IP address, DivingStation receive port, and SDK receive port. The host name is an arbitrary label used to distinguish hosts. The IP address is assigned to the computer running DivingStation. The DivingStation receive port accepts OSC requests from the SDK. It is normally 25791, but it can be changed in DivingStation; use the corresponding value when it is changed. The SDK receive port accepts data streams from DivingStation.

Use `Disconnect` to disconnect from DivingStation.

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

### Querying data
The following functions retrieve information associated with a glove. Each returns the value available when the function is called.

Available value  |Function
--------- | ---------
Glove information  |`GetDataForGlove`
Whether a glove supports haptics  |`DoesGloveSupportHaptics`
Opening and bending angles for each finger joint  |`GetErgonomicsDataForGlove`
Position and rotation of each finger joint  |`GetSkeletonDataForGlove`
Raw sensor position and rotation  |`GetRawSensorTransformsForGlove`
Input from the controller attached to the glove  |`GetControllerDataForGlove`
IDs of all connected gloves  |`GetIdsOfAvailableGloves`
Number of connected gloves  |`GetNumberofAvailableGloves`

Glove data arrives asynchronously. A successful `ConnectToHost` call starts the local session; it does not confirm approval or data reception. Use a glove ID received from DivingStation when querying data. When multiple gloves are present, select the intended device using its ID or properties such as `isLeft`.

The bundled `share/ContactGloveSDKv2/examples/basic_usage.cpp` demonstrates callbacks and polling with DivingStation on the same PC (`127.0.0.1`). It polls the first ID received and disconnects after approximately five seconds.

For LAN connections that require approval, keep the same session alive while waiting for approval and data. Calling `Disconnect` can discard pending or granted approval; a successful connection request does not confirm approval.

### Receiving updates automatically
Register callback functions to receive automatic updates when glove-related values change. Callbacks run on the SDK receiver thread and may query cached data. An exception thrown by one callback is contained; the remaining callbacks and records are still delivered. Do not connect, disconnect, or destroy this SDK from a callback. Schedule connection management on another thread instead. Disconnect waits for active callbacks to finish. Finish callbacks that access the SDK before starting its destruction.

Update that triggers the callback  |Function
--------- | ---------
Glove information  |`RegisterCallbackForGloveDataStream`
Opening and bending angles for each finger joint  |`RegisterCallbackForErgonomicsStream`
Position and rotation of each finger joint  |`RegisterCallbackForSkeletonStream`
Raw sensor position and rotation  |`RegisterCallbackForRawSensorTransformsStream`
Input from the controller attached to the glove  |`RegisterCallbackForControllerDataStream`

See the bundled `share/ContactGloveSDKv2/examples/basic_usage.cpp` for a complete callback and polling example and its five-second lifetime.

## C ABI and Rust bindings
`include/contactglove/ContactGloveSDKv2C.h` is the ContactGlove C++/C/Rust SDK V2 FFI header. It can be used directly from C with no additional header. The C ABI v2 exposes opaque handles, fixed-width numeric types, UTF-8 string views, caller-provided buffers, and subscription tokens.

Although most C ABI data structures correspond to the C++ types documented here, their lifetime and memory contracts differ. For polling results that include strings, the caller provides a buffer in `cg_string_buffer_t`. Even when `CG_RESULT_BUFFER_TOO_SMALL` is returned, `length` receives the required character count. Allocate sufficient storage and retry the same function. Memory returned by the SDK does not need to be freed.

Manage stream subscriptions with `cg_sdk_subscribe_*` and `cg_sdk_unsubscribe`. A callback view is valid only during the callback invocation. After `cg_sdk_unsubscribe` succeeds, its callback is not running. Disconnecting, destroying the SDK, or unsubscribing from within a callback is unsupported.

The v2 updates expose device field `is_external_power_present` and the invalid value `battery_percentage=-1`, `tracking_state` for ergonomics and skeleton data, `profile_uuid` for retargeted skeleton data, `HandSkeletonRaw`, and retarget profile information request and response messages. The Rust bindings expose the same query and callback surface. The numeric C ABI and Rust `color` fields use the DeviceColor values for the device display-color setting. Their numeric `tracking_state` fields use TrackingState values and the same display/diagnostic-only restriction for IkFailed and NotAttempted. While connected, the SDK resends Connect every 30 seconds and reuses the same send socket for Connect, Disconnect, and haptics requests.

The official Rust workspace is included in `rust/` in distributions. `contactglove-sdk-v2-sys` provides the C ABI layer, while `contactglove-sdk-v2` provides owned types and RAII subscriptions.

## Defined types and functions
The following types and functions are defined by this SDK.

### Classes

| Name | Description |
|------|-------------|
| [`SDK`](#sdk) | Main class of the ContactGlove C++/C/Rust SDK V2. |
| [`GloveData`](#glovedata) | Structure holding glove device information. |
| [`Host`](#host) | Structure describing a DivingStation host. |
| [`Vec3`](#vec3) | Three-dimensional vector. |
| [`Quaternion`](#quaternion) | Quaternion representing rotation. |
| [`Transform`](#transform) | Transform combining position and rotation. |
| [`ControllerData`](#controllerdata) | Structure holding controller input states. |
| [`ErgonomicsData`](#ergonomicsdata) | Structure holding finger opening and bending angle data. |
| [`SkeletonData`](#skeletondata) | Structure holding hand skeleton data (joint positions and rotations). |
| [`RawSkeletonData`](#rawskeletondata) | Hand skeleton data before retargeting. |
| [`RetargetProfileInfo`](#retargetprofileinfo) | Response containing retarget profile information. |
| [`RawSensorTransformsData`](#rawsensortransformsdata) | Structure holding raw sensor transform data for one hand. |

### Enumerations

| Name | Description |
|------|-------------|
| [`DeviceFamilyType`](#devicefamilytype)  | Enumeration of device families. |
| [`DeviceColor`](#devicecolor)  | Enumeration of device display-color settings. |
| [`DeviceConnectedState`](#deviceconnectedstate)  | Enumeration of device connection states. |
| [`TrackingState`](#trackingstate)  | Tracking state of IK-derived data. |
| [`SDKReturnCode`](#sdkreturncode)  | Return codes used by SDK functions. |
| [`ButtonStatus`](#buttonstatus)  | Enumeration of button states. |
| [`ErgonomicsDataType`](#ergonomicsdatatype)  | Enumeration identifying the joint represented by each ergonomics data element. |
| [`SkeletonDataType`](#skeletondatatype)  | Enumeration identifying the joint represented by each skeleton data element. |

---

#### DeviceFamilyType

```cpp
enum DeviceFamilyType
```

Enumeration of device families.

ContactGlove3 is the primary target.

| Value | Description |
|-------|-------------|
| `ContactGlove` | ContactGlove. |
| `ContactSheet` | ContactSheet. |
| `ContactGlove2` | ContactGlove2. |
| `ContactGlove3` | ContactGlove3. |

---

#### DeviceColor

```cpp
enum DeviceColor
```

Enumeration of device display-color settings.

**Migrating the display-color type name**

DeviceColor is the public display-color type. Replace references such as `LedColor::Green` with `DeviceColor::Green` and rebuild. `LedColor` remains a C++ compatibility alias of DeviceColor, so existing source still compiles. Prefer DeviceColor in new code. The `GloveData.color` member name and numeric color values stay the same. The C ABI version remains 2. The C ABI and Rust integer `color` fields and the OSC wire format are unchanged.

| Value | Description |
|-------|-------------|
| `Magenta` | Magenta. |
| `Red` | Red. |
| `Orange` | Orange. |
| `Yellow` | Yellow. |
| `Green` | Green. |
| `Cyan` | Cyan. |
| `Blue` | Blue. |
| `Achromatic` | Achromatic. |

---

#### DeviceConnectedState

```cpp
enum DeviceConnectedState
```

Enumeration of device connection states.

| Value | Description |
|-------|-------------|
| `Unknown` | State unknown. |
| `Connected` | Connected. |
| `Unconnected` | Not connected. |
| `Pairing` | Connection attempt in progress. |

---

#### TrackingState

```cpp
enum TrackingState
```

Tracking state of IK-derived data.

NotAttempted means the required IK pass was not performed. This includes earlier processing failures or missing input, and cases where pose calculation was deemed unnecessary, such as update-interval control reusing an existing pose. It does not itself indicate an IK failure.

IkFailed and NotAttempted values may be used for display and diagnostics, but must not be used for control, velocity estimation or interpolation. Only Tracked values are valid for those uses.

SkeletonData reports retarget IK; RawSkeletonData and ErgonomicsData report source skeleton IK. Their states can differ at the same time. For retarget IK, NotAttempted also includes a decision that pose calculation was unnecessary.

| Value | Description |
|-------|-------------|
| `Tracked` | Valid result from the current IK pass. |
| `IkFailed` | IK was attempted but failed. |
| `NotAttempted` | IK was not performed, including intentional update-interval skips. |

---

#### SDKReturnCode

```cpp
enum SDKReturnCode
```

Return codes used by SDK functions.

Indicates the result of a function call or the kind of error.

| Value | Description |
|-------|-------------|
| `Success` | Operation completed successfully. |
| `SdkNotAvailable` | The SDK or requested feature is unavailable. |
| `NotConnected` | Not connected to DivingStation. |
| `InvalidID` | The specified ID does not exist. |
| `InvalidArgument` | An argument is outside its valid range. |

---

#### ButtonStatus

```cpp
enum ButtonStatus
```

Enumeration of button states.

| Value | Description |
|-------|-------------|
| `Unpressed` | Not pressed. |
| `Touching` | Touch sensor detected contact. |
| `Pressed` | Pressed. |

---

#### ErgonomicsDataType

```cpp
enum ErgonomicsDataType
```

Enumeration identifying the joint represented by each ergonomics data element.

Left-hand values are 0 through 19 and right-hand values are 20 through 39, but `ErgonomicsData::data` always holds only one hand (20 elements). To use a value as an index into `ErgonomicsData::data`, use `static_cast<uint32_t>(type) % ERGONOMICS_DATA_COUNT`. Values are in degrees.

Naming convention: `(Left|Right){finger name}{joint name}{motion type}`.

| Value | Description |
|-------|-------------|
| `LeftThumbCMCFlex` | Left thumb CMC joint flexion angle. |
| `LeftThumbMCPFlex` | Left thumb MCP joint flexion angle. |
| `LeftThumbIPFlex` | Left thumb IP joint flexion angle. |
| `LeftThumbMCPTilt` | Left thumb MCP joint tilt angle. |
| `LeftIndexMCPFlex` | Left index finger MCP joint flexion angle. |
| `LeftIndexPIPFlex` | Left index finger PIP joint flexion angle. |
| `LeftIndexDIPFlex` | Left index finger DIP joint flexion angle. |
| `LeftIndexMCPTilt` | Left index finger MCP joint tilt angle. |
| `LeftMiddleMCPFlex` | Left middle finger MCP joint flexion angle. |
| `LeftMiddlePIPFlex` | Left middle finger PIP joint flexion angle. |
| `LeftMiddleDIPFlex` | Left middle finger DIP joint flexion angle. |
| `LeftMiddleMCPTilt` | Left middle finger MCP joint tilt angle. |
| `LeftRingMCPFlex` | Left ring finger MCP joint flexion angle. |
| `LeftRingPIPFlex` | Left ring finger PIP joint flexion angle. |
| `LeftRingDIPFlex` | Left ring finger DIP joint flexion angle. |
| `LeftRingMCPTilt` | Left ring finger MCP joint tilt angle. |
| `LeftLittleMCPFlex` | Left little finger MCP joint flexion angle. |
| `LeftLittlePIPFlex` | Left little finger PIP joint flexion angle. |
| `LeftLittleDIPFlex` | Left little finger DIP joint flexion angle. |
| `LeftLittleMCPTilt` | Left little finger MCP joint tilt angle. |
| `RightThumbCMCFlex` | Right thumb CMC joint flexion angle. |
| `RightThumbMCPFlex` | Right thumb MCP joint flexion angle. |
| `RightThumbIPFlex` | Right thumb IP joint flexion angle. |
| `RightThumbMCPTilt` | Right thumb MCP joint tilt angle. |
| `RightIndexMCPFlex` | Right index finger MCP joint flexion angle. |
| `RightIndexPIPFlex` | Right index finger PIP joint flexion angle. |
| `RightIndexDIPFlex` | Right index finger DIP joint flexion angle. |
| `RightIndexMCPTilt` | Right index finger MCP joint tilt angle. |
| `RightMiddleMCPFlex` | Right middle finger MCP joint flexion angle. |
| `RightMiddlePIPFlex` | Right middle finger PIP joint flexion angle. |
| `RightMiddleDIPFlex` | Right middle finger DIP joint flexion angle. |
| `RightMiddleMCPTilt` | Right middle finger MCP joint tilt angle. |
| `RightRingMCPFlex` | Right ring finger MCP joint flexion angle. |
| `RightRingPIPFlex` | Right ring finger PIP joint flexion angle. |
| `RightRingDIPFlex` | Right ring finger DIP joint flexion angle. |
| `RightRingMCPTilt` | Right ring finger MCP joint tilt angle. |
| `RightLittleMCPFlex` | Right little finger MCP joint flexion angle. |
| `RightLittlePIPFlex` | Right little finger PIP joint flexion angle. |
| `RightLittleDIPFlex` | Right little finger DIP joint flexion angle. |
| `RightLittleMCPTilt` | Right little finger MCP joint tilt angle. |

---

#### SkeletonDataType

```cpp
enum SkeletonDataType
```

Enumeration identifying the joint represented by each skeleton data element.

Use this enum to index `SkeletonData::transforms`. It maps to SKELETON_NODE_COUNT elements for one hand.

Naming convention: `Skeleton{finger}{joint name}` or `Skeleton{finger}Tip`.

| Value | Description |
|-------|-------------|
| `SkeletonWrist` | Wrist. |
| `SkeletonThumbCMC` | Thumb CMC joint. |
| `SkeletonThumbMCP` | Thumb MCP joint. |
| `SkeletonThumbIP` | Thumb IP joint. |
| `SkeletonThumbTip` | Thumb tip. |
| `SkeletonIndexCMC` | Index finger CMC joint. |
| `SkeletonIndexMCP` | Index finger MCP joint. |
| `SkeletonIndexPIP` | Index finger PIP joint. |
| `SkeletonIndexDIP` | Index finger DIP joint. |
| `SkeletonIndexTip` | Index finger tip. |
| `SkeletonMiddleCMC` | Middle finger CMC joint. |
| `SkeletonMiddleMCP` | Middle finger MCP joint. |
| `SkeletonMiddlePIP` | Middle finger PIP joint. |
| `SkeletonMiddleDIP` | Middle finger DIP joint. |
| `SkeletonMiddleTip` | Middle finger tip. |
| `SkeletonRingCMC` | Ring finger CMC joint. |
| `SkeletonRingMCP` | Ring finger MCP joint. |
| `SkeletonRingPIP` | Ring finger PIP joint. |
| `SkeletonRingDIP` | Ring finger DIP joint. |
| `SkeletonRingTip` | Ring finger tip. |
| `SkeletonLittleCMC` | Little finger CMC joint. |
| `SkeletonLittleMCP` | Little finger MCP joint. |
| `SkeletonLittlePIP` | Little finger PIP joint. |
| `SkeletonLittleDIP` | Little finger DIP joint. |
| `SkeletonLittleTip` | Little finger tip. |

### Typedefs

| Return | Name | Description |
|--------|------|-------------|
| `uint64_t` | [`Timestamp`](#timestamp)  | Integer type representing Unix time in milliseconds. |
| `std::string` | [`GloveId`](#gloveid)  | Identifier of a glove device. |
| `DeviceColor` | [`LedColor`](#ledcolor)  | Compatibility alias for DeviceColor. Prefer DeviceColor in new code. |
| `std::function< void(const GloveData &)>` | [`GloveDataStreamCallback_t`](#glovedatastreamcallback_t)  | Callback function type invoked when glove data is updated. |
| `std::function< void(const ControllerData &)>` | [`ControllerDataStreamCallback_t`](#controllerdatastreamcallback_t)  | Callback function type invoked when controller data is updated. |
| `std::function< void(const ErgonomicsData &)>` | [`ErgonomicsStreamCallback_t`](#ergonomicsstreamcallback_t)  | Callback function type invoked when ergonomics data is updated. |
| `std::function< void(const SkeletonData &)>` | [`SkeletonStreamCallback_t`](#skeletonstreamcallback_t)  | Callback function type invoked when skeleton data is updated. |
| `std::function< void(const RawSkeletonData &)>` | [`RawSkeletonStreamCallback_t`](#rawskeletonstreamcallback_t)  |  |
| `std::function< void(const RawSensorTransformsData &)>` | [`RawSensorTransformsStreamCallback_t`](#rawsensortransformsstreamcallback_t)  | Callback function type invoked when raw sensor transform data is updated. |
| `std::function< void(const RetargetProfileInfo &)>` | [`RetargetProfileInfoCallback_t`](#retargetprofileinfocallback_t)  |  |

---

#### Timestamp

```cpp
uint64_t Timestamp()
```

Integer type representing Unix time in milliseconds.

---

#### GloveId

```cpp
std::string GloveId()
```

Identifier of a glove device.

---

#### LedColor

```cpp
DeviceColor LedColor()
```

Compatibility alias for DeviceColor. Prefer DeviceColor in new code.

---

#### GloveDataStreamCallback_t

```cpp
std::function< void(const GloveData &)> GloveDataStreamCallback_t()
```

Callback function type invoked when glove data is updated.

---

#### ControllerDataStreamCallback_t

```cpp
std::function< void(const ControllerData &)> ControllerDataStreamCallback_t()
```

Callback function type invoked when controller data is updated.

---

#### ErgonomicsStreamCallback_t

```cpp
std::function< void(const ErgonomicsData &)> ErgonomicsStreamCallback_t()
```

Callback function type invoked when ergonomics data is updated.

---

#### SkeletonStreamCallback_t

```cpp
std::function< void(const SkeletonData &)> SkeletonStreamCallback_t()
```

Callback function type invoked when skeleton data is updated.

---

#### RawSkeletonStreamCallback_t

```cpp
std::function< void(const RawSkeletonData &)> RawSkeletonStreamCallback_t()
```

---

#### RawSensorTransformsStreamCallback_t

```cpp
std::function< void(const RawSensorTransformsData &)> RawSensorTransformsStreamCallback_t()
```

Callback function type invoked when raw sensor transform data is updated.

---

#### RetargetProfileInfoCallback_t

```cpp
std::function< void(const RetargetProfileInfo &)> RetargetProfileInfoCallback_t()
```

### Variables

| Return | Name | Description |
|--------|------|-------------|
| `uint32_t` | [`ERGONOMICS_DATA_COUNT`](#ergonomics_data_count)  | Number of ergonomics data elements per hand (five fingers times four joints). |
| `uint32_t` | [`SKELETON_NODE_COUNT`](#skeleton_node_count)  | Number of skeleton nodes per hand (wrist, four thumb nodes, and five nodes for each of four fingers). |
| `uint32_t` | [`SKELETON_DATA_COUNT`](#skeleton_data_count)  | Number of skeleton transforms per hand. |
| `uint32_t` | [`RAW_SENSOR_TRANSFORM_COUNT`](#raw_sensor_transform_count)  | Number of raw sensor transforms per hand. |

---

#### ERGONOMICS_DATA_COUNT

```cpp
uint32_t ERGONOMICS_DATA_COUNT = 20
```

Number of ergonomics data elements per hand (five fingers times four joints).

---

#### SKELETON_NODE_COUNT

```cpp
uint32_t SKELETON_NODE_COUNT = 25
```

Number of skeleton nodes per hand (wrist, four thumb nodes, and five nodes for each of four fingers).

---

#### SKELETON_DATA_COUNT

```cpp
uint32_t SKELETON_DATA_COUNT = SKELETON_NODE_COUNT
```

Number of skeleton transforms per hand.

---

#### RAW_SENSOR_TRANSFORM_COUNT

```cpp
uint32_t RAW_SENSOR_TRANSFORM_COUNT = 5
```

Number of raw sensor transforms per hand.



## SDK

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Main class of the ContactGlove C++/C/Rust SDK V2.

### Public Methods

| Return | Name | Description |
|--------|------|-------------|
|  | [`SDK`](#sdk-1)  |  |
|  | [`SDK`](#sdk-2)  | Deleted constructor. |
| `SDKReturnCode` | [`ConnectToHost`](#connecttohost)  | Connect to DivingStation and start data streams. |
| `SDKReturnCode` | [`Disconnect`](#disconnect)  | Disconnect from DivingStation. |
| `SDKReturnCode` | [`DoesGloveSupportHaptics`](#doesglovesupporthaptics) `const` | Query whether a glove supports haptics. |
| `SDKReturnCode` | [`GetDataForGlove`](#getdataforglove) `const` | Retrieve glove information by glove ID. |
| `SDKReturnCode` | [`GetControllerDataForGlove`](#getcontrollerdataforglove) `const` | Retrieve controller input data by glove ID. |
| `SDKReturnCode` | [`GetErgonomicsDataForGlove`](#getergonomicsdataforglove) `const` | Retrieve finger opening and bending angle data by glove ID. |
| `SDKReturnCode` | [`GetSkeletonDataForGlove`](#getskeletondataforglove) `const` | Retrieve joint position and rotation data by glove ID. |
| `SDKReturnCode` | [`GetRawSkeletonDataForGlove`](#getrawskeletondataforglove) `const` | Retrieve pre-retarget hand skeleton data by glove ID. |
| `SDKReturnCode` | [`GetRawSensorTransformsForGlove`](#getrawsensortransformsforglove) `const` | Retrieve raw sensor transform data by glove ID. |
| `SDKReturnCode` | [`GetIdsOfAvailableGloves`](#getidsofavailablegloves) `const` | Retrieve IDs of all currently recognized gloves. |
| `SDKReturnCode` | [`GetNumberofAvailableGloves`](#getnumberofavailablegloves) `const` | Retrieve the total number of currently recognized gloves. |
| `SDKReturnCode` | [`VibrateGlove`](#vibrateglove)  | Vibrate a glove. |
| `SDKReturnCode` | [`RequestRetargetProfileInfo`](#requestretargetprofileinfo)  | Send a request for retarget profile information. |
| `SDKReturnCode` | [`RegisterCallbackForGloveDataStream`](#registercallbackforglovedatastream)  | Register a callback invoked when glove information is updated. |
| `SDKReturnCode` | [`RegisterCallbackForControllerDataStream`](#registercallbackforcontrollerdatastream)  | Register a callback invoked when controller data is updated. |
| `SDKReturnCode` | [`RegisterCallbackForErgonomicsStream`](#registercallbackforergonomicsstream)  | Register a callback invoked when ergonomics data is updated. |
| `SDKReturnCode` | [`RegisterCallbackForSkeletonStream`](#registercallbackforskeletonstream)  | Register a callback invoked when skeleton data is updated. |
| `SDKReturnCode` | [`RegisterCallbackForRawSkeletonStream`](#registercallbackforrawskeletonstream)  |  |
| `SDKReturnCode` | [`RegisterCallbackForRawSensorTransformsStream`](#registercallbackforrawsensortransformsstream)  | Register a callback invoked when raw sensor transform data is updated. |
| `SDKReturnCode` | [`RegisterCallbackForRetargetProfileInfoStream`](#registercallbackforretargetprofileinfostream)  |  |

---

#### SDK

```cpp
SDK()
```

---

#### SDK

```cpp
SDK(const SDK &) = delete
```

Deleted constructor.

---

#### ConnectToHost

```cpp
SDKReturnCode ConnectToHost(const Host & p_Host)
```

Connect to DivingStation and start data streams.

Calling again with the same endpoint and ports resends Connect using the existing socket, including its OS-selected source port when sendPort is 0. Use this for automatic retries while awaiting LAN approval or recovering from lost data. Success means the request was started; it does not confirm LAN approval. Disconnect explicitly ends the session and can revoke approval.

#### Parameters
* `p_Host` Host information for the target DivingStation instance.

#### Returns
SDKReturnCode::Success — Connection started successfully.

#### Returns
SDKReturnCode::SdkNotAvailable — Failed to open a socket.

#### Returns
SDKReturnCode::InvalidArgument — `p_Host.sendPort` is greater than 65535.

#### Returns
SDKReturnCode::InvalidArgument — The receive port is already in use by a connection with a different IP address, destination port, or send port.

---

#### Disconnect

```cpp
SDKReturnCode Disconnect(const Host & p_Host)
```

Disconnect from DivingStation.

#### Parameters
* `p_Host` Host information to disconnect. It must match the value passed to `ConnectToHost()`.

#### Returns
SDKReturnCode::Success — Disconnected successfully.

#### Returns
SDKReturnCode::NotConnected — No connection exists for the specified port.

---

#### DoesGloveSupportHaptics

`const`

```cpp
SDKReturnCode DoesGloveSupportHaptics(const std::string & id, bool * p_IsHaptics) const
```

Query whether a glove supports haptics.

#### Parameters
* `id` ID of the glove to query.

* `p_IsHaptics` Output location for whether haptics are supported; must not be null.

#### Returns
SDKReturnCode::Success — Value retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — The specified glove does not exist.

---

#### GetDataForGlove

`const`

```cpp
SDKReturnCode GetDataForGlove(const std::string & id, GloveData * p_GloveData) const
```

Retrieve glove information by glove ID.

#### Parameters
* `id` ID of the glove to retrieve.

* `p_GloveData` Output location for the retrieved data; must not be null.

#### Returns
SDKReturnCode::Success — Data retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — The specified glove does not exist.

---

#### GetControllerDataForGlove

`const`

```cpp
SDKReturnCode GetControllerDataForGlove(const std::string & id, ControllerData * p_Controller) const
```

Retrieve controller input data by glove ID.

#### Parameters
* `id` ID of the glove to retrieve.

* `p_Controller` Output location for the retrieved data; must not be null.

#### Returns
SDKReturnCode::Success — Data retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_Controller` is null.

---

#### GetErgonomicsDataForGlove

`const`

```cpp
SDKReturnCode GetErgonomicsDataForGlove(const std::string & id, ErgonomicsData * p_ErgonomicsData) const
```

Retrieve finger opening and bending angle data by glove ID.

#### Parameters
* `id` ID of the glove to retrieve.

* `p_ErgonomicsData` Output location for the retrieved data; must not be null.

#### Returns
SDKReturnCode::Success — Data retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_ErgonomicsData` is null.

---

#### GetSkeletonDataForGlove

`const`

```cpp
SDKReturnCode GetSkeletonDataForGlove(const std::string & id, SkeletonData * p_SkeletonData) const
```

Retrieve joint position and rotation data by glove ID.

#### Parameters
* `id` ID of the glove to retrieve.

* `p_SkeletonData` Output location for the retrieved data; must not be null.

#### Returns
SDKReturnCode::Success — Data retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_SkeletonData` is null.

---

#### GetRawSkeletonDataForGlove

`const`

```cpp
SDKReturnCode GetRawSkeletonDataForGlove(const std::string & id, RawSkeletonData * p_RawSkeletonData) const
```

Retrieve pre-retarget hand skeleton data by glove ID.

---

#### GetRawSensorTransformsForGlove

`const`

```cpp
SDKReturnCode GetRawSensorTransformsForGlove(const std::string & id, RawSensorTransformsData * p_RawSensorTransforms) const
```

Retrieve raw sensor transform data by glove ID.

#### Parameters
* `id` ID of the glove to retrieve.

* `p_RawSensorTransforms` Output location for the retrieved data; must not be null.

#### Returns
SDKReturnCode::Success — Data retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — No data exists for the specified ID, or `p_RawSensorTransforms` is null.

---

#### GetIdsOfAvailableGloves

`const`

```cpp
SDKReturnCode GetIdsOfAvailableGloves(GloveId * p_IdsOfAvailableGloves, uint32_t p_NumberOfIdsThatFitInArray) const
```

Retrieve IDs of all currently recognized gloves.

When the output array is too small, only the first `p_NumberOfIdsThatFitInArray` IDs are written. Query the total count in advance with `GetNumberofAvailableGloves()`.

#### Parameters
* `p_IdsOfAvailableGloves` Pointer to the first element of the output ID array; must not be null.

* `p_NumberOfIdsThatFitInArray` Capacity of the output array in elements.

#### Returns
SDKReturnCode::Success — Data retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — `p_IdsOfAvailableGloves` is null.

---

#### GetNumberofAvailableGloves

`const`

```cpp
SDKReturnCode GetNumberofAvailableGloves(uint32_t * n) const
```

Retrieve the total number of currently recognized gloves.

#### Parameters
* `n` Output location for the count; must not be null.

#### Returns
SDKReturnCode::Success — Data retrieved successfully.

#### Returns
SDKReturnCode::InvalidID — `n` is null.

---

#### VibrateGlove

```cpp
SDKReturnCode VibrateGlove(const std::string & id, float frequency, float amplitude, float duration)
```

Vibrate a glove.

#### Parameters
* `id` ID of the glove to vibrate.

* `frequency` Vibration frequency in Hz (100.0 to 500.0).

* `amplitude` Vibration amplitude (0.0 to 1.0).

* `duration` Vibration duration in seconds (0.001 to 1.0).

#### Returns
SDKReturnCode::Success — Request sent successfully.

#### Returns
SDKReturnCode::InvalidID — The specified glove does not exist.

#### Returns
SDKReturnCode::InvalidArgument — An argument is outside its valid range.

---

#### RequestRetargetProfileInfo

```cpp
SDKReturnCode RequestRetargetProfileInfo(const std::string & profileUuid, int32_t requestId)
```

Send a request for retarget profile information.

---

#### RegisterCallbackForGloveDataStream

```cpp
SDKReturnCode RegisterCallbackForGloveDataStream(GloveDataStreamCallback_t p_Callback)
```

Register a callback invoked when glove information is updated.

The callback receives a `GloveData` argument.

#### Parameters
* `p_Callback` Callback function to register.

#### Returns
SDKReturnCode::Success — Registered successfully.

---

#### RegisterCallbackForControllerDataStream

```cpp
SDKReturnCode RegisterCallbackForControllerDataStream(ControllerDataStreamCallback_t p_Callback)
```

Register a callback invoked when controller data is updated.

#### Parameters
* `p_Callback` Callback function to register.

#### Returns
SDKReturnCode::Success — Registered successfully.

---

#### RegisterCallbackForErgonomicsStream

```cpp
SDKReturnCode RegisterCallbackForErgonomicsStream(ErgonomicsStreamCallback_t p_Callback)
```

Register a callback invoked when ergonomics data is updated.

#### Parameters
* `p_Callback` Callback function to register.

#### Returns
SDKReturnCode::Success — Registered successfully.

---

#### RegisterCallbackForSkeletonStream

```cpp
SDKReturnCode RegisterCallbackForSkeletonStream(SkeletonStreamCallback_t p_Callback)
```

Register a callback invoked when skeleton data is updated.

#### Parameters
* `p_Callback` Callback function to register.

#### Returns
SDKReturnCode::Success — Registered successfully.

---

#### RegisterCallbackForRawSkeletonStream

```cpp
SDKReturnCode RegisterCallbackForRawSkeletonStream(RawSkeletonStreamCallback_t p_Callback)
```

---

#### RegisterCallbackForRawSensorTransformsStream

```cpp
SDKReturnCode RegisterCallbackForRawSensorTransformsStream(RawSensorTransformsStreamCallback_t p_Callback)
```

Register a callback invoked when raw sensor transform data is updated.

#### Parameters
* `p_Callback` Callback function to register.

#### Returns
SDKReturnCode::Success — Registered successfully.

---

#### RegisterCallbackForRetargetProfileInfoStream

```cpp
SDKReturnCode RegisterCallbackForRetargetProfileInfoStream(RetargetProfileInfoCallback_t p_Callback)
```



## GloveData

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Structure holding glove device information.

Corresponds to data received at OSC address `/DS2/HC/Device`.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `std::string` | [`id`](#id)  | Device ID. |
| `std::string` | [`pairId`](#pairid)  | ID of the paired glove; empty when no pair exists. |
| `int32_t` | [`batteryPercentage`](#batterypercentage)  | Battery percentage [0 - 100]; -1 when invalid. |
| `bool` | [`isHaptics`](#ishaptics)  | Whether haptics are supported. |
| `bool` | [`isLeft`](#isleft)  | Whether this is a left-hand glove. |
| `DeviceFamilyType` | [`familyType`](#familytype)  | Device family. |
| `DeviceColor` | [`color`](#color)  | Device display-color setting. |
| `DeviceConnectedState` | [`connectedState`](#connectedstate)  | Connection state. |
| `std::string` | [`name`](#name)  | Device name. |
| `bool` | [`isExternalPowerPresent`](#isexternalpowerpresent)  | Whether external power is detected. |

---

#### id

```cpp
std::string id
```

Device ID.

---

#### pairId

```cpp
std::string pairId
```

ID of the paired glove; empty when no pair exists.

---

#### batteryPercentage

```cpp
int32_t batteryPercentage = -1
```

Battery percentage [0 - 100]; -1 when invalid.

---

#### isHaptics

```cpp
bool isHaptics = false
```

Whether haptics are supported.

---

#### isLeft

```cpp
bool isLeft = false
```

Whether this is a left-hand glove.

---

#### familyType

```cpp
DeviceFamilyType familyType = DeviceFamilyType::ContactGlove
```

Device family.

---

#### color

```cpp
DeviceColor color = DeviceColor::Achromatic
```

Device display-color setting.

---

#### connectedState

```cpp
DeviceConnectedState connectedState = DeviceConnectedState::Unknown
```

Connection state.

---

#### name

```cpp
std::string name
```

Device name.

---

#### isExternalPowerPresent

```cpp
bool isExternalPowerPresent = false
```

Whether external power is detected.



## Host

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Structure describing a DivingStation host.

Pass this structure to `ConnectToHost()` and `Disconnect()`. `port` is the OSC receive port on DivingStation. `receivePort` is the OSC receive port on the SDK. `sendPort` is the OSC send port on the SDK; when it is `0`, the operating system selects an available port.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `std::string` | [`hostName`](#hostname)  | Arbitrary label used to distinguish hosts, for example `"localhost"`. |
| `std::string` | [`ipAddress`](#ipaddress)  | IP address, for example `"127.0.0.1"`. |
| `uint32_t` | [`port`](#port)  | OSC receive port on DivingStation. |
| `uint32_t` | [`receivePort`](#receiveport)  | OSC receive port on the SDK. |
| `uint32_t` | [`sendPort`](#sendport)  | OSC send port on the SDK; 0 lets the OS select a port. |

---

#### hostName

```cpp
std::string hostName
```

Arbitrary label used to distinguish hosts, for example `"localhost"`.

---

#### ipAddress

```cpp
std::string ipAddress
```

IP address, for example `"127.0.0.1"`.

---

#### port

```cpp
uint32_t port = 25791
```

OSC receive port on DivingStation.

---

#### receivePort

```cpp
uint32_t receivePort = 25788
```

OSC receive port on the SDK.

---

#### sendPort

```cpp
uint32_t sendPort = 0
```

OSC send port on the SDK; 0 lets the OS select a port.



## Vec3

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Three-dimensional vector.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `float` | [`x`](#x)  | X component. |
| `float` | [`y`](#y)  | Y component. |
| `float` | [`z`](#z)  | Z component. |

---

#### x

```cpp
float x = 0.0f
```

X component.

---

#### y

```cpp
float y = 0.0f
```

Y component.

---

#### z

```cpp
float z = 0.0f
```

Z component.



## Quaternion

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Quaternion representing rotation.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `float` | [`w`](#w)  | Scalar component. |
| `float` | [`x`](#x-1)  | X component. |
| `float` | [`y`](#y-1)  | Y component. |
| `float` | [`z`](#z-1)  | Z component. |

---

#### w

```cpp
float w = 1.0f
```

Scalar component.

---

#### x

```cpp
float x = 0.0f
```

X component.

---

#### y

```cpp
float y = 0.0f
```

Y component.

---

#### z

```cpp
float z = 0.0f
```

Z component.



## Transform

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Transform combining position and rotation.

Used as each element of `SkeletonData::transforms`.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `Vec3` | [`position`](#position)  | Position. |
| `Quaternion` | [`rotation`](#rotation)  | Rotation. |

---

#### position

```cpp
Vec3 position
```

Position.

---

#### rotation

```cpp
Quaternion rotation
```

Rotation.



## ControllerData

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Structure holding controller input states.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `std::string` | [`id`](#id-1)  | Glove ID. |
| `bool` | [`isLeft`](#isleft-1)  | Whether this is the left hand. |
| `ButtonStatus` | [`aButton`](#abutton)  | A button state. |
| `ButtonStatus` | [`bButton`](#bbutton)  | B button state. |
| `ButtonStatus` | [`sysButton`](#sysbutton)  | System button state. |
| `ButtonStatus` | [`triggerButton`](#triggerbutton)  | Trigger button state. |
| `ButtonStatus` | [`joystickButton`](#joystickbutton)  | Joystick button state. |
| `ButtonStatus` | [`trackpadButton`](#trackpadbutton)  | Trackpad touch state. |
| `float` | [`trigger`](#trigger)  | Trigger value (0.0 to 1.0). |
| `float` | [`gripValue`](#gripvalue)  | Grip value (0.0 to 1.0). |
| `float` | [`gripForce`](#gripforce)  | Grip force (0.0 to 1.0). |
| `float` | [`joystickX`](#joystickx)  | Joystick X-axis value (-1.0 to 1.0). |
| `float` | [`joystickY`](#joysticky)  | Joystick Y-axis value (-1.0 to 1.0). |
| `float` | [`trackpadX`](#trackpadx)  | Trackpad X-axis value (-1.0 to 1.0). |
| `float` | [`trackpadY`](#trackpady)  | Trackpad Y-axis value (-1.0 to 1.0). |

---

#### id

```cpp
std::string id
```

Glove ID.

---

#### isLeft

```cpp
bool isLeft = false
```

Whether this is the left hand.

---

#### aButton

```cpp
ButtonStatus aButton = ButtonStatus::Unpressed
```

A button state.

---

#### bButton

```cpp
ButtonStatus bButton = ButtonStatus::Unpressed
```

B button state.

---

#### sysButton

```cpp
ButtonStatus sysButton = ButtonStatus::Unpressed
```

System button state.

---

#### triggerButton

```cpp
ButtonStatus triggerButton = ButtonStatus::Unpressed
```

Trigger button state.

---

#### joystickButton

```cpp
ButtonStatus joystickButton = ButtonStatus::Unpressed
```

Joystick button state.

---

#### trackpadButton

```cpp
ButtonStatus trackpadButton = ButtonStatus::Unpressed
```

Trackpad touch state.

---

#### trigger

```cpp
float trigger = 0.0f
```

Trigger value (0.0 to 1.0).

---

#### gripValue

```cpp
float gripValue = 0.0f
```

Grip value (0.0 to 1.0).

---

#### gripForce

```cpp
float gripForce = 0.0f
```

Grip force (0.0 to 1.0).

---

#### joystickX

```cpp
float joystickX = 0.0f
```

Joystick X-axis value (-1.0 to 1.0).

---

#### joystickY

```cpp
float joystickY = 0.0f
```

Joystick Y-axis value (-1.0 to 1.0).

---

#### trackpadX

```cpp
float trackpadX = 0.0f
```

Trackpad X-axis value (-1.0 to 1.0).

---

#### trackpadY

```cpp
float trackpadY = 0.0f
```

Trackpad Y-axis value (-1.0 to 1.0).



## ErgonomicsData

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Structure holding finger opening and bending angle data.

`data` has ERGONOMICS_DATA_COUNT elements for one hand. Values are in degrees. See ErgonomicsDataType for the meaning of each index.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `std::string` | [`id`](#id-2)  | Glove ID. |
| `bool` | [`isLeft`](#isleft-2)  | Whether this is the left hand. |
| `Timestamp` | [`generatedAt`](#generatedat)  | Frame generation time (Unix time in milliseconds). |
| `TrackingState` | [`trackingState`](#trackingstate-1)  | Tracking state of IK-derived data. |
| `std::array< float, ERGONOMICS_DATA_COUNT >` | [`data`](#data)  | Opening and bending angle values per joint (20 elements for one hand, in degrees). |

---

#### id

```cpp
std::string id
```

Glove ID.

---

#### isLeft

```cpp
bool isLeft = false
```

Whether this is the left hand.

---

#### generatedAt

```cpp
Timestamp generatedAt = 0
```

Frame generation time (Unix time in milliseconds).

---

#### trackingState

```cpp
TrackingState trackingState = TrackingState::Tracked
```

Tracking state of IK-derived data.

---

#### data

```cpp
std::array< float, ERGONOMICS_DATA_COUNT > data {}
```

Opening and bending angle values per joint (20 elements for one hand, in degrees).



## SkeletonData

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Structure holding hand skeleton data (joint positions and rotations).

`transforms` has SKELETON_DATA_COUNT elements for one hand. See SkeletonDataType for the meaning of each index. Positions are in meters in a right-handed hand-model coordinate system rooted at the wrist. +X points from the fingertips toward the wrist, +Y from the palm toward the back of the hand, and +Z from little finger to thumb on the right hand and from thumb to little finger on the left hand. Rotations are local quaternions relative to the parent joint; wrist and fingertip rotations are identity placeholders, not measured rotations. These conventions apply to skeleton transforms; raw sensor rotations instead represent orientations in hand-model space.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `std::string` | [`id`](#id-3)  | Glove ID. |
| `std::string` | [`profileUuid`](#profileuuid)  | Stable UUID of the retarget profile. |
| `bool` | [`isLeft`](#isleft-3)  | Whether this is the left hand. |
| `Timestamp` | [`generatedAt`](#generatedat-1)  | Frame generation time (Unix time in milliseconds). |
| `TrackingState` | [`trackingState`](#trackingstate-2)  | Tracking state of retarget IK. |
| `std::array< Transform, SKELETON_DATA_COUNT >` | [`transforms`](#transforms)  | Position and rotation per node (25 elements). |

---

#### id

```cpp
std::string id
```

Glove ID.

---

#### profileUuid

```cpp
std::string profileUuid
```

Stable UUID of the retarget profile.

---

#### isLeft

```cpp
bool isLeft = false
```

Whether this is the left hand.

---

#### generatedAt

```cpp
Timestamp generatedAt = 0
```

Frame generation time (Unix time in milliseconds).

---

#### trackingState

```cpp
TrackingState trackingState = TrackingState::Tracked
```

Tracking state of retarget IK.

---

#### transforms

```cpp
std::array< Transform, SKELETON_DATA_COUNT > transforms {}
```

Position and rotation per node (25 elements).



## RawSkeletonData

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Hand skeleton data before retargeting.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `std::string` | [`id`](#id-4)  |  |
| `bool` | [`isLeft`](#isleft-4)  |  |
| `Timestamp` | [`generatedAt`](#generatedat-2)  |  |
| `TrackingState` | [`trackingState`](#trackingstate-3)  |  |
| `std::array< Transform, SKELETON_DATA_COUNT >` | [`transforms`](#transforms-1)  |  |

---

#### id

```cpp
std::string id
```

---

#### isLeft

```cpp
bool isLeft = false
```

---

#### generatedAt

```cpp
Timestamp generatedAt = 0
```

---

#### trackingState

```cpp
TrackingState trackingState = TrackingState::Tracked
```

---

#### transforms

```cpp
std::array< Transform, SKELETON_DATA_COUNT > transforms {}
```



## RetargetProfileInfo

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Response containing retarget profile information.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `int32_t` | [`requestId`](#requestid)  |  |
| `std::string` | [`profileUuid`](#profileuuid-1)  |  |
| `int32_t` | [`status`](#status)  |  |
| `std::string` | [`name`](#name-1)  |  |
| `std::string` | [`description`](#description)  |  |

---

#### requestId

```cpp
int32_t requestId = 0
```

---

#### profileUuid

```cpp
std::string profileUuid
```

---

#### status

```cpp
int32_t status = 0
```

---

#### name

```cpp
std::string name
```

---

#### description

```cpp
std::string description
```



## RawSensorTransformsData

```cpp
#include <contactglove/ContactGloveSDKv2.hpp>
```

Structure holding raw sensor transform data for one hand.

Corresponds to data received at OSC address `/DS2/HC/RawSensorTransforms`. Each transform is expressed in the same right-handed model-rooted space as `/DS2/HC/HandSkeleton`. Its position is the sensor center relative to the `wrist`, and its rotation is the orientation from the sensor local frame to model-rooted space. `transforms` contains RAW_SENSOR_TRANSFORM_COUNT elements in thumb, index, middle, ring, and little finger order.

### Public Attributes

| Return | Name | Description |
|--------|------|-------------|
| `std::string` | [`id`](#id-5)  | Glove ID. |
| `bool` | [`isLeft`](#isleft-5)  | Whether this is the left hand. |
| `Timestamp` | [`generatedAt`](#generatedat-3)  | Frame generation time (Unix time in milliseconds). |
| `std::array< Transform, RAW_SENSOR_TRANSFORM_COUNT >` | [`transforms`](#transforms-2)  | Position and rotation per sensor (five elements for one hand). |

---

#### id

```cpp
std::string id
```

Glove ID.

---

#### isLeft

```cpp
bool isLeft = false
```

Whether this is the left hand.

---

#### generatedAt

```cpp
Timestamp generatedAt = 0
```

Frame generation time (Unix time in milliseconds).

---

#### transforms

```cpp
std::array< Transform, RAW_SENSOR_TRANSFORM_COUNT > transforms {}
```

Position and rotation per sensor (five elements for one hand).

Generated by [Moxygen](https://0state.com/moxygen)
