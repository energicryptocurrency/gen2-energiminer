cmake_policy(SET CMP0015 NEW)

if (NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__STDC_WANT_LIB_EXT1__=1 -DUSE_SECURE_MEMZERO")
endif()

aux_source_directory(. SRC_LIST)

include_directories(BEFORE ..)

set(EXECUTABLE energiminer)

file(GLOB HEADERS "*.h")

add_executable(${EXECUTABLE} ${SRC_LIST} ${SOURCES} ${HEADERS})

hunter_add_package(CLI11)
find_package(CLI11 CONFIG REQUIRED)

target_link_libraries(${EXECUTABLE} energiminer-buildinfo libjson-rpc-cpp::client poolprotocols libprimitives libcommon libnrgcore hwmon Boost::boost Threads::Threads Boost::system CLI11::CLI11)

include(GNUInstallDirs)
install(TARGETS energiminer DESTINATION ${CMAKE_INSTALL_BINDIR})
if(MSVC)
    install(FILES $<TARGET_PDB_FILE:energiminer> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
endif()



if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set(ENERGIMINER_START start.cmd)
else()
    set(ENERGIMINER_START start.sh)
endif()

install(
    FILES ${CMAKE_SOURCE_DIR}/misc/${ENERGIMINER_START}
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                GROUP_EXECUTE GROUP_READ
                WORLD_EXECUTE WORLD_READ 
    DESTINATION ${CMAKE_INSTALL_BINDIR})
