#include <chrono>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "contactglove/ContactGloveSDKv2.hpp"

int main() {
    std::mutex state_mutex;
    std::string observed_glove_id;
    std::set<std::string> printed_glove_ids;
    std::chrono::steady_clock::time_point last_selected_callback_log;

    ContactGloveSDKv2::SDK sdk;

    sdk.RegisterCallbackForGloveDataStream([&](const ContactGloveSDKv2::GloveData& g) {
        bool should_print = false;
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!g.id.empty()) {
                if (observed_glove_id.empty())
                    observed_glove_id = g.id;
                const bool first_for_id = printed_glove_ids.insert(g.id).second;
                const bool selected_id = (g.id == observed_glove_id);
                const bool periodic_selected =
                    selected_id &&
                    (last_selected_callback_log.time_since_epoch().count() == 0 ||
                     now - last_selected_callback_log >= std::chrono::seconds(1));

                should_print = first_for_id || periodic_selected;
                if (selected_id && should_print)
                    last_selected_callback_log = now;
            }
        }

        if (should_print) {
            std::cout << "[callback] id=" << g.id
                      << "  battery=" << g.batteryPercentage << "%"
                      << "  left="    << g.isLeft
                      << "  state="   << static_cast<uint32_t>(g.connectedState)
                      << "\n";
        }
    });

    ContactGloveSDKv2::Host host;
    host.hostName  = "localhost";
    host.ipAddress = "127.0.0.1";
    host.port      = 25791;
    host.receivePort = 25788;

    if (sdk.ConnectToHost(host) != ContactGloveSDKv2::SDKReturnCode::Success) {
        std::cerr << "Failed to start listener on port " << host.receivePort << "\n";
        return 1;
    }
    std::cout << "Listening on port " << host.receivePort << "...\n";

    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::string glove_id;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            glove_id = observed_glove_id;
        }

        if (glove_id.empty()) {
            std::cout << "[poll] waiting for callback data\n";
            continue;
        }

        ContactGloveSDKv2::GloveData data;
        auto rc = sdk.GetDataForGlove(glove_id, &data);
        if (rc == ContactGloveSDKv2::SDKReturnCode::Success)
            std::cout << "[poll] id=" << data.id << "  name=" << data.name << "\n";
        else
            std::cout << "[poll] " << glove_id << ": no data yet\n";
    }

    sdk.Disconnect(host);
    return 0;
}
