#ifndef CONTACTGLOVE_SDK_V2_C_H
#define CONTACTGLOVE_SDK_V2_C_H

/*
 * ContactGlove C++/C/Rust SDK V2 C ABI, version 2.
 *
 * This header is valid C and C++.  It deliberately contains no C++ types,
 * exceptions, or caller-owned SDK allocations, so it is the supported FFI
 * boundary for bindings in other languages.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
  #if defined(CONTACTGLOVE_SDK_STATIC)
    #define CONTACTGLOVE_C_API
  #elif defined(CONTACTGLOVE_SDK_EXPORTS)
    #define CONTACTGLOVE_C_API __declspec(dllexport)
  #else
    #define CONTACTGLOVE_C_API __declspec(dllimport)
  #endif
  #define CONTACTGLOVE_C_CALL __cdecl
#else
  #define CONTACTGLOVE_C_API __attribute__((visibility("default")))
  #define CONTACTGLOVE_C_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CONTACTGLOVE_C_ABI_VERSION 2u
#define CONTACTGLOVE_ERGONOMICS_DATA_COUNT 20u
#define CONTACTGLOVE_SKELETON_DATA_COUNT 25u
#define CONTACTGLOVE_RAW_SENSOR_TRANSFORM_COUNT 5u

typedef struct cg_sdk cg_sdk_t;
typedef uint64_t cg_subscription_t;

typedef uint32_t cg_result_t;
#define CG_RESULT_SUCCESS           0u
#define CG_RESULT_SDK_NOT_AVAILABLE 1u
#define CG_RESULT_NOT_CONNECTED     2u
#define CG_RESULT_INVALID_ID        3u
#define CG_RESULT_INVALID_ARGUMENT  4u
#define CG_RESULT_BUFFER_TOO_SMALL  5u

typedef struct cg_string_view {
    const char* data;
    size_t      length;
} cg_string_view_t;

/*
 * Caller-owned UTF-8 output buffer.  Set data and capacity before calling an
 * output function.  The SDK writes length excluding the terminating NUL.  On
 * CG_RESULT_BUFFER_TOO_SMALL length is still set to the required size and no
 * output data fields are written.
 */
typedef struct cg_string_buffer {
    char*  data;
    size_t capacity;
    size_t length;
} cg_string_buffer_t;

typedef struct cg_host {
    cg_string_view_t host_name;
    cg_string_view_t ip_address;
    uint32_t         port;
    uint32_t         receive_port;
    uint32_t         send_port; /* 0 lets the OS select an ephemeral port. */
} cg_host_t;

typedef struct cg_vec3 {
    float x;
    float y;
    float z;
} cg_vec3_t;

typedef struct cg_quaternion {
    float w;
    float x;
    float y;
    float z;
} cg_quaternion_t;

typedef struct cg_transform {
    cg_vec3_t       position;
    cg_quaternion_t rotation;
} cg_transform_t;

typedef struct cg_glove_data {
    cg_string_buffer_t id;
    cg_string_buffer_t pair_id;
    int32_t            battery_percentage;
    uint32_t           is_haptics;
    uint32_t           is_left;
    uint32_t           family_type;
    uint32_t           color;
    uint32_t           connected_state;
    cg_string_buffer_t name;
    uint32_t           is_external_power_present;
} cg_glove_data_t;

typedef struct cg_controller_data {
    cg_string_buffer_t id;
    uint32_t           is_left;
    uint32_t           a_button;
    uint32_t           b_button;
    uint32_t           sys_button;
    uint32_t           trigger_button;
    uint32_t           joystick_button;
    uint32_t           trackpad_button;
    float              trigger;
    float              grip_value;
    float              grip_force;
    float              joystick_x;
    float              joystick_y;
    float              trackpad_x;
    float              trackpad_y;
} cg_controller_data_t;

typedef struct cg_ergonomics_data {
    cg_string_buffer_t id;
    uint32_t           is_left;
    uint64_t           generated_at;
    uint32_t           tracking_state;
    float              data[CONTACTGLOVE_ERGONOMICS_DATA_COUNT];
} cg_ergonomics_data_t;

typedef struct cg_skeleton_data {
    cg_string_buffer_t id;
    cg_string_buffer_t profile_uuid;
    uint32_t           is_left;
    uint64_t           generated_at;
    uint32_t            tracking_state;
    cg_transform_t     transforms[CONTACTGLOVE_SKELETON_DATA_COUNT];
} cg_skeleton_data_t;

typedef struct cg_raw_skeleton_data {
    cg_string_buffer_t id;
    uint32_t            is_left;
    uint64_t            generated_at;
    uint32_t            tracking_state;
    cg_transform_t      transforms[CONTACTGLOVE_SKELETON_DATA_COUNT];
} cg_raw_skeleton_data_t;

typedef struct cg_raw_sensor_transforms_data {
    cg_string_buffer_t id;
    uint32_t           is_left;
    uint64_t           generated_at;
    cg_transform_t     transforms[CONTACTGLOVE_RAW_SENSOR_TRANSFORM_COUNT];
} cg_raw_sensor_transforms_data_t;

/* Borrowed callback views are valid only until the callback returns. */
typedef struct cg_glove_data_view {
    cg_string_view_t id;
    cg_string_view_t pair_id;
    int32_t          battery_percentage;
    uint32_t         is_haptics;
    uint32_t         is_left;
    uint32_t         family_type;
    uint32_t         color;
    uint32_t         connected_state;
    cg_string_view_t name;
    uint32_t         is_external_power_present;
} cg_glove_data_view_t;

typedef struct cg_controller_data_view {
    cg_string_view_t id;
    uint32_t         is_left;
    uint32_t         a_button;
    uint32_t         b_button;
    uint32_t         sys_button;
    uint32_t         trigger_button;
    uint32_t         joystick_button;
    uint32_t         trackpad_button;
    float            trigger;
    float            grip_value;
    float            grip_force;
    float            joystick_x;
    float            joystick_y;
    float            trackpad_x;
    float            trackpad_y;
} cg_controller_data_view_t;

typedef struct cg_ergonomics_data_view {
    cg_string_view_t id;
    uint32_t         is_left;
    uint64_t         generated_at;
    uint32_t         tracking_state;
    const float*     data;
    size_t           data_count;
} cg_ergonomics_data_view_t;

typedef struct cg_skeleton_data_view {
    cg_string_view_t   id;
    cg_string_view_t   profile_uuid;
    uint32_t           is_left;
    uint64_t           generated_at;
    uint32_t           tracking_state;
    const cg_transform_t* transforms;
    size_t              transform_count;
} cg_skeleton_data_view_t;

typedef struct cg_raw_skeleton_data_view {
    cg_string_view_t    id;
    uint32_t            is_left;
    uint64_t            generated_at;
    uint32_t            tracking_state;
    const cg_transform_t* transforms;
    size_t              transform_count;
} cg_raw_skeleton_data_view_t;

typedef struct cg_raw_sensor_transforms_data_view {
    cg_string_view_t   id;
    uint32_t           is_left;
    uint64_t           generated_at;
    const cg_transform_t* transforms;
    size_t              transform_count;
} cg_raw_sensor_transforms_data_view_t;

typedef struct cg_retarget_profile_info_data {
    int32_t             request_id;
    cg_string_buffer_t  profile_uuid;
    int32_t             status;
    cg_string_buffer_t  name;
    cg_string_buffer_t  description;
} cg_retarget_profile_info_data_t;

typedef struct cg_retarget_profile_info_view {
    int32_t             request_id;
    cg_string_view_t    profile_uuid;
    int32_t             status;
    cg_string_view_t    name;
    cg_string_view_t    description;
} cg_retarget_profile_info_view_t;

typedef void (CONTACTGLOVE_C_CALL *cg_glove_callback_t)(
    void* context, const cg_glove_data_view_t* data);
typedef void (CONTACTGLOVE_C_CALL *cg_controller_callback_t)(
    void* context, const cg_controller_data_view_t* data);
typedef void (CONTACTGLOVE_C_CALL *cg_ergonomics_callback_t)(
    void* context, const cg_ergonomics_data_view_t* data);
typedef void (CONTACTGLOVE_C_CALL *cg_skeleton_callback_t)(
    void* context, const cg_skeleton_data_view_t* data);
typedef void (CONTACTGLOVE_C_CALL *cg_raw_skeleton_callback_t)(
    void* context, const cg_raw_skeleton_data_view_t* data);
typedef void (CONTACTGLOVE_C_CALL *cg_raw_sensor_transforms_callback_t)(
    void* context, const cg_raw_sensor_transforms_data_view_t* data);
typedef void (CONTACTGLOVE_C_CALL *cg_retarget_profile_info_callback_t)(
    void* context, const cg_retarget_profile_info_view_t* data);

CONTACTGLOVE_C_API uint32_t CONTACTGLOVE_C_CALL cg_get_abi_version(void);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_create(cg_sdk_t** out_sdk);

/* Do not call from an SDK callback.  Destroy waits for receiver threads. */
CONTACTGLOVE_C_API void CONTACTGLOVE_C_CALL cg_sdk_destroy(cg_sdk_t* sdk);

CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_connect(
    cg_sdk_t* sdk, const cg_host_t* host);
/* Do not call from an SDK callback. */
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_disconnect(
    cg_sdk_t* sdk, const cg_host_t* host);

CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_does_glove_support_haptics(
    cg_sdk_t* sdk, cg_string_view_t id, uint32_t* out_is_haptics);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_glove_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_glove_data_t* out_data);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_controller_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_controller_data_t* out_data);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_ergonomics_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_ergonomics_data_t* out_data);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_skeleton_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_skeleton_data_t* out_data);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_raw_skeleton_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_raw_skeleton_data_t* out_data);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_raw_sensor_transforms_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_raw_sensor_transforms_data_t* out_data);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_available_glove_count(
    cg_sdk_t* sdk, uint32_t* out_count);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_available_glove_id(
    cg_sdk_t* sdk, uint32_t index, cg_string_buffer_t* out_id);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_vibrate_glove(
    cg_sdk_t* sdk, cg_string_view_t id, float frequency, float amplitude, float duration);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_request_retarget_profile_info(
    cg_sdk_t* sdk, cg_string_view_t profile_uuid, int32_t request_id);

CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_glove_data(
    cg_sdk_t* sdk, cg_glove_callback_t callback, void* context, cg_subscription_t* out_subscription);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_controller_data(
    cg_sdk_t* sdk, cg_controller_callback_t callback, void* context, cg_subscription_t* out_subscription);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_ergonomics_data(
    cg_sdk_t* sdk, cg_ergonomics_callback_t callback, void* context, cg_subscription_t* out_subscription);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_skeleton_data(
    cg_sdk_t* sdk, cg_skeleton_callback_t callback, void* context, cg_subscription_t* out_subscription);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_raw_skeleton_data(
    cg_sdk_t* sdk, cg_raw_skeleton_callback_t callback, void* context, cg_subscription_t* out_subscription);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_raw_sensor_transforms_data(
    cg_sdk_t* sdk, cg_raw_sensor_transforms_callback_t callback, void* context, cg_subscription_t* out_subscription);
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_retarget_profile_info(
    cg_sdk_t* sdk, cg_retarget_profile_info_callback_t callback, void* context, cg_subscription_t* out_subscription);
/* Do not call from an SDK callback.  On return the callback is no longer running. */
CONTACTGLOVE_C_API cg_result_t CONTACTGLOVE_C_CALL cg_sdk_unsubscribe(
    cg_sdk_t* sdk, cg_subscription_t subscription);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONTACTGLOVE_SDK_V2_C_H */
