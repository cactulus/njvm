cmake_minimum_required(VERSION 3.23)
project(njvm)

set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED 14)
llvm_map_components_to_libnames(llvm_all ${LLVM_TARGETS_TO_BUILD} Passes ExecutionEngine MCJIT)

add_executable(njvm main.cpp)
target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${LLVM_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${llvm_all})