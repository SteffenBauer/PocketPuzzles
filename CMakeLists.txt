cmake_minimum_required(VERSION 3.21)

# Do not pollute the source
if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
    message( FATAL_ERROR [=[

In-source builds not allowed!
Please make a new directory, like (called a build directory)
and run cmake from there.
You may need to remove CMakeCache.txt and CMakeFiles directory.

Example:
rm -rf CMakeCache.txt CMakeFiles ; mkdir -p build ; cd build ; cmake .. ; cmake --build .
]=] )
endif()

# Where to build the targets
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# Include the SDK
include("${CMAKE_TOOLCHAIN_FILE}")

# Where to find pbres
string(REGEX REPLACE "[^/]+$" "pbres" PBRES "${CMAKE_C_COMPILER}")

# Add the standard link directories
link_directories("${PB_LINK_DIRECTORIES}")

# Add the standard include directories
include_directories("${PB_INCLUDE_DIRECTORIES}")
include_directories("${FREETYPE_INCLUDE_DIRS}")

# set the project name
project(SGTPuzzles)

# Get today's date into ${TODAY}
execute_process(
	COMMAND date +%Y-%m-%d
	OUTPUT_VARIABLE TODAY
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get git description into ${GIT_VERSION}
execute_process(
	COMMAND git describe --always --tags
	OUTPUT_VARIABLE GIT_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Add the puzzle's include directories
include_directories(include include/frontend)

# Get all sources
# Note: According to cmake's description this is not
# a recommended way to add source files
aux_source_directory(games gameSrcs)
aux_source_directory(utils utilSrcs)
aux_source_directory(frontend frontendSrcs)

# Generate the icons.c
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/auxsrc/icons.c
	COMMAND mkdir -p "${CMAKE_BINARY_DIR}/auxsrc"
	COMMAND ${PBRES} -c ${CMAKE_BINARY_DIR}/auxsrc/icons.c -4 "${CMAKE_SOURCE_DIR}/icons/*/*.bmp"
)

add_executable(SGTPuzzles.app
	${CMAKE_BINARY_DIR}/auxsrc/icons.c
	${gameSrcs}
	${utilSrcs}
	${frontendSrcs}
)
target_link_libraries(SGTPuzzles.app inkview freetype m)
target_compile_definitions(SGTPuzzles.app
	PUBLIC
		VERSION="${TODAY}-${GIT_VERSION}"
		COMBINED
)