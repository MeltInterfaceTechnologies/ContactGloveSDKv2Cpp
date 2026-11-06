cmake_minimum_required(VERSION 3.20)

option(CONTACTGLOVE_MACOS_UNIVERSAL
    "Build a macOS 13+ Universal Binary package (arm64 and x86_64)"
    OFF)

if(APPLE AND CONTACTGLOVE_MACOS_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
        "Architectures for the macOS Universal Binary package" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
        "Minimum macOS version for the Universal Binary package" FORCE)
endif()

# Let CONTACTGLOVE_BUILD_STATIC select embedded MSVC debug information below.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(ContactGloveSDKv2 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CONTACTGLOVE_OSCPACK_REVISION "3e7e3a102e316d75fe176885a6dd08f216f2eac0")
option(CONTACTGLOVE_BUILD_STATIC
    "Build ContactGloveSDKv2 as a static library that embeds oscpack"
    OFF)
option(CONTACTGLOVE_PACKAGE_DEB
    "Build only the Ubuntu/Debian DEB package for Linux packaging"
    OFF)

if(MSVC)
    add_compile_options(/utf-8)
    # Keep Debug symbols inside the static library so consumers need no
    # separate compiler PDB.
    if(CONTACTGLOVE_BUILD_STATIC)
        if(POLICY CMP0141)
            set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
                "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
        else()
            foreach(_flags_var
                    CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELWITHDEBINFO
                    CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO)
                string(REGEX REPLACE "[/-]Zi( |$)" "/Z7\\1"
                    ${_flags_var} "${${_flags_var}}")
            endforeach()
        endif()
    endif()
endif()

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CONTACTGLOVE_PACKAGE_DEB
        AND UNIX
        AND NOT APPLE
        AND CMAKE_INSTALL_LIBDIR STREQUAL "lib")
    set(CONTACTGLOVE_DEB_MULTIARCH "${CMAKE_LIBRARY_ARCHITECTURE}")
    if(NOT CONTACTGLOVE_DEB_MULTIARCH)
        find_program(DPKG_ARCHITECTURE_EXECUTABLE dpkg-architecture)
        if(DPKG_ARCHITECTURE_EXECUTABLE)
            execute_process(
                COMMAND "${DPKG_ARCHITECTURE_EXECUTABLE}" -qDEB_HOST_MULTIARCH
                OUTPUT_VARIABLE CONTACTGLOVE_DEB_MULTIARCH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()
    endif()
    if(CONTACTGLOVE_DEB_MULTIARCH)
        set(CMAKE_INSTALL_LIBDIR "lib/${CONTACTGLOVE_DEB_MULTIARCH}" CACHE PATH
            "Install directory for libraries" FORCE)
    endif()
endif()

FetchContent_Declare(
    oscpack
    GIT_REPOSITORY https://github.com/RossBencina/oscpack.git
    GIT_TAG        ${CONTACTGLOVE_OSCPACK_REVISION}
)
FetchContent_GetProperties(oscpack)
if(NOT oscpack_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(oscpack)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()

include(cmake/PatchOscpack.cmake)
if(WIN32)
    contactglove_patch_oscpack_socket(win32 OSCPACK_UDP_SOCKET_SOURCE)
else()
    contactglove_patch_oscpack_socket(posix OSCPACK_UDP_SOCKET_SOURCE)
endif()

set(OSCPACK_SOURCES
    ${oscpack_SOURCE_DIR}/osc/OscReceivedElements.cpp
    ${oscpack_SOURCE_DIR}/osc/OscPrintReceivedElements.cpp
    ${oscpack_SOURCE_DIR}/osc/OscOutboundPacketStream.cpp
    ${oscpack_SOURCE_DIR}/osc/OscTypes.cpp
    ${oscpack_SOURCE_DIR}/ip/IpEndpointName.cpp
)
if(WIN32)
    list(APPEND OSCPACK_SOURCES
        ${oscpack_SOURCE_DIR}/ip/win32/NetworkingUtils.cpp
        ${OSCPACK_UDP_SOCKET_SOURCE}
    )
else()
    list(APPEND OSCPACK_SOURCES
        ${oscpack_SOURCE_DIR}/ip/posix/NetworkingUtils.cpp
        ${OSCPACK_UDP_SOCKET_SOURCE}
    )
endif()

add_library(oscpack_internal OBJECT ${OSCPACK_SOURCES})
set_target_properties(oscpack_internal PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(oscpack_internal PUBLIC ${oscpack_SOURCE_DIR})
if(WIN32)
    target_link_libraries(oscpack_internal PUBLIC ws2_32 winmm)
endif()

if(CONTACTGLOVE_BUILD_STATIC)
    set(CONTACTGLOVE_SDK_LIBRARY_TYPE STATIC)
else()
    set(CONTACTGLOVE_SDK_LIBRARY_TYPE SHARED)
endif()

# oscpack objects are compiled into the SDK library in both modes, so a static
# SDK is a single archive that does not export a separate oscpack target.
add_library(ContactGloveSDKv2 ${CONTACTGLOVE_SDK_LIBRARY_TYPE}
    src/ContactGloveSDKv2.cpp
    src/ContactGloveSDKv2C.cpp
    $<TARGET_OBJECTS:oscpack_internal>
)
if(CONTACTGLOVE_BUILD_STATIC)
    # Consumers must not see dllimport declarations for a static library.
    target_compile_definitions(ContactGloveSDKv2 PUBLIC CONTACTGLOVE_SDK_STATIC)
    set_target_properties(ContactGloveSDKv2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    target_compile_definitions(ContactGloveSDKv2 PRIVATE CONTACTGLOVE_SDK_EXPORTS)
endif()
set_target_properties(ContactGloveSDKv2 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
if(WIN32)
    # Debug and Release MSVC runtimes are not ABI compatible, so Windows
    # packages ship both ContactGloveSDKv2.dll and ContactGloveSDKv2d.dll
    # (static builds: ContactGloveSDKv2.lib and ContactGloveSDKv2d.lib).
    set_target_properties(ContactGloveSDKv2 PROPERTIES DEBUG_POSTFIX d)
endif()
target_include_directories(ContactGloveSDKv2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${oscpack_SOURCE_DIR}
)
if(WIN32)
    target_link_libraries(ContactGloveSDKv2 PRIVATE ws2_32 winmm)
endif()
if(UNIX)
    target_link_libraries(ContactGloveSDKv2 PRIVATE pthread)
endif()

add_executable(basic_usage examples/basic_usage.cpp)
set_target_properties(basic_usage PROPERTIES OUTPUT_NAME contactglove-basic-usage)
target_link_libraries(basic_usage PRIVATE ContactGloveSDKv2)

add_executable(c_basic_usage examples/c_basic_usage.c)
set_target_properties(c_basic_usage PROPERTIES
    OUTPUT_NAME contactglove-c-basic-usage
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
target_link_libraries(c_basic_usage PRIVATE ContactGloveSDKv2)

if(APPLE)
    # The SDK archive keeps shared libraries in lib/ and samples in bin/.
    # Keep the samples runnable immediately after extraction without changing
    # the SDK library's @rpath install name for downstream applications.
    set_target_properties(basic_usage c_basic_usage PROPERTIES
        INSTALL_RPATH "@loader_path/../lib"
    )
endif()

# The C sample is linked against a mock, not the SDK, for a socket-free
# concurrent callback/polling regression.
find_package(Threads REQUIRED)
add_executable(c_sample_regression EXCLUDE_FROM_ALL
    examples/c_basic_usage.c tests/c_sample_mock.cpp)
target_compile_definitions(c_sample_regression PRIVATE CONTACTGLOVE_SDK_EXPORTS)
target_link_libraries(c_sample_regression PRIVATE Threads::Threads)
target_include_directories(c_sample_regression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(c_sample_regression PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

add_executable(sender_port_integration EXCLUDE_FROM_ALL tests/sender_port_integration.cpp)
target_link_libraries(sender_port_integration PRIVATE ContactGloveSDKv2)

add_executable(protocol_v2_validation EXCLUDE_FROM_ALL tests/protocol_v2_validation.cpp)
target_link_libraries(protocol_v2_validation PRIVATE ContactGloveSDKv2 oscpack_internal)

# Native runtime regressions remain opt-in like the existing integration tests.
enable_testing()
add_executable(runtime_regression EXCLUDE_FROM_ALL tests/runtime_regression.cpp)
target_link_libraries(runtime_regression PRIVATE ContactGloveSDKv2 oscpack_internal)
foreach(mode pre-run malformed timestamp rapid callbacks)
    add_test(NAME runtime_${mode} COMMAND runtime_regression --${mode})
    set_tests_properties(runtime_${mode} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
endforeach()
add_test(NAME c_sample_regression COMMAND c_sample_regression)
set_tests_properties(c_sample_regression PROPERTIES TIMEOUT 20 RUN_SERIAL TRUE)
add_test(NAME protocol_v2_validation COMMAND protocol_v2_validation)
set_tests_properties(protocol_v2_validation PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)

# ---------------------------------------------------------------------------
# Install and package
# ---------------------------------------------------------------------------
install(TARGETS ContactGloveSDKv2
    EXPORT ContactGloveSDKv2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(MSVC AND NOT CONTACTGLOVE_BUILD_STATIC)
    install(FILES $<TARGET_PDB_FILE:ContactGloveSDKv2>
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        CONFIGURATIONS Debug
        OPTIONAL
    )
endif()

# Multi-config packages ship the Debug SDK library next to the Release one,
# but only one copy of each sample.
if(CMAKE_CONFIGURATION_TYPES)
    set(CONTACTGLOVE_SAMPLE_INSTALL_CONFIGURATIONS
        CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
else()
    set(CONTACTGLOVE_SAMPLE_INSTALL_CONFIGURATIONS)
endif()

install(TARGETS basic_usage
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ${CONTACTGLOVE_SAMPLE_INSTALL_CONFIGURATIONS}
)

install(TARGETS c_basic_usage
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ${CONTACTGLOVE_SAMPLE_INSTALL_CONFIGURATIONS}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ContactGloveSDKv2Targets
    FILE ContactGloveSDKv2Targets.cmake
    NAMESPACE ContactGloveSDKv2::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContactGloveSDKv2
)

configure_package_config_file(
    cmake/ContactGloveSDKv2Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ContactGloveSDKv2Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContactGloveSDKv2
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ContactGloveSDKv2ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ContactGloveSDKv2Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ContactGloveSDKv2ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContactGloveSDKv2
)

set(CONTACTGLOVE_SDK_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME}")
# The two root READMEs are the sole authored guides. Generate layout-specific
# copies with links to files shipped in each install or archive.
configure_file(cmake/CleanupLegacyReadme.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/CleanupLegacyReadme.cmake" @ONLY)
configure_file(cmake/GenerateReadmes.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/GenerateReadmes.cmake" @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/CleanupLegacyReadme.cmake")
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/GenerateReadmes.cmake")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/readmes/install-doc/README.md"
    "${CMAKE_CURRENT_BINARY_DIR}/readmes/install-doc/README_JA.md"
    DESTINATION ${CONTACTGLOVE_SDK_DOCDIR}
)
install(FILES
    api.md
    LICENSE.md
    NOTICE.md
    THIRD_PARTY_NOTICES.md
    DESTINATION ${CONTACTGLOVE_SDK_DOCDIR}
)
install(DIRECTORY bindings/rust/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/rust
    PATTERN "target" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
)
install(DIRECTORY examples/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/examples
    PATTERN "osc_test_server" EXCLUDE
    PATTERN ".venv" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
)

set(CPACK_PACKAGE_NAME "ContactGloveSDKv2Cpp")
set(CPACK_PACKAGE_VENDOR "Melt Interface Technologies Inc.")
set(CPACK_PACKAGE_CONTACT "info@melt-interface-technologies.com")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
    if(CONTACTGLOVE_MACOS_UNIVERSAL)
        set(CPACK_PACKAGE_FILE_NAME
            "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-macOS-universal")
    endif()
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ")
    if(CONTACTGLOVE_PACKAGE_DEB)
        set(CONTACTGLOVE_DEB_DISTRIBUTION ""
            CACHE STRING "Ubuntu distribution codename suffix for Debian package release, such as jammy or noble")
        # DEB mode packages only the filesystem-layout archive; the default
        # Linux configuration remains responsible for the root-README TGZ.
        set(CPACK_GENERATOR "DEB")
        set(CPACK_PACKAGE_NAME "contactglove-sdk-v2")
        set(CPACK_PACKAGE_FILE_NAME
            "contactglove-sdk-v2-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

        set(CPACK_DEBIAN_PACKAGE_NAME "contactglove-sdk-v2")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
        set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
        set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://melt-interface-technologies.com/")
        set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
            "ContactGlove C++/C/Rust SDK V2 native runtime, headers, bindings, examples, and notices.")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
        if(CONTACTGLOVE_DEB_DISTRIBUTION)
            set(CPACK_DEBIAN_PACKAGE_RELEASE "0${CONTACTGLOVE_DEB_DISTRIBUTION}")
        else()
            set(CPACK_DEBIAN_PACKAGE_RELEASE "0")
        endif()
    endif()
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# Archives are consumed directly from their package root, while ordinary
# installs use the project-specific documentation directory above. This
# CPack-only hook runs after staging and therefore does not affect
# `cmake --install` or DESTDIR packaging.
if(WIN32 OR NOT CONTACTGLOVE_PACKAGE_DEB)
    configure_file(
        cmake/CopyPackageReadme.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/CopyPackageReadme.cmake"
        @ONLY
    )
    list(APPEND CPACK_PRE_BUILD_SCRIPTS
        "${CMAKE_CURRENT_BINARY_DIR}/CopyPackageReadme.cmake"
    )
endif()

include(CPack)

# ---------------------------------------------------------------------------
# Doxygen (optional — enable with -DBUILD_DOCS=ON)
# ---------------------------------------------------------------------------
find_package(Doxygen)

option(BUILD_DOCS "Build API documentation with Doxygen" OFF)

if(BUILD_DOCS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR
            "Doxygen was not found. Install it or set BUILD_DOCS=OFF.")
    endif()

    set(DOXYGEN_OUTPUT_DIRECTORY     "${CMAKE_CURRENT_BINARY_DIR}/docs")
    set(DOXYGEN_PROJECT_NAME         "ContactGlove C++/C/Rust SDK V2")
    set(DOXYGEN_PROJECT_NUMBER       "${PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF        "ContactGlove C++/C/Rust SDK V2 for ContactGlove devices via OSC")
    set(DOXYGEN_GENERATE_HTML        NO)
    set(DOXYGEN_GENERATE_LATEX       NO)
    set(DOXYGEN_GENERATE_XML         YES)
    set(DOXYGEN_RECURSIVE            YES)
    set(DOXYGEN_MARKDOWN_SUPPORT     YES)
    set(DOXYGEN_GENERATE_DOCSET      NO)
    set(DOXYGEN_AUTOLINK_SUPPORT     NO)
    set(DOXYGEN_EXTRACT_PRIVATE      NO)
    set(DOXYGEN_STRIP_FROM_INC_PATH  "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)

    if(DOXYGEN_DOT_FOUND)
        set(DOXYGEN_HAVE_DOT         YES)
        set(DOXYGEN_UML_LOOK         YES)
        set(DOXYGEN_COLLABORATION_GRAPH YES)
    endif()

    doxygen_add_docs(docs
        "${CMAKE_CURRENT_SOURCE_DIR}/include/contactglove/ContactGloveSDKv2.hpp"
        COMMENT "Generating API documentation with Doxygen"
    )
    add_custom_command(TARGET docs POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_doxygen_xml.py"
            "${CMAKE_CURRENT_BINARY_DIR}/docs/xml"
        COMMENT "Preserving referenced initializer text for moxygen")
endif()
