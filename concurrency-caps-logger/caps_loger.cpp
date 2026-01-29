#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <atomic>
#include <iomanip>

std::mutex logMutex;
std::condition_variable logCv;
std::queue<std::string> logQueue;
std::atomic<bool> running(true);

std::string getTimeStamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);
    
    std::stringstream ss;
    ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

int getCapsLockState(const std::string& path) {
    std::ifstream file(path);
    int value = 0;
    if (file) {
        file >> value;
    }
    return value;
}

/* Thread 1: Monitor Caps Lock state */
void monitorThread(std::string path) {
    int lastState = getCapsLockState(path);
    
    while (running) {
        int currentState = getCapsLockState(path);
        
        if (currentState != lastState) {
            std::string stateStr = (lastState > 0 ? "[on]" : "[off]");
            stateStr += "->";
            stateStr += (currentState > 0 ? "[on]" : "[off]");
            
            std::string timestamp = getTimeStamp();
            std::string logMsg = timestamp + " : " + stateStr;

            {
                std::lock_guard<std::mutex> lock(logMutex);
                logQueue.push(logMsg);
            }
            logCv.notify_one();
            
            lastState = currentState;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/* Thread 2: Write logs to file */
void loggerThread() {
    std::ofstream logFile("caps_lock_log.txt", std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }

    while (running || !logQueue.empty()) {
        std::unique_lock<std::mutex> lock(logMutex);
        logCv.wait(lock, []{ return !logQueue.empty() || !running; });

        while (!logQueue.empty()) {
            std::string msg = logQueue.front();
            logQueue.pop();
            lock.unlock();
            
            logFile << msg << std::endl;
            std::cout << "Logged: " << msg << std::endl;

            lock.lock();
        }
    }
    logFile.close();
}

int main() {
    std::string capsPath = "/sys/class/leds/input3::capslock/brightness";

    std::cout << "Monitoring Caps Lock at: " << capsPath << std::endl;
    std::cout << "Press Enter to stop..." << std::endl;

    std::thread t1(monitorThread, capsPath);
    std::thread t2(loggerThread);

    std::cin.get();

    running = false;
    logCv.notify_all();

    /* join because monitorThread might be still reading from file : it will read and exit*/
    t1.join();
    /* join because loggerThread might be still writing to file and the queue might not be empty */
    t2.join();

    std::cout << "Logger stopped." << std::endl;
    return 0;
}
