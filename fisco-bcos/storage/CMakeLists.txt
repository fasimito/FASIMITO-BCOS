file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(leveldb-storage ${SRC_LIST} ${HEADERS})

target_link_libraries(leveldb-storage PUBLIC initializer)
