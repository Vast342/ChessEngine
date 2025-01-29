# initializing stuff
cmake_minimum_required(VERSION 3.11)
project(Clarity VERSION 7.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# all of the main typical files, used for all builds
set(Common_SRC_Files 
src/board.cpp 
src/globals.cpp 
src/move.cpp 
src/movegen.cpp 
src/eval.cpp
src/search.cpp
src/tests.cpp
src/tt.cpp
src/external/fathom/tbprobe.cpp)

# Builds for V1 through V4, each with different featuresets.
add_executable(Clarity_x86-64-v1 ${Common_SRC_Files} src/magic.cpp src/uci.cpp)
add_executable(Clarity_x86-64-v2 ${Common_SRC_Files} src/magic.cpp src/uci.cpp)
add_executable(Clarity_x86-64-v3_Magic ${Common_SRC_Files} src/magic.cpp src/uci.cpp)
add_executable(Clarity_x86-64-v3_BMI2 ${Common_SRC_Files} src/bmi2.cpp src/uci.cpp)
add_executable(Clarity_x86-64-v4 ${Common_SRC_Files} src/bmi2.cpp src/uci.cpp)
add_executable(Clarity_Native ${Common_SRC_Files} src/bmi2.cpp src/uci.cpp)

# data generator build
add_executable(Clarity_Datagen ${Common_SRC_Files} src/bmi2.cpp src/datagen.cpp)

target_compile_options(Clarity_x86-64-v1 PRIVATE -Wall -Wextra -Wpedantic -march=x86-64 -flto)
target_compile_options(Clarity_x86-64-v2 PRIVATE -Wall -Wextra -Wpedantic -march=x86-64-v2 -flto)
target_compile_options(Clarity_x86-64-v3_Magic PRIVATE -Wall -Wextra -Wpedantic -march=x86-64-v3 -flto)
target_compile_options(Clarity_x86-64-v3_BMI2 PRIVATE -Wall -Wextra -Wpedantic -march=x86-64-v3 -flto)
target_compile_options(Clarity_x86-64-v4 PRIVATE -Wall -Wextra -Wpedantic -march=x86-64-v4 -flto)
target_compile_options(Clarity_Native PRIVATE -Wall -Wextra -Wpedantic -march=native -flto)

target_compile_options(Clarity_Datagen PRIVATE -Wall -Wextra -Wpedantic -march=native -flto)

# making the entire binary standalone, for ease of use
target_link_libraries(Clarity_x86-64-v1 -static)
target_link_libraries(Clarity_x86-64-v2 -static)
target_link_libraries(Clarity_x86-64-v3_Magic -static)
target_link_libraries(Clarity_x86-64-v3_BMI2 -static)
target_link_libraries(Clarity_x86-64-v4 -static)
target_link_libraries(Clarity_Native -static)

target_link_libraries(Clarity_Datagen -static)

target_compile_definitions(Clarity_x86-64-v1 PUBLIC NetworkFile="../src/cn_031.nnue")
target_compile_definitions(Clarity_x86-64-v2 PUBLIC NetworkFile="../src/cn_031.nnue")
target_compile_definitions(Clarity_x86-64-v3_Magic PUBLIC NetworkFile="../src/cn_031.nnue")
target_compile_definitions(Clarity_x86-64-v3_BMI2 PUBLIC NetworkFile="../src/cn_031.nnue")
target_compile_definitions(Clarity_x86-64-v4 PUBLIC NetworkFile="../src/cn_031.nnue")
target_compile_definitions(Clarity_Native PUBLIC NetworkFile="../src/cn_031.nnue")

target_compile_definitions(Clarity_Datagen PUBLIC NetworkFile="../src/cn_031.nnue")
