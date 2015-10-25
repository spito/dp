
add_library(DIVINE-net client.cpp communicator.cpp daemon.cpp logger.cpp message.cpp)
target_include_directories(DIVINE-net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads)
target_link_libraries(DIVINE-net ${CMAKE_THREAD_LIBS_INIT})
