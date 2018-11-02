cmake_minimum_required(VERSION 3.5)

include(cmake/cable/bootstrap.cmake)

include(CableBuildInfo)
include(CableBuildType)
include(HunterGate)
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.21.9.tar.gz"
    SHA1 "0056188988906abb63a06c6f1aaef01832c62b74"
    LOCAL
)

if(HUNTER_ENABLED)
	find_package(PythonInterp)
	if(PYTHONINTERP_FOUND)
		hunter_gate_self(
			"${HUNTER_CACHED_ROOT}"
			"${HUNTER_VERSION}"
			"${HUNTER_SHA1}"
			hunter_dir
		)
		set(hunter_upload_script "${hunter_dir}/maintenance/upload-cache-to-github.py")
		set(hunter_cache_dir "${HUNTER_GATE_ROOT}/_Base/Cache")
		set(hunter_tmp_dir "${HUNTER_GATE_ROOT}/tmp")
		add_custom_target(
			hunter_upload_cache
			${PYTHON_EXECUTABLE} ${hunter_upload_script}
			--username hunter-cache-bot
			--repo-owner ethereum
			--repo hunter-cache
			--cache-dir ${hunter_cache_dir}
			--temp-dir ${hunter_tmp_dir}
		)
	endif()
endif()


project(energiminer
        VERSION "2.0.0" # AUTO-REPLACE
        )

# link_directories interprate relative paths with respect to CMAKE_CURRENT_SOURCE_DIR
cmake_policy(SET CMP0015 NEW)

# let cmake autolink dependencies on windows
# it's specified globally, cause qt libraries requires that on windows and they are also found globally
cmake_policy(SET CMP0020 NEW)

# 3.1 and above
cmake_policy(SET CMP0054 NEW)

cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Release RelWithDebInfo)
######################################################################################################

option(HASHCL "Build with OpenCL mining" ON)
option(HASHCUDA "Build with CUDA mining" OFF)

# propagates CMake configuration options to the compiler
function(configureProject)
    if (HASHCL)
        add_definitions(-DNRGHASHCL)
    endif()
	if (HASHCUDA)
        add_definitions(-DNRGHASHCUDA)
	endif()
endfunction()



hunter_add_package(Boost COMPONENTS system filesystem)
find_package(Boost CONFIG REQUIRED COMPONENTS system filesystem)

hunter_add_package(jsoncpp)
find_package(jsoncpp CONFIG REQUIRED)

hunter_add_package(libjson-rpc-cpp)
find_package(libjson-rpc-cpp CONFIG REQUIRED)

configureProject()

message("------------------------------------------------------------------------")
message("-- CMake ${CMAKE_VERSION}")
message("-- Build ${CMAKE_BUILD_TYPE} / ${CMAKE_SYSTEM_NAME}")
message("------------------------------------------------------------- components")
message("-- HASHCL         Build OpenCL components                  ${HASHCL}")
message("-- HASHCUDA       Build CUDA components                    ${HASHCUDA}")
message("------------------------------------------------------------------------")
message("")

include(EthCompilerSettings)

cable_add_buildinfo_library(PROJECT_NAME ${PROJECT_NAME})

add_subdirectory(protocol)
add_subdirectory(primitives)
add_subdirectory(common)
add_subdirectory(libhwmon)
add_subdirectory(nrghash)
add_subdirectory(nrgcore)

if (HASHCL)
    add_subdirectory(libegihash-cl)
endif()
if (HASHCUDA)
    add_subdirectory(libnrghash-cuda)
endif()
add_subdirectory(energiminer)


if(WIN32)
	set(CPACK_GENERATOR ZIP)
else()
	set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME})
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
include(CPack)


