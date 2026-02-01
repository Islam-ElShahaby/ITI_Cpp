#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <limits>
#include <condition_variable>
#include <vector>

#define SERVICE_ID 0x1234
#define INSTANCE_ID 0x5678
#define METHOD_ID 0x0001
#define EVENT_ID 0x8778
#define EVENTGROUP_ID 0x4465

std::mutex server_avilable_mutex;
std::condition_variable server_avilable_cv;
bool is_connected = false;
std::string client_name;

std::shared_ptr<vsomeip::application> app;

void send_led_command(bool turn_on) {
    /* Create a Request Message */
    std::shared_ptr<vsomeip::message> request = vsomeip::runtime::get()->create_request();
    request->set_service(SERVICE_ID);
    request->set_instance(INSTANCE_ID);
    request->set_method(METHOD_ID);

    /* Create the Payload */
    std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
    std::vector<vsomeip::byte_t> data;
    data.push_back(turn_on ? 0x01 : 0x00);
    std::copy(client_name.begin(), client_name.end(), std::back_inserter(data));

    payload->set_data(data);
    request->set_payload(payload);

    // Send the message
    app->send(request);
    std::cout << "Sent LED Command: " << (turn_on ? "ON" : "OFF") << std::endl;
}

void on_message(const std::shared_ptr<vsomeip::message> &_response) {
  if(_response->get_service() == SERVICE_ID && _response->get_instance() == INSTANCE_ID && _response->get_method() == EVENT_ID) {
      std::shared_ptr<vsomeip::payload> pl = _response->get_payload();
      std::string message = std::string(reinterpret_cast<const char*>(pl->get_data()),0,pl->get_length());
      std::cout << "Received Notification: " << message << std::endl;
  }
}

void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    std::cout << "Service Status changed to: " << (_is_available ? "AVAILABLE" : "NOT_AVAILABLE") << std::endl;
    std::lock_guard<std::mutex> lock(server_avilable_mutex);
    is_connected = _is_available;
    
    if (_is_available) {
        std::set<vsomeip::eventgroup_t> groups;
        groups.insert(EVENTGROUP_ID);
        app->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, groups, vsomeip::event_type_e::ET_EVENT);
        app->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
    }
    server_avilable_cv.notify_all();
}

void server_status_monitor() {
    bool last_state = false;
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::lock_guard<std::mutex> lock(server_avilable_mutex);
        if (is_connected && !last_state) {
            std::cout << "[Monitor] Server connected" << std::endl;
        } else if (!is_connected && last_state) {
             std::cout << "[Monitor] Server disconnected" << std::endl;
        }
        last_state = is_connected;
    }
}

// This thread simulates reading the hardware button
void input_simulation_thread() {
    {
        std::unique_lock<std::mutex> lock(server_avilable_mutex);
        server_avilable_cv.wait(lock, []{ return is_connected; });
    }
    std::cout << "Client Application Running. Type 1 (ON) or 0 (OFF) or any other key to exit..." << std::endl;

    while (true) {
        int user_input;
        if (!(std::cin >> user_input) || user_input < 0 || user_input > 1) {
            std::cout << "Program Stopped" << std::endl;
            app->stop();
            break;
        } 
        if (user_input == 1) {
            send_led_command(true);
        } else {
            send_led_command(false);
        }
    }
}



int main() {
    std::cout << "Enter Client Name: "; 
    std::cin >> client_name;

    /* Setup */
    app = vsomeip::runtime::get()->create_application("button_client_app");
    
    if (!app->init()) {
        std::cerr << "Couldn't initialize application" << std::endl;
        return 0;
    }
    app->request_service(SERVICE_ID, INSTANCE_ID);
    
    app->register_availability_handler(SERVICE_ID, INSTANCE_ID, on_availability);
    app->register_message_handler(SERVICE_ID, INSTANCE_ID, EVENT_ID, on_message);

    /* Start the Monitor Thread */
    std::thread monitor(server_status_monitor);
    monitor.detach();

    /* Start the Input Thread */
    std::thread sender(input_simulation_thread);
    sender.detach();

    /* Start the Main Event Loop */
    app->start();

    return 0;
}

