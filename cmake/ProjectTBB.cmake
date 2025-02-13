include(ExternalProject)
set(TBB_LIB_SUFFIX a)
if (APPLE)
    set(ENABLE_STD_LIB cpp0x=1 stdlib=libc++)
else()
    set(ENABLE_STD_LIB cpp0x=1)
endif()

ExternalProject_Add(tbb
    PREFIX ${CMAKE_SOURCE_DIR}/deps
    DOWNLOAD_NO_PROGRESS 1
    DOWNLOAD_NAME tbb_2019_u3.tar.gz
    URL https://github.com/01org/tbb/archive/2019_U3.tar.gz
    URL_HASH SHA256=b2244147bc8159cdd8f06a38afeb42f3237d3fc822555499d7ccfbd4b86f8ece
    BUILD_IN_SOURCE 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    LOG_INSTALL 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make extra_inc=big_iron.inc ${ENABLE_STD_LIB}
    INSTALL_COMMAND bash -c "cp ./build/*_release/*.${TBB_LIB_SUFFIX}* ${CMAKE_SOURCE_DIR}/deps/lib"
)

ExternalProject_Get_Property(tbb SOURCE_DIR)
add_library(TBB STATIC IMPORTED)
set(TBB_INCLUDE_DIR ${SOURCE_DIR}/include)
set(TBB_LIBRARY ${CMAKE_SOURCE_DIR}/deps/lib/libtbb.${TBB_LIB_SUFFIX})
file(MAKE_DIRECTORY ${TBB_INCLUDE_DIR})  # Must exist.
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/lib/)  # Must exist.

set_property(TARGET TBB PROPERTY IMPORTED_LOCATION ${TBB_LIBRARY})
set_property(TARGET TBB PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${TBB_INCLUDE_DIR})
add_dependencies(TBB tbb)
unset(SOURCE_DIR)
