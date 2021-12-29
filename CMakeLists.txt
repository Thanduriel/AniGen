cmake_minimum_required (VERSION 3.15)
project (SpriteAniGen)

#---------------------------------------------------------------------------------------
# main project
#---------------------------------------------------------------------------------------

OPTION(WITH_TORCH "Build with alternative map generation. Requires torch." OFF)

# find all source and header files
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "src/*.hpp" "src/*.inl")
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp")

if (NOT WITH_TORCH)
	list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/src/core/optim.cpp")
	list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/src/core/optim.hpp")
endif ()

# organize the files according to the organization in the file system
# from: http:#cmake.3232098.n2.nabble.com/Keep-folder-structure-in-Visual-Studio-Project-generated-with-CMake-td7586044.html
macro(GroupSources curdir)
	file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
	foreach(child ${children})
		if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
			GroupSources(${curdir}/${child})
		else()
			string(REPLACE "/" "\\" groupname ${curdir})
		source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
		endif()
	endforeach()
endmacro()

# execute tha macro
GroupSources(src)

add_library(AniGenLib ${SOURCE_FILES})
set_target_properties(AniGenLib PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
)

add_executable(AniGen "src/main.cpp")
set_target_properties(AniGen PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
)
target_link_libraries(AniGen PRIVATE AniGenLib)

add_executable(Tests "tests/tests.cpp")
set_target_properties(Tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
)
target_link_libraries(Tests PRIVATE AniGenLib)
target_include_directories(Tests PRIVATE "src/")

if (MSVC)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT AniGen)
	# this should be set by vcpkg and can cause issues with libraries
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
#	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:${VCPKG_CRT_LINKAGE},dynamic>:DLL>" CACHE STRING "")
else()
	# public options are propagated to the other targets
	target_compile_options(AniGenLib PUBLIC "$<$<CONFIG:DEBUG>:-Wall;-pedantic>")
	target_compile_options(AniGenLib PUBLIC "$<$<CONFIG:RELEASE>:-Wall;-pedantic;-O3;-march=native;-DNDEBUG>")
endif (MSVC)

#---------------------------------------------------------------------------------------
# libraries
#---------------------------------------------------------------------------------------

# SFML
find_package(SFML COMPONENTS system graphics window QUIET)
if (NOT SFML_FOUND)
	message("SFML not found. Attempting to use local version at dependencies/SFML.")
	set(SFML_BUILD_GRAPHICS TRUE)
	set(SFML_BUILD_WINDOW TRUE)
	set(BUILD_SHARED_LIBS FALSE)
	set(SFML_BUILD_AUDIO FALSE)
	set(SFML_BUILD_EXAMPLES FALSE)
	set(SFML_BUILD_DOC FALSE)
	set(SFML_BUILD_NETWORK FALSE)
	add_subdirectory("dependencies/SFML")
endif(NOT SFML_FOUND)
target_link_libraries(AniGenLib PUBLIC sfml-system sfml-graphics sfml-window)

# arg parsing
find_package(args CONFIG QUIET)
if (NOT args_FOUND)
	message("Args not found. Trying a local version in dependencies/args.")
	target_include_directories(AniGen PRIVATE "dependencies/args")
else()
	target_link_libraries(AniGen PRIVATE taywee::args)
endif(NOT args_FOUND)

if (WITH_TORCH)
	find_package(Torch REQUIRED)
	target_link_libraries(AniGenLib PUBLIC "${TORCH_LIBRARIES}")
	add_compile_definitions(WITH_TORCH)
endif (WITH_TORCH)