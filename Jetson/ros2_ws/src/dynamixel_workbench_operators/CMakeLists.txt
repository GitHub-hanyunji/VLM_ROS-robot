################################################################################
# Set minimum required version of cmake, project name and compile options
################################################################################

cmake_minimum_required(VERSION 3.5)

project(dynamixel_workbench_operators)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# ===========================
# Use C++17 (ROS2 Humble)
# ===========================
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

################################################################################
# Find packages
################################################################################

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)

include_directories(include)

################################################################################
# Build
################################################################################

add_executable(dynamixel_workbench_operators
  src/wheel_operator.cpp
)

ament_target_dependencies(
  dynamixel_workbench_operators
  rclcpp
  geometry_msgs
)

target_include_directories(dynamixel_workbench_operators PRIVATE
  /usr/local/include/dynamixel_sdk
)

target_link_libraries(dynamixel_workbench_operators
  dxl_x64_cpp
)

################################################################################
# Install
################################################################################

install(TARGETS
  dynamixel_workbench_operators
  DESTINATION lib/${PROJECT_NAME}
)

################################################################################
# Test
################################################################################

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()