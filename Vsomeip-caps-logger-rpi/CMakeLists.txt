cmake_minimum_required(VERSION 3.10)
project(CapsLockSync)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find vsomeip library
find_package(vsomeip3 REQUIRED)

# --- CONDITIONAL BUILD LOGIC ---
if(CMAKE_CROSSCOMPILING)
    message(STATUS ">> DETECTED: Cross-Compilation (Building for Raspberry Pi)")

    # Raspberry Pi Server executable
    add_executable(Raspberry-Server Rsapberry-Server.cpp)
    target_link_libraries(Raspberry-Server vsomeip3 pthread)

    # Install Server Config
    install(TARGETS Raspberry-Server RUNTIME DESTINATION bin)
    install(FILES vsomeip-server.json DESTINATION etc/vsomeip)

else()
    message(STATUS ">> DETECTED: Native Build (Building for PC)")

    # PC Client executable
    add_executable(PC-SyncClient PC-SyncClient.cpp)
    target_link_libraries(PC-SyncClient vsomeip3 pthread)

    # Install Client Config
    install(TARGETS PC-SyncClient RUNTIME DESTINATION bin)
    install(FILES vsomeip-client.json DESTINATION etc/vsomeip)

endif()