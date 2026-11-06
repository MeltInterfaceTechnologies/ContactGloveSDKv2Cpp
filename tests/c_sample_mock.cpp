#include "contactglove/ContactGloveSDKv2C.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

// No sockets or SDK runtime: exercise the actual C sample with a callback
// producer running concurrently with its polling loop. The callback ID and
// the cached polling ID deliberately differ, as they can after an update.
struct cg_sdk {
    cg_glove_callback_t callback = nullptr;
    void* context = nullptr;
    std::atomic<bool> stop{false};
    std::atomic<unsigned> callbacks{0};
    std::thread receiver;
    unsigned count_calls = 0;
    unsigned id_calls = 0;
    unsigned successful_polls = 0;
    unsigned invalid_polls = 0;
};

static const char callback_id[] = "CALLBACK-A";
static const char cached_id[] = "CACHED-B";
static bool write_string(cg_string_buffer_t* target, const char* text) {
    const size_t length = std::strlen(text);
    target->length = length;
    if (!target->data || target->capacity <= length) return false;
    std::memcpy(target->data, text, length + 1);
    return true;
}

extern "C" {
uint32_t CONTACTGLOVE_C_CALL cg_get_abi_version(void) { return CONTACTGLOVE_C_ABI_VERSION; }
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_create(cg_sdk_t** out) {
    *out = new cg_sdk;
    return CG_RESULT_SUCCESS;
}
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_subscribe_glove_data(
    cg_sdk_t* sdk, cg_glove_callback_t callback, void* context, cg_subscription_t* out) {
    sdk->callback = callback;
    sdk->context = context;
    *out = 1;
    return CG_RESULT_SUCCESS;
}
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_connect(cg_sdk_t* sdk, const cg_host_t*) {
    sdk->receiver = std::thread([sdk] {
        cg_glove_data_view_t data{};
        data.id = {callback_id, sizeof(callback_id) - 1};
        data.battery_percentage = 88;
        while (!sdk->stop.load()) {
            sdk->callback(sdk->context, &data);
            sdk->callbacks.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    while (sdk->callbacks.load() == 0) std::this_thread::yield();
    return CG_RESULT_SUCCESS;
}
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_available_glove_count(cg_sdk_t* sdk, uint32_t* count) {
    *count = ++sdk->count_calls == 1 ? 0 : 1;
    return CG_RESULT_SUCCESS;
}
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_available_glove_id(
    cg_sdk_t* sdk, uint32_t index, cg_string_buffer_t* out) {
    if (index != 0) return CG_RESULT_INVALID_ID;
    if (++sdk->id_calls == 1) {
        out->length = 150; // The sample must not poll with an incomplete ID.
        return CG_RESULT_BUFFER_TOO_SMALL;
    }
    return write_string(out, cached_id) ? CG_RESULT_SUCCESS : CG_RESULT_BUFFER_TOO_SMALL;
}
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_get_glove_data(
    cg_sdk_t* sdk, cg_string_view_t id, cg_glove_data_t* out) {
    if (id.length != sizeof(cached_id) - 1 || std::memcmp(id.data, cached_id, id.length) != 0) {
        ++sdk->invalid_polls;
        return CG_RESULT_INVALID_ID;
    }
    if (!write_string(&out->id, cached_id) || !write_string(&out->pair_id, "") ||
        !write_string(&out->name, "mock glove")) return CG_RESULT_BUFFER_TOO_SMALL;
    ++sdk->successful_polls;
    return CG_RESULT_SUCCESS;
}
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_unsubscribe(cg_sdk_t* sdk, cg_subscription_t) {
    sdk->stop.store(true);
    sdk->receiver.join();
    return CG_RESULT_SUCCESS;
}
cg_result_t CONTACTGLOVE_C_CALL cg_sdk_disconnect(cg_sdk_t*, const cg_host_t*) {
    return CG_RESULT_SUCCESS;
}
void CONTACTGLOVE_C_CALL cg_sdk_destroy(cg_sdk_t* sdk) {
    const bool passed = sdk->callbacks.load() > 1 && sdk->count_calls == 5 &&
                        sdk->id_calls == 4 && sdk->successful_polls == 3 &&
                        sdk->invalid_polls == 0;
    delete sdk;
    if (!passed) {
        std::fprintf(stderr, "C sample concurrent polling regression failed\n");
        std::exit(EXIT_FAILURE);
    }
}
}
