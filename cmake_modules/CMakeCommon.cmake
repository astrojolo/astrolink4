include(CheckCCompilerFlag)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if (CMAKE_VERSION VERSION_LESS "3.1")
        # Older CMake: try to add -std flags only if a -std flag is not already present
        if(NOT CMAKE_C_FLAGS MATCHES "-std")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
        endif()
        if(NOT CMAKE_CXX_FLAGS MATCHES "-std")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
        endif()
    else()
        # Prefer CMake-managed language standards for portability
        set(CMAKE_C_STANDARD 99)
        set(CMAKE_C_STANDARD_REQUIRED ON)
        set(CMAKE_C_EXTENSIONS ON)  # keep GNU extensions for C (similar to -std=gnu99)

        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        set(CMAKE_CXX_EXTENSIONS OFF) # use -std=c++17 (no GNU extensions)
    endif()
endif()