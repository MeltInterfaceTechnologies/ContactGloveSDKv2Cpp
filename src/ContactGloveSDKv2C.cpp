#include "contactglove/ContactGloveSDKv2C.h"

#include "contactglove/ContactGloveSDKv2.hpp"

#include <array>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace ContactGloveSDKv2;

namespace {

cg_result_t ToResult(SDKReturnCode value) {
    return static_cast<cg_result_t>(value);
}

bool ToString(cg_string_view_t value, std::string* out) {
    if (!out || (!value.data && value.length != 0)) return false;
    out->assign(value.data ? value.data : "", value.length);
    return true;
}

cg_string_view_t ToView(const std::string& value) {
    return {value.data(), value.size()};
}

bool PrepareString(cg_string_buffer_t* target, const std::string& value) {
    if (!target) return false;
    target->length = value.size();
    return target->data && target->capacity >= value.size() + 1;
}

void CopyString(cg_string_buffer_t* target, const std::string& value) {
    std::memcpy(target->data, value.data(), value.size());
    target->data[value.size()] = '\0';
}

cg_transform_t ToTransform(const Transform& value) {
    return {{value.position.x, value.position.y, value.position.z},
            {value.rotation.w, value.rotation.x, value.rotation.y, value.rotation.z}};
}

Host ToHost(const cg_host_t& value, bool* ok) {
    Host host;
    *ok = ToString(value.host_name, &host.hostName) && ToString(value.ip_address, &host.ipAddress);
    host.port = value.port;
    host.receivePort = value.receive_port;
    host.sendPort = value.send_port;
    return host;
}

template <typename Function>
cg_result_t Guard(Function&& function) {
    try {
        return function();
    } catch (...) {
        return CG_RESULT_SDK_NOT_AVAILABLE;
    }
}

} // namespace

struct cg_sdk {
    enum class SubscriptionKind {
        Glove,
        Controller,
        Ergonomics,
        Skeleton,
        RawSkeleton,
        RawSensorTransforms,
        RetargetProfileInfo,
    };

    struct Subscription {
        SubscriptionKind kind;
        void* context = nullptr;
        cg_glove_callback_t glove = nullptr;
        cg_controller_callback_t controller = nullptr;
        cg_ergonomics_callback_t ergonomics = nullptr;
        cg_skeleton_callback_t skeleton = nullptr;
        cg_raw_skeleton_callback_t raw_skeleton = nullptr;
        cg_raw_sensor_transforms_callback_t raw_sensor_transforms = nullptr;
        cg_retarget_profile_info_callback_t profile_info = nullptr;
        bool active = true;
        size_t in_flight = 0;
    };

    std::mutex subscription_mutex;
    std::condition_variable subscription_cv;
    cg_subscription_t next_subscription = 1;
    std::unordered_map<cg_subscription_t, std::shared_ptr<Subscription>> subscriptions;
    SDK sdk;

    cg_sdk() {
        sdk.RegisterCallbackForGloveDataStream(
            [this](const GloveData& value) { DispatchGlove(value); });
        sdk.RegisterCallbackForControllerDataStream(
            [this](const ControllerData& value) { DispatchController(value); });
        sdk.RegisterCallbackForErgonomicsStream(
            [this](const ErgonomicsData& value) { DispatchErgonomics(value); });
        sdk.RegisterCallbackForSkeletonStream(
            [this](const SkeletonData& value) { DispatchSkeleton(value); });
        sdk.RegisterCallbackForRawSkeletonStream(
            [this](const RawSkeletonData& value) { DispatchRawSkeleton(value); });
        sdk.RegisterCallbackForRawSensorTransformsStream(
            [this](const RawSensorTransformsData& value) { DispatchRawSensorTransforms(value); });
        sdk.RegisterCallbackForRetargetProfileInfoStream(
            [this](const RetargetProfileInfo& value) { DispatchProfileInfo(value); });
    }

    cg_result_t AddSubscription(std::shared_ptr<Subscription> subscription,
                                cg_subscription_t* out_subscription) {
        if (!out_subscription) return CG_RESULT_INVALID_ARGUMENT;
        std::lock_guard<std::mutex> lock(subscription_mutex);
        const cg_subscription_t id = next_subscription++;
        subscriptions.emplace(id, std::move(subscription));
        *out_subscription = id;
        return CG_RESULT_SUCCESS;
    }

    cg_result_t RemoveSubscription(cg_subscription_t id) {
        std::unique_lock<std::mutex> lock(subscription_mutex);
        const auto it = subscriptions.find(id);
        if (it == subscriptions.end()) return CG_RESULT_INVALID_ARGUMENT;
        const auto subscription = it->second;
        subscription->active = false;
        subscription_cv.wait(lock, [&subscription] { return subscription->in_flight == 0; });
        subscriptions.erase(it);
        return CG_RESULT_SUCCESS;
    }

    std::vector<std::shared_ptr<Subscription>> BeginDispatch(SubscriptionKind kind) {
        std::vector<std::shared_ptr<Subscription>> ready;
        std::lock_guard<std::mutex> lock(subscription_mutex);
        for (const auto& entry : subscriptions) {
            const auto& subscription = entry.second;
            if (subscription->active && subscription->kind == kind) {
                ++subscription->in_flight;
                ready.push_back(subscription);
            }
        }
        return ready;
    }

    void EndDispatch(const std::shared_ptr<Subscription>& subscription) {
        std::lock_guard<std::mutex> lock(subscription_mutex);
        --subscription->in_flight;
        subscription_cv.notify_all();
    }

    void DispatchGlove(const GloveData& value) {
        const cg_glove_data_view_t view = {
            ToView(value.id), ToView(value.pairId), value.batteryPercentage,
            value.isHaptics ? 1u : 0u, value.isLeft ? 1u : 0u,
            static_cast<uint32_t>(value.familyType), static_cast<uint32_t>(value.color),
            static_cast<uint32_t>(value.connectedState), ToView(value.name),
            value.isExternalPowerPresent ? 1u : 0u};
        for (const auto& subscription : BeginDispatch(SubscriptionKind::Glove)) {
            try {
                subscription->glove(subscription->context, &view);
            } catch (...) {
            }
            EndDispatch(subscription);
        }
    }

    void DispatchController(const ControllerData& value) {
        const cg_controller_data_view_t view = {
            ToView(value.id), value.isLeft ? 1u : 0u,
            static_cast<uint32_t>(value.aButton), static_cast<uint32_t>(value.bButton),
            static_cast<uint32_t>(value.sysButton), static_cast<uint32_t>(value.triggerButton),
            static_cast<uint32_t>(value.joystickButton), static_cast<uint32_t>(value.trackpadButton),
            value.trigger, value.gripValue, value.gripForce, value.joystickX, value.joystickY,
            value.trackpadX, value.trackpadY};
        for (const auto& subscription : BeginDispatch(SubscriptionKind::Controller)) {
            try {
                subscription->controller(subscription->context, &view);
            } catch (...) {
            }
            EndDispatch(subscription);
        }
    }

    void DispatchErgonomics(const ErgonomicsData& value) {
        const cg_ergonomics_data_view_t view = {
            ToView(value.id), value.isLeft ? 1u : 0u, value.generatedAt,
            static_cast<uint32_t>(value.trackingState),
            value.data.data(), value.data.size()};
        for (const auto& subscription : BeginDispatch(SubscriptionKind::Ergonomics)) {
            try {
                subscription->ergonomics(subscription->context, &view);
            } catch (...) {
            }
            EndDispatch(subscription);
        }
    }

    void DispatchSkeleton(const SkeletonData& value) {
        std::array<cg_transform_t, SKELETON_DATA_COUNT> transforms{};
        for (size_t index = 0; index < transforms.size(); ++index)
            transforms[index] = ToTransform(value.transforms[index]);
        const cg_skeleton_data_view_t view = {
            ToView(value.id), ToView(value.profileUuid), value.isLeft ? 1u : 0u, value.generatedAt,
            static_cast<uint32_t>(value.trackingState), transforms.data(), transforms.size()};
        for (const auto& subscription : BeginDispatch(SubscriptionKind::Skeleton)) {
            try {
                subscription->skeleton(subscription->context, &view);
            } catch (...) {
            }
            EndDispatch(subscription);
        }
    }

    void DispatchRawSkeleton(const RawSkeletonData& value) {
        std::array<cg_transform_t, SKELETON_DATA_COUNT> transforms{};
        for (size_t index = 0; index < transforms.size(); ++index)
            transforms[index] = ToTransform(value.transforms[index]);
        const cg_raw_skeleton_data_view_t view = {
            ToView(value.id), value.isLeft ? 1u : 0u, value.generatedAt,
            static_cast<uint32_t>(value.trackingState), transforms.data(), transforms.size()};
        for (const auto& subscription : BeginDispatch(SubscriptionKind::RawSkeleton)) {
            try {
                subscription->raw_skeleton(subscription->context, &view);
            } catch (...) {
            }
            EndDispatch(subscription);
        }
    }

    void DispatchRawSensorTransforms(const RawSensorTransformsData& value) {
        std::array<cg_transform_t, RAW_SENSOR_TRANSFORM_COUNT> transforms{};
        for (size_t index = 0; index < transforms.size(); ++index)
            transforms[index] = ToTransform(value.transforms[index]);
        const cg_raw_sensor_transforms_data_view_t view = {
            ToView(value.id), value.isLeft ? 1u : 0u, value.generatedAt,
            transforms.data(), transforms.size()};
        for (const auto& subscription : BeginDispatch(SubscriptionKind::RawSensorTransforms)) {
            try {
                subscription->raw_sensor_transforms(subscription->context, &view);
            } catch (...) {
            }
            EndDispatch(subscription);
        }
    }

    void DispatchProfileInfo(const RetargetProfileInfo& value) {
        const cg_retarget_profile_info_view_t view = {
            value.requestId, ToView(value.profileUuid), value.status,
            ToView(value.name), ToView(value.description)};
        for (const auto& subscription : BeginDispatch(SubscriptionKind::RetargetProfileInfo)) {
            try {
                subscription->profile_info(subscription->context, &view);
            } catch (...) {
            }
            EndDispatch(subscription);
        }
    }
};

extern "C" {

uint32_t CONTACTGLOVE_C_CALL cg_get_abi_version(void) {
    return CONTACTGLOVE_C_ABI_VERSION;
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_create(cg_sdk_t** out_sdk) {
    if (!out_sdk) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        *out_sdk = new cg_sdk();
        return CG_RESULT_SUCCESS;
    });
}

void CONTACTGLOVE_C_CALL cg_sdk_destroy(cg_sdk_t* sdk) {
    try {
        delete sdk;
    } catch (...) {
    }
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_connect(cg_sdk_t* sdk, const cg_host_t* host) {
    if (!sdk || !host) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        bool ok = false;
        const Host native_host = ToHost(*host, &ok);
        return ok ? ToResult(sdk->sdk.ConnectToHost(native_host)) : CG_RESULT_INVALID_ARGUMENT;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_disconnect(cg_sdk_t* sdk, const cg_host_t* host) {
    if (!sdk || !host) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        bool ok = false;
        const Host native_host = ToHost(*host, &ok);
        return ok ? ToResult(sdk->sdk.Disconnect(native_host)) : CG_RESULT_INVALID_ARGUMENT;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_does_glove_support_haptics(
    cg_sdk_t* sdk, cg_string_view_t id, uint32_t* out_is_haptics) {
    if (!sdk || !out_is_haptics) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        bool supports_haptics = false;
        const auto result = sdk->sdk.DoesGloveSupportHaptics(native_id, &supports_haptics);
        if (result == SDKReturnCode::Success) *out_is_haptics = supports_haptics ? 1u : 0u;
        return ToResult(result);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_glove_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_glove_data_t* out_data) {
    if (!sdk || !out_data) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        GloveData value;
        const auto result = sdk->sdk.GetDataForGlove(native_id, &value);
        if (result != SDKReturnCode::Success) return ToResult(result);
        const bool id_fits = PrepareString(&out_data->id, value.id);
        const bool pair_id_fits = PrepareString(&out_data->pair_id, value.pairId);
        const bool name_fits = PrepareString(&out_data->name, value.name);
        const bool fits = id_fits && pair_id_fits && name_fits;
        if (!fits) return CG_RESULT_BUFFER_TOO_SMALL;
        CopyString(&out_data->id, value.id);
        CopyString(&out_data->pair_id, value.pairId);
        CopyString(&out_data->name, value.name);
        out_data->battery_percentage = value.batteryPercentage;
        out_data->is_haptics = value.isHaptics ? 1u : 0u;
        out_data->is_left = value.isLeft ? 1u : 0u;
        out_data->family_type = static_cast<uint32_t>(value.familyType);
        out_data->color = static_cast<uint32_t>(value.color);
        out_data->connected_state = static_cast<uint32_t>(value.connectedState);
        out_data->is_external_power_present = value.isExternalPowerPresent ? 1u : 0u;
        return CG_RESULT_SUCCESS;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_controller_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_controller_data_t* out_data) {
    if (!sdk || !out_data) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        ControllerData value;
        const auto result = sdk->sdk.GetControllerDataForGlove(native_id, &value);
        if (result != SDKReturnCode::Success) return ToResult(result);
        if (!PrepareString(&out_data->id, value.id)) return CG_RESULT_BUFFER_TOO_SMALL;
        CopyString(&out_data->id, value.id);
        out_data->is_left = value.isLeft ? 1u : 0u;
        out_data->a_button = static_cast<uint32_t>(value.aButton);
        out_data->b_button = static_cast<uint32_t>(value.bButton);
        out_data->sys_button = static_cast<uint32_t>(value.sysButton);
        out_data->trigger_button = static_cast<uint32_t>(value.triggerButton);
        out_data->joystick_button = static_cast<uint32_t>(value.joystickButton);
        out_data->trackpad_button = static_cast<uint32_t>(value.trackpadButton);
        out_data->trigger = value.trigger;
        out_data->grip_value = value.gripValue;
        out_data->grip_force = value.gripForce;
        out_data->joystick_x = value.joystickX;
        out_data->joystick_y = value.joystickY;
        out_data->trackpad_x = value.trackpadX;
        out_data->trackpad_y = value.trackpadY;
        return CG_RESULT_SUCCESS;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_ergonomics_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_ergonomics_data_t* out_data) {
    if (!sdk || !out_data) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        ErgonomicsData value;
        const auto result = sdk->sdk.GetErgonomicsDataForGlove(native_id, &value);
        if (result != SDKReturnCode::Success) return ToResult(result);
        if (!PrepareString(&out_data->id, value.id)) return CG_RESULT_BUFFER_TOO_SMALL;
        CopyString(&out_data->id, value.id);
        out_data->is_left = value.isLeft ? 1u : 0u;
        out_data->generated_at = value.generatedAt;
        out_data->tracking_state = static_cast<uint32_t>(value.trackingState);
        std::memcpy(out_data->data, value.data.data(), sizeof(out_data->data));
        return CG_RESULT_SUCCESS;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_skeleton_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_skeleton_data_t* out_data) {
    if (!sdk || !out_data) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        SkeletonData value;
        const auto result = sdk->sdk.GetSkeletonDataForGlove(native_id, &value);
        if (result != SDKReturnCode::Success) return ToResult(result);
        if (!PrepareString(&out_data->id, value.id) ||
            !PrepareString(&out_data->profile_uuid, value.profileUuid))
            return CG_RESULT_BUFFER_TOO_SMALL;
        CopyString(&out_data->id, value.id);
        CopyString(&out_data->profile_uuid, value.profileUuid);
        out_data->is_left = value.isLeft ? 1u : 0u;
        out_data->generated_at = value.generatedAt;
        out_data->tracking_state = static_cast<uint32_t>(value.trackingState);
        for (size_t index = 0; index < value.transforms.size(); ++index)
            out_data->transforms[index] = ToTransform(value.transforms[index]);
        return CG_RESULT_SUCCESS;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_raw_skeleton_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_raw_skeleton_data_t* out_data) {
    if (!sdk || !out_data) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        RawSkeletonData value;
        const auto result = sdk->sdk.GetRawSkeletonDataForGlove(native_id, &value);
        if (result != SDKReturnCode::Success) return ToResult(result);
        if (!PrepareString(&out_data->id, value.id)) return CG_RESULT_BUFFER_TOO_SMALL;
        CopyString(&out_data->id, value.id);
        out_data->is_left = value.isLeft ? 1u : 0u;
        out_data->generated_at = value.generatedAt;
        out_data->tracking_state = static_cast<uint32_t>(value.trackingState);
        for (size_t index = 0; index < value.transforms.size(); ++index)
            out_data->transforms[index] = ToTransform(value.transforms[index]);
        return CG_RESULT_SUCCESS;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_raw_sensor_transforms_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_raw_sensor_transforms_data_t* out_data) {
    if (!sdk || !out_data) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        RawSensorTransformsData value;
        const auto result = sdk->sdk.GetRawSensorTransformsForGlove(native_id, &value);
        if (result != SDKReturnCode::Success) return ToResult(result);
        if (!PrepareString(&out_data->id, value.id)) return CG_RESULT_BUFFER_TOO_SMALL;
        CopyString(&out_data->id, value.id);
        out_data->is_left = value.isLeft ? 1u : 0u;
        out_data->generated_at = value.generatedAt;
        for (size_t index = 0; index < value.transforms.size(); ++index)
            out_data->transforms[index] = ToTransform(value.transforms[index]);
        return CG_RESULT_SUCCESS;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_available_glove_count(
    cg_sdk_t* sdk, uint32_t* out_count) {
    if (!sdk || !out_count) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] { return ToResult(sdk->sdk.GetNumberofAvailableGloves(out_count)); });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_available_glove_id(
    cg_sdk_t* sdk, uint32_t index, cg_string_buffer_t* out_id) {
    if (!sdk || !out_id) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        uint32_t count = 0;
        const auto count_result = sdk->sdk.GetNumberofAvailableGloves(&count);
        if (count_result != SDKReturnCode::Success) return ToResult(count_result);
        if (index >= count) return CG_RESULT_INVALID_ID;
        std::vector<GloveId> ids(count);
        const auto ids_result = sdk->sdk.GetIdsOfAvailableGloves(ids.data(), count);
        if (ids_result != SDKReturnCode::Success) return ToResult(ids_result);
        if (!PrepareString(out_id, ids[index])) return CG_RESULT_BUFFER_TOO_SMALL;
        CopyString(out_id, ids[index]);
        return CG_RESULT_SUCCESS;
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_vibrate_glove(
    cg_sdk_t* sdk, cg_string_view_t id, float frequency, float amplitude, float duration) {
    if (!sdk) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_id;
        if (!ToString(id, &native_id)) return CG_RESULT_INVALID_ARGUMENT;
        return ToResult(sdk->sdk.VibrateGlove(native_id, frequency, amplitude, duration));
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_request_retarget_profile_info(
    cg_sdk_t* sdk, cg_string_view_t profile_uuid, int32_t request_id) {
    if (!sdk) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        std::string native_uuid;
        if (!ToString(profile_uuid, &native_uuid)) return CG_RESULT_INVALID_ARGUMENT;
        return ToResult(sdk->sdk.RequestRetargetProfileInfo(native_uuid, request_id));
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_glove_data(
    cg_sdk_t* sdk, cg_glove_callback_t callback, void* context, cg_subscription_t* out_subscription) {
    if (!sdk || !callback) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        auto subscription = std::make_shared<cg_sdk::Subscription>();
        subscription->kind = cg_sdk::SubscriptionKind::Glove;
        subscription->context = context;
        subscription->glove = callback;
        return sdk->AddSubscription(std::move(subscription), out_subscription);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_controller_data(
    cg_sdk_t* sdk, cg_controller_callback_t callback, void* context, cg_subscription_t* out_subscription) {
    if (!sdk || !callback) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        auto subscription = std::make_shared<cg_sdk::Subscription>();
        subscription->kind = cg_sdk::SubscriptionKind::Controller;
        subscription->context = context;
        subscription->controller = callback;
        return sdk->AddSubscription(std::move(subscription), out_subscription);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_ergonomics_data(
    cg_sdk_t* sdk, cg_ergonomics_callback_t callback, void* context, cg_subscription_t* out_subscription) {
    if (!sdk || !callback) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        auto subscription = std::make_shared<cg_sdk::Subscription>();
        subscription->kind = cg_sdk::SubscriptionKind::Ergonomics;
        subscription->context = context;
        subscription->ergonomics = callback;
        return sdk->AddSubscription(std::move(subscription), out_subscription);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_skeleton_data(
    cg_sdk_t* sdk, cg_skeleton_callback_t callback, void* context, cg_subscription_t* out_subscription) {
    if (!sdk || !callback) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        auto subscription = std::make_shared<cg_sdk::Subscription>();
        subscription->kind = cg_sdk::SubscriptionKind::Skeleton;
        subscription->context = context;
        subscription->skeleton = callback;
        return sdk->AddSubscription(std::move(subscription), out_subscription);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_raw_skeleton_data(
    cg_sdk_t* sdk, cg_raw_skeleton_callback_t callback, void* context, cg_subscription_t* out_subscription) {
    if (!sdk || !callback) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        auto subscription = std::make_shared<cg_sdk::Subscription>();
        subscription->kind = cg_sdk::SubscriptionKind::RawSkeleton;
        subscription->context = context;
        subscription->raw_skeleton = callback;
        return sdk->AddSubscription(std::move(subscription), out_subscription);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_raw_sensor_transforms_data(
    cg_sdk_t* sdk, cg_raw_sensor_transforms_callback_t callback, void* context, cg_subscription_t* out_subscription) {
    if (!sdk || !callback) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        auto subscription = std::make_shared<cg_sdk::Subscription>();
        subscription->kind = cg_sdk::SubscriptionKind::RawSensorTransforms;
        subscription->context = context;
        subscription->raw_sensor_transforms = callback;
        return sdk->AddSubscription(std::move(subscription), out_subscription);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_retarget_profile_info(
    cg_sdk_t* sdk, cg_retarget_profile_info_callback_t callback, void* context, cg_subscription_t* out_subscription) {
    if (!sdk || !callback) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] {
        auto subscription = std::make_shared<cg_sdk::Subscription>();
        subscription->kind = cg_sdk::SubscriptionKind::RetargetProfileInfo;
        subscription->context = context;
        subscription->profile_info = callback;
        return sdk->AddSubscription(std::move(subscription), out_subscription);
    });
}

cg_result_t CONTACTGLOVE_C_CALL cg_sdk_unsubscribe(
    cg_sdk_t* sdk, cg_subscription_t subscription) {
    if (!sdk || subscription == 0) return CG_RESULT_INVALID_ARGUMENT;
    return Guard([&] { return sdk->RemoveSubscription(subscription); });
}

} // extern "C"
