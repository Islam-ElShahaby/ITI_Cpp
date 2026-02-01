#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstring>
#include <sstream>
#include <fstream>
#include <atomic>

#define SERVICE_ID 0x1234
#define INSTANCE_ID 0x5678
#define METHOD_ID 0x0001
#define EVENT_ID 0x8778
#define EVENTGROUP_ID 0x4465

// GPIO Configuration for Raspberry Pi LED
#define GPIO_PIN "529"
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_UNEXPORT_PATH "/sys/class/gpio/unexport"
#define GPIO_DIRECTION_PATH "/sys/class/gpio/gpio" GPIO_PIN "/direction"
#define GPIO_VALUE_PATH "/sys/class/gpio/gpio" GPIO_PIN "/value"

std::shared_ptr<vsomeip::application> app;
std::atomic<bool> current_led_state(false);

bool init_gpio() {
    // Export GPIO pin
    std::ofstream export_file(GPIO_EXPORT_PATH);
    if (export_file.is_open()) {
        export_file << GPIO_PIN;
        export_file.close();
    } else {
        std::cerr << "Warning: Could not export GPIO pin (may already be exported)" << std::endl;
    }

    // Wait a moment for the export to take effect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Set direction to output
    std::ofstream direction_file(GPIO_DIRECTION_PATH);
    if (direction_file.is_open()) {
        direction_file << "out";
        direction_file.close();
        std::cout << "GPIO" << GPIO_PIN << " initialized as output" << std::endl;
        return true;
    } else {
        std::cerr << "Failed to set GPIO direction. Run with sudo!" << std::endl;
        return false;
    }
}

void cleanup_gpio() {
    std::ofstream unexport_file(GPIO_UNEXPORT_PATH);
    if (unexport_file.is_open()) {
        unexport_file << GPIO_PIN;
        unexport_file.close();
        std::cout << "GPIO" << GPIO_PIN << " unexported" << std::endl;
    }
}

void set_led(bool turn_on) {
    std::ofstream value_file(GPIO_VALUE_PATH);
    if (value_file.is_open()) {
        value_file << (turn_on ? "1" : "0");
        value_file.close();
        current_led_state = turn_on;
        std::cout << "Set LED to " << (turn_on ? "ON" : "OFF") << std::endl;
    } else {
        std::cerr << "Failed to write to GPIO value file" << std::endl;
    }
}

void broadcast_led_state(bool state, const std::string& source_client) {
    std::shared_ptr<vsomeip::payload> notify_payload = vsomeip::runtime::get()->create_payload();
    std::vector<vsomeip::byte_t> notify_data;
    
    // First byte: LED state
    notify_data.push_back(state ? 0x01 : 0x00);
    
    // Remaining bytes: source client name
    std::copy(source_client.begin(), source_client.end(), std::back_inserter(notify_data));
    
    notify_payload->set_data(notify_data);
    app->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, notify_payload);
    
    std::cout << "Broadcasted LED state: " << (state ? "ON" : "OFF") 
              << " (source: " << source_client << ")" << std::endl;
}

void on_message(const std::shared_ptr<vsomeip::message> &_request) {
    if (_request->get_service() == SERVICE_ID && 
        _request->get_instance() == INSTANCE_ID && 
        _request->get_method() == METHOD_ID) {
        
        std::shared_ptr<vsomeip::payload> pl = _request->get_payload();
        std::vector<vsomeip::byte_t> data = {0};
        if(pl->get_length() > 0) {
             data.assign(pl->get_data(), pl->get_data() + pl->get_length());
        }

        // Parse payload
        bool is_turning_on = (data.size() > 0 && data[0] == 0x01);
        std::string client_name = "Unknown";
        if (data.size() > 1) {
            client_name = std::string(data.begin() + 1, data.end());
        }

        std::cout << "Received LED Command from [" << client_name << "]: Turn " 
                  << (is_turning_on ? "ON" : "OFF") << std::endl;

        // Control physical LED
        set_led(is_turning_on);

        // Broadcast state to all clients so they can sync
        broadcast_led_state(is_turning_on, client_name);
    }
}

int main() {
    std::cout << "=== Raspberry Pi LED Server ===" << std::endl;
    
    // Initialize GPIO
    if (!init_gpio()) {
        std::cerr << "Warning: GPIO initialization failed, continuing anyway..." << std::endl;
    }

    // Setup vsomeip application
    app = vsomeip::runtime::get()->create_application("button_service_app");
    
    if (!app->init()) {
        std::cerr << "Couldn't initialize application" << std::endl;
        cleanup_gpio();
        return 1;
    }

    // Setup event groups for notifications
    std::set<vsomeip::eventgroup_t> groups;
    groups.insert(EVENTGROUP_ID);

    // Register message handler for LED control requests
    app->register_message_handler(SERVICE_ID, INSTANCE_ID, METHOD_ID, on_message);
    
    // Offer the LED control service
    app->offer_service(SERVICE_ID, INSTANCE_ID);
    app->offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, groups, 
                     vsomeip::event_type_e::ET_EVENT, 
                     std::chrono::milliseconds::zero(), 
                     false, true, nullptr, 
                     vsomeip::reliability_type_e::RT_UNKNOWN);

    std::cout << "Service Offered: 0x" << std::hex << SERVICE_ID 
              << ":0x" << INSTANCE_ID << std::dec << std::endl;
    std::cout << "Waiting for clients..." << std::endl;
    
    // Start the event loop
    app->start();
    
    // Cleanup on exit
    cleanup_gpio();
    return 0;
}
