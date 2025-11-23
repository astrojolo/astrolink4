cmake_minimum_required(VERSION 3.10)
project(indi-astrolink4 VERSION 0.1 LANGUAGES C CXX)

# Use modern CMake settings for the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Allow additional modules to be found in cmake_modules/
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

# Provide standard install directories
include(GNUInstallDirs)

# Find INDI (expects a FindINDI.cmake in cmake_modules/ or a package config)
find_package(INDI REQUIRED)

# Source files
set(INDI_ASTROLINK4_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_astrolink4.cpp
)

# Create target
add_executable(indi_astrolink4 ${INDI_ASTROLINK4_SRCS})

# Include directories for the target (use target properties instead of global include_directories)
target_include_directories(indi_astrolink4 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${INDI_INCLUDE_DIR}
)

# Link against INDIdriver library
target_link_libraries(indi_astrolink4 PRIVATE indidriver)

# Install rules
install(TARGETS indi_astrolink4
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install device XML into INDI data directory if provided by the INDI package
if(DEFINED INDI_DATA_DIR)
    install(FILES indi_astrolink4.xml DESTINATION ${INDI_DATA_DIR})
else()
    # Fallback to share directory if INDI did not provide a data dir
    install(FILES indi_astrolink4.xml DESTINATION ${CMAKE_INSTALL_DATADIR})
endif()