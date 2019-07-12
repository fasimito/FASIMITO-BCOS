file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(storage_benchmark ${SRC_LIST} ${HEADERS})

target_link_libraries(storage_benchmark PUBLIC initializer storage)
