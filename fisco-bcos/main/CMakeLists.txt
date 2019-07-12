file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(fisco-bcos ${SRC_LIST} ${HEADERS})
target_include_directories(fisco-bcos PRIVATE ..)
target_link_libraries(fisco-bcos PUBLIC initializer)
install(TARGETS fisco-bcos DESTINATION bin)
