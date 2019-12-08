cmake_minimum_required(VERSION 3.0)
project(libgig)

if(NOT MSVC)
	message(FATAL_ERROR "Please use configure and make, this cmake file is currently only to generate msvc solution files")
endif()


#configuration options
set(LIBGIG_BUILD_TOOLS ON CACHE BOOL "Build the extra tools")
set(LIBGIG_ENABLE_TESTING ON CACHE BOOL "Build the test cases")
set(LIBGIG_BUILD_SHARED ${BUILD_SHARED_LIBS} CACHE BOOL "build dynamic libraries")

set(BUILD_SHARED_LIBS ${LIBGIG_BUILD_SHARED})

if(LIBGIG_ENABLE_TESTING)
	enable_testing()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#TODO: this is written for vcpkg cppunit install
#if you're adding other platform and package managers it's probably better to put everything in FindCppUnit.cmake
#and replace everything here with a simple find_package(CppUnit)
if(WIN32)
	find_path(CPPUNIT_INCLUDE_DIRS cppunit/TestFixture.h)
	find_library(CPPUNIT_LIBRARIES_DEBUG cppunitd_dll)
	find_library(CPPUNIT_LIBRARIES_RELEASE cppunit_dll)
	mark_as_advanced(CPPUNIT_INCLUDE_DIRS CPPUNIT_LIBRARIES_DEBUG CPPUNIT_LIBRARIES_RELEASE)

	if(CPPUNIT_LIBRARIES_DEBUG AND CPPUNIT_LIBRARIES_RELEASE AND CPPUNIT_INCLUDE_DIRS)
		set(CPPUNIT_FOUND TRUE)
		set(CPPUNIT_LIBRARIES
		$<$<CONFIG:Debug>:${CPPUNIT_LIBRARIES_DEBUG}>
		$<$<CONFIG:RelWithDebInfo>:${CPPUNIT_LIBRARIES_RELEASE}>
		$<$<CONFIG:Release>:${CPPUNIT_LIBRARIES_RELEASE}>
		$<$<CONFIG:MinSizeRel>:${CPPUNIT_LIBRARIES_RELEASE}>)
	else()
		message("cppunit not found. Testcases will not be built.")
	endif()
endif(WIN32)

if(LIBGIG_BUILD_TOOLS)
	find_package(LibSndFile)
endif()

#get version from configure.ac
#have to make a slight detour since I cannot get the cmake regex subgroup to work with file
file(STRINGS configure.ac VERSION_STRINGS REGEX "^m4_define\\(libgig_release.*\\)$")

string(REGEX MATCH "libgig_release_major, ([^)]*)\\)" TEMP ${VERSION_STRINGS})
set(VERSION_MAJOR ${CMAKE_MATCH_1})
string(REGEX MATCH "libgig_release_minor, ([^)]*)\\)" TEMP ${VERSION_STRINGS})
set(VERSION_MINOR ${CMAKE_MATCH_1})
string(REGEX MATCH "libgig_release_build, ([^)]*)\\)" TEMP ${VERSION_STRINGS})
set(VERSION_BUILD ${CMAKE_MATCH_1})

set(PACKAGE_NAME "libgig")

set(LIBGIG_HEADERS
	src/DLS.h
	src/gig.h
	src/RIFF.h
	src/SF.h
	src/Korg.h
	src/Serialization.h)

#libgig core library
add_library(libgig
	src/DLS.cpp
	src/gig.cpp
	src/helper.cpp
	src/Korg.cpp
	src/RIFF.cpp
	src/Serialization.cpp
	src/SF.cpp
	src/typeinfo.cpp)
if(WIN32)
	target_sources(libgig PRIVATE win32/dllmain.cpp)
endif()

set_target_properties(libgig PROPERTIES PUBLIC_HEADER "${LIBGIG_HEADERS}")
target_compile_definitions(libgig PRIVATE PACKAGE="${PACKAGE_NAME}" VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}")
target_include_directories(libgig PUBLIC $<INSTALL_INTERFACE:include>)
if(MSVC)
	target_compile_definitions(libgig PRIVATE NOMINMAX)
endif()
if(WIN32)
	target_link_libraries(libgig Rpcrt4.lib dbghelp.lib)
endif()
if(BUILD_SHARED_LIBS AND MSVC)
	set_target_properties(libgig PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

#libakai library
add_library(libakai src/Akai.cpp)
if(WIN32)
	target_sources(libakai PRIVATE win32/dllmain.cpp)
endif()
set_target_properties(libakai PROPERTIES PUBLIC_HEADER src/Akai.h)
target_include_directories(libakai PUBLIC $<INSTALL_INTERFACE:include>)
target_compile_definitions(libakai PRIVATE PACKAGE="${PACKAGE_NAME}" VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}")

if(MSVC)
	target_compile_definitions(libakai PRIVATE NOMINMAX)
endif()
if(BUILD_SHARED_LIBS AND MSVC)
	set_target_properties(libakai PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
if(WIN32)
	target_link_libraries(libakai Rpcrt4.lib )
endif()

if(CPPUNIT_FOUND AND LIBGIG_ENABLE_TESTING)
	add_subdirectory(src/testcases)
endif()

if(LIBGIG_BUILD_TOOLS)
	add_subdirectory(src/tools)
endif(LIBGIG_BUILD_TOOLS)

#installation
#It also creates a *-config.cmake files so other cmake users can include it more easily into their projects
install(TARGETS libgig EXPORT libgig-config
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
	PUBLIC_HEADER DESTINATION include/libgig)
install(EXPORT libgig-config NAMESPACE libgig:: DESTINATION share/libgig)

install(TARGETS libakai EXPORT libakai-config
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
	PUBLIC_HEADER DESTINATION include/libgig)
install(EXPORT libakai-config NAMESPACE libgig:: DESTINATION share/libgig)

