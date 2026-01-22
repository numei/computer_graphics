cmake_minimum_required(VERSION 3.11.0)
project(HelloGL VERSION 0.1.0)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Use C++ 17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set source files directory
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src/)

# Try to find packages
if(WIN32)
    find_package(assimp QUIET)
    find_package(OpenAL QUIET)
    # If found via vcpkg, use the imported targets
    if(assimp_FOUND)
        message(STATUS "Found assimp via vcpkg/system")
    endif()
    if(OpenAL_FOUND)
        message(STATUS "Found OpenAL via vcpkg/system")
        # OpenAL might be found as openal-soft, check for include directories
        if(TARGET OpenAL::OpenAL)
            get_target_property(OPENAL_INCLUDE_DIRS OpenAL::OpenAL INTERFACE_INCLUDE_DIRECTORIES)
            if(OPENAL_INCLUDE_DIRS)
                include_directories(${OPENAL_INCLUDE_DIRS})
            endif()
        endif()
        # Also check for openal-soft package
        if(TARGET openal-soft::OpenAL)
            get_target_property(OPENAL_INCLUDE_DIRS openal-soft::OpenAL INTERFACE_INCLUDE_DIRECTORIES)
            if(OPENAL_INCLUDE_DIRS)
                include_directories(${OPENAL_INCLUDE_DIRS})
            endif()
        endif()
    endif()
else()
    find_package(assimp REQUIRED)
    find_package(OpenAL REQUIRED)
    include_directories(${OPENAL_INCLUDE_DIRS})
endif() 
# Add header files
set(HEADER_DIR ${PROJECT_SOURCE_DIR}/include/)
set(LIB_DIR ${PROJECT_SOURCE_DIR}/lib/)
include_directories(${HEADER_DIR} ${LIB_DIR})

# vcpkg toolchain should automatically add vcpkg include directories
# If using vcpkg, the CMAKE_PREFIX_PATH should already include vcpkg's installed directory

# Compile sources
set(SOURCES ${SRC_DIR}/Audio.cpp ${SRC_DIR}/StaticModel.cpp ${SRC_DIR}/glad.c ${SRC_DIR}/TextRenderer.cpp ${SRC_DIR}/UI.cpp  ${SRC_DIR}/Player.cpp ${SRC_DIR}/Game.cpp ${SRC_DIR}/main.cpp)
set(HEADERS ${SRC_DIR}/Audio.h ${SRC_DIR}/StaticModel.h ${SRC_DIR}/Shader.h ${SRC_DIR}/TextRenderer.h ${SRC_DIR}/UI.h ${SRC_DIR}/Player.h ${SRC_DIR}/Game.h)
# set(SOURCES ${SRC_DIR}glad.c ${SRC_DIR}main.cpp)

add_executable(HelloGL ${SOURCES})


# Link libraries (must be after add_executable)
if(WIN32)
    # Windows: Find GLFW library
    # First try CMake's find_package (if using vcpkg) - works with MSVC
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        # Use vcpkg or system-installed GLFW
        if(TARGET glfw)
            target_link_libraries(HelloGL glfw)
            message(STATUS "Using vcpkg/system GLFW (glfw target)")
        elseif(TARGET glfw3)
            target_link_libraries(HelloGL glfw3)
            message(STATUS "Using vcpkg/system GLFW (glfw3 target)")
        else()
            target_link_libraries(HelloGL ${GLFW3_LIBRARIES})
            message(STATUS "Using vcpkg/system GLFW (libraries)")
        endif()
    else()
        # Manually find GLFW in lib folder
        # Check if glfw3.lib exists directly
        if(EXISTS "${LIB_DIR}/glfw3.lib")
            message(STATUS "Using manually installed GLFW")
            set(GLFW_LIBRARY "${LIB_DIR}/glfw3.lib")
            message(STATUS "Found GLFW: ${GLFW_LIBRARY}")
            target_link_libraries(HelloGL ${GLFW_LIBRARY})
            # If DLL exists, copy it to output directory
            if(EXISTS "${LIB_DIR}/glfw3.dll")
                add_custom_command(TARGET HelloGL POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LIB_DIR}/glfw3.dll"
                    $<TARGET_FILE_DIR:HelloGL>)
                message(STATUS "Will copy glfw3.dll to output directory")
            endif()
        else()
            # Try find_library as fallback
            find_library(GLFW_LIBRARY 
                NAMES glfw3 glfw
                PATHS ${LIB_DIR}
                NO_DEFAULT_PATH
            )
            if(GLFW_LIBRARY)
                message(STATUS "Found GLFW: ${GLFW_LIBRARY}")
                target_link_libraries(HelloGL ${GLFW_LIBRARY})
                get_filename_component(GLFW_DLL_PATH "${GLFW_LIBRARY}" DIRECTORY)
                if(EXISTS "${GLFW_DLL_PATH}/glfw3.dll")
                    add_custom_command(TARGET HelloGL POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${GLFW_DLL_PATH}/glfw3.dll"
                        $<TARGET_FILE_DIR:HelloGL>)
                    message(STATUS "Will copy glfw3.dll to output directory")
                endif()
            else()
                message(FATAL_ERROR 
                    "GLFW library not found!\n"
                    "Please install via vcpkg: vcpkg install glfw3:x64-windows\n"
                    "Or download MinGW-compatible GLFW from: https://www.glfw.org/download.html")
            endif()
        endif()
    endif()
    
    # MSVC doesn't need explicit math library linking
    # MinGW would need -lm, but we're using MSVC now
elseif(APPLE)
    # macOS platform
    set(GLFW_LINK ${LIB_DIR}libglfw.3.dylib)
    if(EXISTS ${GLFW_LINK})
        target_link_libraries(HelloGL ${GLFW_LINK})
    else()
        message(FATAL_ERROR "GLFW library not found: ${GLFW_LINK}")
    endif()
else()
    # Linux platform
    find_package(glfw3 REQUIRED)
    target_link_libraries(HelloGL glfw)
endif()

# Link system OpenGL framework
if (APPLE)
    target_link_libraries(HelloGL "-framework OpenGL")
elseif(WIN32)
    # Windows: OpenGL loaded through glad, usually no additional linking needed
    # If needed, can link opengl32
    # target_link_libraries(HelloGL opengl32)
else()
    # Linux
    find_package(OpenGL REQUIRED)
    target_link_libraries(HelloGL OpenGL::GL)
endif()

# Link OpenAL library
if (APPLE)
    target_link_libraries(HelloGL "-framework OpenAL")
elseif(WIN32)
    # Windows: Find OpenAL library
    if(OpenAL_FOUND)
        if(TARGET OpenAL::OpenAL)
            target_link_libraries(HelloGL OpenAL::OpenAL)
            message(STATUS "Linking OpenAL::OpenAL")
        elseif(TARGET openal-soft::OpenAL)
            target_link_libraries(HelloGL openal-soft::OpenAL)
            message(STATUS "Linking openal-soft::OpenAL")
        else()
            target_link_libraries(HelloGL ${OPENAL_LIBRARIES})
            message(STATUS "Linking OpenAL libraries: ${OPENAL_LIBRARIES}")
        endif()
    else()
        # Manually find OpenAL in lib folder
        find_library(OPENAL_LIBRARY
            NAMES OpenAL32 openal
            PATHS ${LIB_DIR}
                  "${LIB_DIR}/OpenAL"
            NO_DEFAULT_PATH
        )
        if(OPENAL_LIBRARY)
            message(STATUS "Found OpenAL: ${OPENAL_LIBRARY}")
            target_link_libraries(HelloGL ${OPENAL_LIBRARY})
            # Check for DLL
            get_filename_component(OPENAL_LIB_DIR "${OPENAL_LIBRARY}" DIRECTORY)
            if(EXISTS "${OPENAL_LIB_DIR}/OpenAL32.dll")
                add_custom_command(TARGET HelloGL POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OPENAL_LIB_DIR}/OpenAL32.dll"
                    $<TARGET_FILE_DIR:HelloGL>)
            endif()
        else()
            message(WARNING "OpenAL library not found in ${LIB_DIR}, linking may fail")
        endif()
    endif()
else()
    find_package(OpenAL REQUIRED)
    target_link_libraries(HelloGL OpenAL::OpenAL)
endif()

# Link assimp library
if (APPLE)
    target_link_libraries(HelloGL ${ASSIMP_LIBRARIES})
elseif(WIN32)
    # Windows: Find assimp library
    if(assimp_FOUND)
        if(TARGET assimp::assimp)
            target_link_libraries(HelloGL assimp::assimp)
            message(STATUS "Linking assimp::assimp")
        else()
            target_link_libraries(HelloGL ${ASSIMP_LIBRARIES})
            message(STATUS "Linking assimp libraries: ${ASSIMP_LIBRARIES}")
        endif()
    else()
        message(FATAL_ERROR "assimp not found! Please install via vcpkg: vcpkg install assimp:x64-windows")
    endif()
else()
    find_package(assimp REQUIRED)
    target_link_libraries(HelloGL ${ASSIMP_LIBRARIES})
endif()

set(RESOURCE_DIRS
    shaders
    assets
)

foreach(dir ${RESOURCE_DIRS})
    add_custom_command(TARGET HelloGL POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/${dir}
        ${CMAKE_CURRENT_BINARY_DIR}/${dir}
    )
endforeach()

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

