#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
static void wait_one_second(void) { Sleep(1000); }
#else
#include <unistd.h>
static void wait_one_second(void) { sleep(1); }
#endif

#include "contactglove/ContactGloveSDKv2C.h"

static cg_string_view_t string_view(const char* value) {
    cg_string_view_t result = {value, strlen(value)};
    return result;
}

static void CONTACTGLOVE_C_CALL on_glove(void* context, const cg_glove_data_view_t* data) {
    char id[128];
    size_t copied;
    (void)context;
    if (!data || !data->id.data) return;

    copied = data->id.length;
    if (copied >= sizeof(id)) copied = sizeof(id) - 1;
    memcpy(id, data->id.data, copied);
    id[copied] = '\0';
    printf("[callback] id=%s  battery=%d%%  left=%u\n",
           id, data->battery_percentage, data->is_left);
}

int main(void) {
    cg_sdk_t* sdk = NULL;
    cg_subscription_t subscription = 0;
    const cg_host_t host = {
        {"localhost", 9}, {"127.0.0.1", 9}, 25791, 25788, 0};
    cg_result_t result;
    int index;

    if (cg_get_abi_version() != CONTACTGLOVE_C_ABI_VERSION) {
        fprintf(stderr, "Unexpected C ABI version\n");
        return 1;
    }
    result = cg_sdk_create(&sdk);
    if (result != CG_RESULT_SUCCESS) {
        fprintf(stderr, "Failed to create SDK: %u\n", result);
        return 1;
    }
    result = cg_sdk_subscribe_glove_data(sdk, on_glove, NULL, &subscription);
    if (result != CG_RESULT_SUCCESS) {
        fprintf(stderr, "Failed to subscribe: %u\n", result);
        cg_sdk_destroy(sdk);
        return 1;
    }
    result = cg_sdk_connect(sdk, &host);
    if (result != CG_RESULT_SUCCESS) {
        fprintf(stderr, "Failed to start listener: %u\n", result);
        cg_sdk_unsubscribe(sdk, subscription);
        cg_sdk_destroy(sdk);
        return 1;
    }
    printf("Listening on port %u...\n", host.receive_port);

    for (index = 0; index < 5; ++index) {
        uint32_t count = 0;
        char selected_id[128];
        cg_string_buffer_t selected = {selected_id, sizeof(selected_id), 0};
        char id[128];
        char pair_id[128];
        char name[128];
        cg_glove_data_t data = {0};

        wait_one_second();
        result = cg_sdk_get_available_glove_count(sdk, &count);
        if (result != CG_RESULT_SUCCESS) {
            printf("[poll] could not get glove count (%u)\n", result);
            continue;
        }
        if (count == 0) {
            printf("[poll] waiting for glove data\n");
            continue;
        }
        // The list may change between calls. Handle an unavailable ID or a
        // buffer that is too small instead of using an incomplete string.
        result = cg_sdk_get_available_glove_id(sdk, 0, &selected);
        if (result != CG_RESULT_SUCCESS) {
            printf("[poll] could not get glove ID (%u)\n", result);
            continue;
        }

        data.id.data = id;
        data.id.capacity = sizeof(id);
        data.pair_id.data = pair_id;
        data.pair_id.capacity = sizeof(pair_id);
        data.name.data = name;
        data.name.capacity = sizeof(name);
        result = cg_sdk_get_glove_data(sdk, string_view(selected_id), &data);
        if (result == CG_RESULT_SUCCESS)
            printf("[poll] id=%s  name=%s\n", data.id.data, data.name.data);
        else
            printf("[poll] %s: no data yet (%u)\n", selected_id, result);
    }

    cg_sdk_unsubscribe(sdk, subscription);
    cg_sdk_disconnect(sdk, &host);
    cg_sdk_destroy(sdk);
    return 0;
}
