cmake_minimum_required(VERSION 3.10)

project(hdrmerge)

if(WIN32 OR APPLE)
    if(NOT(ALGLIB_ROOT))
        include(FetchContent)
        FetchContent_Populate(alglib
            URL "https://www.alglib.net/translator/re/alglib-4.01.0.cpp.gpl.zip"
            URL_HASH SHA512=d080d4f4d5879b9e2a5869c2f1a226df9de1bf436cb3a416d4f1f6e36e346f4075ee4de77cc161bd3d814bfad6604b5309c204f0c7a7a3410eec92a7d6e6ed31
            QUIET
        )
        FetchContent_GetProperties(alglib)
        set(ALGLIB_ROOT "${alglib_SOURCE_DIR}")
    endif()
    message(STATUS "ALGLIB_ROOT = ${ALGLIB_ROOT}")
endif()

add_compile_options(-Wall -Wno-unknown-pragmas)
set(PLATFORM "")

# Required and optional packages
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Generate resources automatically
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED Widgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LibRaw REQUIRED IMPORTED_TARGET libraw libraw_r)
find_package(exiv2 REQUIRED)
find_package(ZLIB REQUIRED)

# Commented-out as it doesn't link
#find_package(Boost 1.46 COMPONENTS unit_test_framework)

if(WIN32 OR APPLE)
    set(ALGLIB_INCLUDE_DIRS ${ALGLIB_ROOT}/src)
    add_library(alglib STATIC
        ${ALGLIB_ROOT}/src/alglibinternal.cpp
        ${ALGLIB_ROOT}/src/alglibmisc.cpp
        ${ALGLIB_ROOT}/src/ap.cpp
        ${ALGLIB_ROOT}/src/dataanalysis.cpp
        ${ALGLIB_ROOT}/src/diffequations.cpp
        ${ALGLIB_ROOT}/src/fasttransforms.cpp
        ${ALGLIB_ROOT}/src/integration.cpp
        ${ALGLIB_ROOT}/src/interpolation.cpp
        ${ALGLIB_ROOT}/src/linalg.cpp
        ${ALGLIB_ROOT}/src/optimization.cpp
        ${ALGLIB_ROOT}/src/solvers.cpp
        ${ALGLIB_ROOT}/src/specialfunctions.cpp
        ${ALGLIB_ROOT}/src/statistics.cpp
    )
    target_compile_definitions(alglib PUBLIC AE_COMPILE_SPLINE1D)
else()
    find_package(ALGLIB REQUIRED)
endif()

include_directories(
    "${ZLIB_INCLUDE_DIRS}"
    "${ALGLIB_INCLUDE_DIRS}"
)

if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    find_package(OpenMP)
    if(CMAKE_COMPILER_IS_GNUCC)
        set(STRIP -s)
    endif()
endif()

# The version number.
set(HDRMERGE_VERSION_MAJOR 0)
set(HDRMERGE_VERSION_MINOR 5)
set(HDRMERGE_VERSION_REV 0)
set(HDRMERGE_VERSION ${HDRMERGE_VERSION_MAJOR}.${HDRMERGE_VERSION_MINOR}.${HDRMERGE_VERSION_REV})

# configure a header file to pass some of the CMake settings to the source code
configure_file(
    "${PROJECT_SOURCE_DIR}/src/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)

# add the binary tree to the search path for include files so that we will find config.h
include_directories("${PROJECT_BINARY_DIR}")

set(PLATFORM_SOURCES "")
if(WIN32)
    # This is required by libraw.h
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32 -DLIBRAW_NODLL")
    set(PLATFORM WIN32)
    configure_file(
        "${PROJECT_SOURCE_DIR}/data/winres.rc"
        "${PROJECT_BINARY_DIR}/winres.rc"
    )
    set(PLATFORM_SOURCES "${PROJECT_BINARY_DIR}/winres.rc")
elseif(APPLE)
    set(PLATFORM MACOSX_BUNDLE)
    set(MACOSX_BUNDLE_ICON_FILE "icon")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.hdrmerge")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "HDRMerge for macOS v${HDRMERGE_VERSION}")
    set(MACOSX_BUNDLE_BUNDLE_NAME "HDRMerge")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${HDRMERGE_VERSION}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${HDRMERGE_VERSION}")
    set(MACOSX_BUNDLE_INFO_COPYRIGHT "Copyright 2018 Javier Celaya")
    set(PLATFORM_SOURCES "data/images/icon.icns")
endif()

# Sources and headers
set(hdrmerge_sources
    src/Image.cpp
    src/ImageStack.cpp
    src/Bitmap.cpp
    src/RawParameters.cpp
    src/EditableMask.cpp
    src/DngFloatWriter.cpp
    src/TiffDirectory.cpp
    src/BoxBlur.cpp
    src/ExifTransfer.cpp
    src/ImageIO.cpp
)

set(hdrmerge_gui_sources
    src/AboutDialog.cpp
    src/MainWindow.cpp
    src/PreviewWidget.cpp
    src/DraggableScrollArea.cpp
    src/DngPropertiesDialog.cpp
    src/LoadOptionsDialog.cpp
    src/FileSystem.cpp
)

set(hdrmerge_qobject_headers
    src/MainWindow.hpp
    src/PreviewWidget.hpp
    src/LoadOptionsDialog.hpp
    src/DraggableScrollArea.hpp
    src/DngPropertiesDialog.hpp
    src/AboutDialog.hpp
    src/FileSystem.hpp
)

set(hdrmerge_translations
    data/hdrmerge_es.ts
)

# Libs
set(hdrmerge_libs
    PkgConfig::LibRaw
    exiv2lib
    "${ZLIB_LIBRARIES}"
)

if(WIN32)
    find_library(EXPAT_LIBRARY libexpat.a)
    find_library(ICONV_LIBRARY libiconv.a)
    find_library(GETTEXT_LIBRARY libintl.a)
    set(hdrmerge_libs
        ${hdrmerge_libs}
        "${EXPAT_LIBRARY}"
        "${GETTEXT_LIBRARY}"
        "${ICONV_LIBRARY}"
        psapi
        wsock32
        imm32
        winmm
    )
else()
    set(hdrmerge_libs
        "${hdrmerge_libs}"
        "${ALGLIB_LIB}"
    )
endif()

#QT4_ADD_TRANSLATION(hdrmerge_qm ${hdrmerge_translations})

# Generate the XML version of hdrmerge_qm
foreach(file ${hdrmerge_qm})
    get_filename_component(basename ${file} NAME)
    set(HDRMERGE_QM_XML "${HDRMERGE_QM_XML}
    <file>${basename}</file>")
endforeach()

configure_file(
    "${PROJECT_SOURCE_DIR}/data/translations.qrc.in"
    "${PROJECT_BINARY_DIR}/translations.qrc"
)

add_executable(hdrmerge ${PLATFORM}
    src/main.cpp
    src/Launcher.cpp
    data/resources.qrc
    ${hdrmerge_translations}
    ${hdrmerge_sources}
    ${hdrmerge_gui_sources}
    ${hdrmerge_moc}
    "${PLATFORM_SOURCES}"
)
if(WIN32 OR APPLE)
    target_link_libraries(hdrmerge alglib)
endif()

target_link_libraries(hdrmerge ${STRIP} ${hdrmerge_libs} Qt6::Widgets)
if(OpenMP_FOUND)
    target_link_libraries(hdrmerge OpenMP::OpenMP_CXX)
endif()

if(WIN32)
    # Compile a target without GUI, for the .com executable
    add_executable(hdrmerge-nogui
        src/main.cpp
        src/Launcher.cpp
        data/resources.qrc
        ${hdrmerge_translations}
        ${hdrmerge_sources}
        ${hdrmerge_moc}
        "${PLATFORM_SOURCES}"
    )
    target_link_libraries(hdrmerge-nogui alglib)
    if(OpenMP_FOUND)
        target_link_libraries(hdrmerge-nogui OpenMP::OpenMP_CXX)
    endif()
    
    # WARNING: ImageIO class need QImage which need Qt6::Widgets
    #          Launcher.cpp need QApplication which need Qt6::Widgets
    target_link_libraries(hdrmerge-nogui ${STRIP} ${hdrmerge_libs} Qt6::Widgets)
    target_compile_definitions(hdrmerge-nogui PRIVATE "NO_GUI")
    # Create the installer with makensis
    find_program(MAKENSIS_EXECUTABLE makensis.exe PATH_SUFFIXES "NSIS/Bin")
    if(MAKENSIS_EXECUTABLE)
        message(STATUS "NSIS found")
        set(SETUP_PROG hdrmerge-setup${WIN_ARCH}-${HDRMERGE_VERSION}.exe)
        string(REPLACE "/" "\\" PROJ_SRC_DIR "${PROJECT_SOURCE_DIR}")
        message(STATUS "PROJ_SRC_DIR       = ${PROJ_SRC_DIR}")

        # ASSUMING YOU'RE USING EXIV2 BUNDLED WITH MSYS2
        find_program(MINGW_LIB_DIR exiv2.exe)
        if(MINGW_LIB_DIR)
            get_filename_component(MINGW_LIB_DIR "${MINGW_LIB_DIR}" PATH)
            string(REPLACE "/" "\\" MINGW_LIB_DIR "${MINGW_LIB_DIR}")
            message(STATUS "MINGW_LIB_DIR      = ${MINGW_LIB_DIR}")

            set(QT6_PLUGINS_DIR "${MINGW_LIB_DIR}\\..\\share\\qt6\\plugins")
            message(STATUS "QT6_PLUGINS_DIR    = ${QT6_PLUGINS_DIR}")

            configure_file(
                "${PROJECT_SOURCE_DIR}/data/setup.nsi"
                "${PROJECT_BINARY_DIR}/setup.nsi" @ONLY
            )
            add_custom_command(OUTPUT ${SETUP_PROG}
                COMMAND "${MAKENSIS_EXECUTABLE}" -V2 setup.nsi
                MAIN_DEPENDENCY setup.nsi
                DEPENDS "${PROJECT_BINARY_DIR}/hdrmerge.exe"
                WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
            add_custom_target(hdrmerge-setup ALL DEPENDS "${PROJECT_BINARY_DIR}/${SETUP_PROG}")
        else(MINGW_LIB_DIR)
            message(STATUS "Unable to locate the 'bin' directory to get the dependencies")
        endif(MINGW_LIB_DIR)
    else()
        message(STATUS "NSIS not found")
    endif()
elseif(APPLE)
    set_source_files_properties(data/images/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    install(TARGETS hdrmerge BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    if(NOT DEFINED CMAKE_INSTALL_BINDIR)
        set(CMAKE_INSTALL_BINDIR "bin")
    endif()
    message(STATUS "Install binary to: " ${CMAKE_INSTALL_BINDIR})
    install(TARGETS hdrmerge DESTINATION ${CMAKE_INSTALL_BINDIR})
    find_program(XDG-DESKTOP-MENU_EXECUTABLE xdg-desktop-menu)
    find_program(XDG-ICON-RESOURCE_EXECUTABLE xdg-icon-resource)
    if(XDG-DESKTOP-MENU_EXECUTABLE AND XDG-ICON-RESOURCE_EXECUTABLE)
        configure_file(
            "${PROJECT_SOURCE_DIR}/data/hdrmerge.desktop"
            "${PROJECT_BINARY_DIR}/hdrmerge.desktop"
        )
        install(CODE "
            execute_process(COMMAND \"${XDG-ICON-RESOURCE_EXECUTABLE}\" install --novendor --size 128 \"${PROJECT_SOURCE_DIR}/data/images/icon.png\" hdrmerge-icon)
            execute_process(COMMAND \"${XDG-DESKTOP-MENU_EXECUTABLE}\" install --novendor \"${PROJECT_BINARY_DIR}/hdrmerge.desktop\")"
        )
    endif()
endif()

if(Boost_FOUND)
    add_subdirectory(test)
endif()

