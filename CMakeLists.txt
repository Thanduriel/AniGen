cmake_minimum_required (VERSION 3.15)
project (SpriteAniGen)

#---------------------------------------------------------------------------------------
# main project
#---------------------------------------------------------------------------------------

# find all source and header files
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "src/*.hpp" "src/*.inl")
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp")

# organize the files according to the organisation in the file system
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
	message("SFML not found. Attempting to use local version.")
	set(SFML_BUILD_GRAPHICS TRUE)
	set(SFML_BUILD_WINDOW TRUE)
	set(BUILD_SHARED_LIBS FALSE)
	set(SFML_BUILD_AUDIO FALSE)
	set(SFML_BUILD_EXAMPLES FALSE)
	set(SFML_BUILD_DOC FALSE)
	set(SFML_BUILD_NETWORK FALSE)
	target_include_directories(AniGenLib PRIVATE "dependencies/SFML/include")
	add_subdirectory("dependencies/SFML")
endif(NOT SFML_FOUND)
target_link_libraries(AniGenLib PUBLIC sfml-system sfml-graphics sfml-window)

# arg parsing
find_package(args CONFIG REQUIRED)
target_link_libraries(AniGen PRIVATE taywee::args)