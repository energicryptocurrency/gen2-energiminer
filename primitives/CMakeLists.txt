file(GLOB SOURCES "*.cpp")
file(GLOB SEADERS "*.h")

add_library(libprimitives ${SOURCES} ${HEADERS})
target_link_libraries(libprimitives PRIVATE jsoncpp_lib_static)
target_include_directories(libprimitives PRIVATE ..)
