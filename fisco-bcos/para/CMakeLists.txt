file(GLOB SRC_LIST "*.cpp")
foreach(file ${SRC_LIST})
    get_filename_component(binrary ${file} NAME_WE)
    add_executable(${binrary} ${file})
    target_link_libraries(${binrary} PUBLIC initializer)
endforeach(file)

 