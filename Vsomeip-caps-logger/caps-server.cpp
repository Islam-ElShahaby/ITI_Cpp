#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstring>
#include <sstream>
#include <fstream>

#define SERVICE_ID 0x1234
#define INSTANCE_ID 0x5678
#define METHOD_ID 0x0001
#define EVENT_ID 0x8778
#define EVENTGROUP_ID 0x4465

std::shared_ptr<vsomeip::application> app;

void set_caps_lock(bool turn_on) {
    std::string brightness_path = "/sys/class/leds/input16::capslock/brightness";
    std::ofstream led_file(brightness_path);
    if (led_file.is_open()) {
        led_file << (turn_on ? "1" : "0");
        led_file.close();
        std::cout << "Set LED " << brightness_path << " to " << (turn_on ? "ON" : "OFF") << std::endl;
    } else {
            std::cerr << "Failed to write to " << brightness_path << " (Run in sudo mode)" << std::endl;
    }
}

void on_message(const std::shared_ptr<vsomeip::message> &_request) {
    if (_request->get_service() == SERVICE_ID && _request->get_instance() == INSTANCE_ID && _request->get_method() == METHOD_ID) {
        std::shared_ptr<vsomeip::payload> pl = _request->get_payload();
        std::vector<vsomeip::byte_t> data = {0};
        if(pl->get_length() > 0) {
             data.assign(pl->get_data(), pl->get_data() + pl->get_length());
        }

        // Parsing Payload
        bool is_turning_on = (data.size() > 0 && data[0] == 0x01); /* first byte is the state, also parse any value other than 0x01 as false */
        std::string client_name = "Unknown"; /* if the app doesn't send the client name, it will be unknown */
        if (data.size() > 1) {
            client_name = std::string(data.begin() + 1, data.end()); /* first byte is the state, the rest is the client name */
        }

        std::cout << "Received Request from [" << client_name << "]: Turn " << (is_turning_on ? "ON" : "OFF") << std::endl;

        // Control Physical LED
        set_caps_lock(is_turning_on);

        // Create Notification Message
        std::stringstream ss;
        ss << "Client " << client_name << " changed CapsLock to " << (is_turning_on ? "ON" : "OFF");
        std::string notification_text = ss.str();

        std::shared_ptr<vsomeip::payload> notify_payload = vsomeip::runtime::get()->create_payload();
        std::vector<vsomeip::byte_t> notify_data(notification_text.begin(), notification_text.end());
        notify_payload->set_data(notify_data);

        app->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, notify_payload);
        std::cout << "Broadcasted Event: " << notification_text << std::endl;
    }
}

int main() {
    app = vsomeip::runtime::get()->create_application("button_service_app");
    
    if (!app->init()) {
        std::cerr << "Couldn't initialize application" << std::endl;
        return 1;
    }

    std::set<vsomeip::eventgroup_t> groups;
    groups.insert(EVENTGROUP_ID);

    app->register_message_handler(SERVICE_ID, INSTANCE_ID, METHOD_ID, on_message);
    
    app->offer_service(SERVICE_ID, INSTANCE_ID);
    app->offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, groups, vsomeip::event_type_e::ET_EVENT, std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip::reliability_type_e::RT_UNKNOWN);

    std::cout << "Service Offered: " << std::hex << SERVICE_ID << ":" << INSTANCE_ID << std::endl;
    
    app->start();
    return 0;
}
