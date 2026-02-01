#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <limits>
#include <condition_variable>
#include <vector>
#include <fstream>
#include <atomic>
#include <dirent.h>
#include <cstring>

#define SERVICE_ID 0x1234
#define INSTANCE_ID 0x5678
#define METHOD_ID 0x0001
#define EVENT_ID 0x8778
#define EVENTGROUP_ID 0x4465

std::mutex server_available_mutex;
std::condition_variable server_available_cv;
bool is_connected = false;
std::string client_name;
std::string capslock_led_path;

std::shared_ptr<vsomeip::application> app;
std::atomic<bool> running(true);
std::atomic<bool> is_syncing(false);

/* the Dynamically find the CapsLock LED path functions are made by ai *
 * capslock path kept changing idk why                                 */

std::string find_capslock_led_path() {
    const std::string leds_base = "/sys/class/leds/";
    DIR* dir = opendir(leds_base.c_str());
    if (!dir) {
        std::cerr << "Could not open " << leds_base << std::endl;
        return "";
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.find("capslock") != std::string::npos) {
            closedir(dir);
            return leds_base + name + "/brightness";
        }
    }
    closedir(dir);
    return "";
}

int get_capslock_state() {
    if (capslock_led_path.empty()) return -1;
    
    std::ifstream file(capslock_led_path);
    int value = -1;
    if (file) {
        file >> value;
    }
    return value;
}

void set_capslock_led(bool turn_on) {
    if (capslock_led_path.empty()) {
        std::cerr << "CapsLock LED path not found" << std::endl;
        return;
    }
    
    std::ofstream led_file(capslock_led_path);
    if (led_file.is_open()) {
        led_file << (turn_on ? "1" : "0");
        led_file.close();
        std::cout << "Set local CapsLock LED to " << (turn_on ? "ON" : "OFF") << std::endl;
    } else {
        std::cerr << "Failed to write to " << capslock_led_path 
                  << " (Run with sudo)" << std::endl;
    }
}

void send_led_command(bool turn_on) {
    std::shared_ptr<vsomeip::message> request = vsomeip::runtime::get()->create_request();
    request->set_service(SERVICE_ID);
    request->set_instance(INSTANCE_ID);
    request->set_method(METHOD_ID);

    std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
    std::vector<vsomeip::byte_t> data;
    data.push_back(turn_on ? 0x01 : 0x00);
    std::copy(client_name.begin(), client_name.end(), std::back_inserter(data));

    payload->set_data(data);
    request->set_payload(payload);

    app->send(request);
    std::cout << "Sent LED Command: " << (turn_on ? "ON" : "OFF") << std::endl;
}

void on_message(const std::shared_ptr<vsomeip::message> &_response) {
    if (_response->get_service() == SERVICE_ID && 
        _response->get_instance() == INSTANCE_ID && 
        _response->get_method() == EVENT_ID) {
        
        std::shared_ptr<vsomeip::payload> pl = _response->get_payload();
        if (pl->get_length() == 0) return;
        
        const vsomeip::byte_t* data = pl->get_data();
        vsomeip::length_t len = pl->get_length();
        
        // Parse state (first byte)
        bool led_state = (data[0] == 0x01);
        
        // Parse source client name (remaining bytes)
        std::string source_client = "Unknown";
        if (len > 1) {
            source_client = std::string(reinterpret_cast<const char*>(data + 1), len - 1);
        }
        
        std::cout << "Received LED State Event: " << (led_state ? "ON" : "OFF") 
                  << " (from: " << source_client << ")" << std::endl;
        
        // Sync local CapsLock LED if the event came from a different client
        if (source_client != client_name) {
            is_syncing = true;  // Set guard to prevent re-triggering
            set_capslock_led(led_state);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));  // Wait for state to settle
            is_syncing = false;
        }
    }
}

/* Handle service availability changes */
void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    std::cout << "Service Status: " << (_is_available ? "AVAILABLE" : "NOT_AVAILABLE") << std::endl;
    
    std::lock_guard<std::mutex> lock(server_available_mutex);
    is_connected = _is_available;
    
    if (_is_available) {
        std::set<vsomeip::eventgroup_t> groups;
        groups.insert(EVENTGROUP_ID);
        app->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, groups, vsomeip::event_type_e::ET_EVENT);
        app->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
    }
    server_available_cv.notify_all();
}

void capslock_monitor_thread() {
    // Wait for server connection
    {
        std::unique_lock<std::mutex> lock(server_available_mutex);
        server_available_cv.wait(lock, []{ return is_connected; });
    }
    
    std::cout << "CapsLock Monitor started. Monitoring: " << capslock_led_path << std::endl;
    
    int last_state = get_capslock_state();
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Skip if we're currently syncing from a remote event
        
        if (is_syncing) {
            last_state = get_capslock_state();
            continue;
        }
        
        int current_state = get_capslock_state();
        
        if (current_state != last_state && current_state >= 0) {
            std::cout << "CapsLock state changed: " << (current_state ? "ON" : "OFF") << std::endl;
            
            // Check if still connected before sending
            std::lock_guard<std::mutex> lock(server_available_mutex);
            if (is_connected) {
                send_led_command(current_state == 1);
            }
            last_state = current_state;
        }
    }
}

void server_status_monitor() {
    bool last_state = false;
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::lock_guard<std::mutex> lock(server_available_mutex);
        if (is_connected && !last_state) {
            std::cout << "[Monitor] Server connected" << std::endl;
        } else if (!is_connected && last_state) {
            std::cout << "[Monitor] Server disconnected" << std::endl;
        }
        last_state = is_connected;
    }
}

int main() {
    // Get client name
    std::cout << "Enter Client Name: ";
    std::cin >> client_name;

    // Dynamically find the CapsLock LED path
    capslock_led_path = find_capslock_led_path();
    if (capslock_led_path.empty()) {
        std::cerr << "Warning: Could not find CapsLock LED path in /sys/class/leds/" << std::endl;
    } else {
        std::cout << "Found CapsLock LED at: " << capslock_led_path << std::endl;
    }

    // Setup vsomeip application
    app = vsomeip::runtime::get()->create_application("capslock_client_app");
    
    if (!app->init()) {
        std::cerr << "Couldn't initialize application" << std::endl;
        return 1;
    }
    
    app->request_service(SERVICE_ID, INSTANCE_ID);
    app->register_availability_handler(SERVICE_ID, INSTANCE_ID, on_availability);
    app->register_message_handler(SERVICE_ID, INSTANCE_ID, EVENT_ID, on_message);

    // Start monitor threads
    std::thread status_monitor(server_status_monitor);
    std::thread capslock_monitor(capslock_monitor_thread);
    status_monitor.detach();
    capslock_monitor.detach();

    // Start vsomeip event loop in a separate thread
    std::thread vsomeip_thread([&]() {
        app->start();
    });
    vsomeip_thread.detach();

    std::cout << "Client started. Press CapsLock to toggle LED..." << std::endl;
    std::cout << "Press Enter to stop." << std::endl;

    // Wait for Enter key to stop
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
    
    running = false;
    app->stop();
    
    std::cout << "Client stopped." << std::endl;
    return 0;
}
