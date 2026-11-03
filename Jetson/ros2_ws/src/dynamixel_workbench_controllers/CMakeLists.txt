################################################################################
# Set minimum required version of cmake, project name and compile options
################################################################################
cmake_minimum_required(VERSION 3.5)
project(dynamixel_workbench_controllers)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

## Compile as C++14, supported in ROS Melodic and newer
#add_compile_options(-std=c++14)

################################################################################
# Find catkin packages and libraries for catkin and system dependencies
################################################################################
#find_package(catkin REQUIRED COMPONENTS
#  roscpp
#  sensor_msgs
#  geometry_msgs
#  trajectory_msgs
#  dynamixel_workbench_msgs
#  dynamixel_workbench_toolbox
#  cmake_modules
#)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(dynamixel_workbench_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
#find_package(trajectory_msgs REQUIRED)
#find_package(dynamixel_workbench_toolbox REQUIRED)

include_directories(include)
include_directories(/home/ncslab/ros2_ws/install/dynamixel_workbench_msgs/include/)
include_directories(/usr/local/include/dynamixel_sdk)

#find_package(Eigen3 REQUIRED)

# Resolve system dependency on yaml-cpp, which apparently does not
# provide a CMake find_package() module.

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)
#find_path(YAML_CPP_INCLUDE_DIR
#  NAMES yaml_cpp.h
#  PATHS ${YAML_CPP_INCLUDE_DIRS}
#)
#find_library(YAML_CPP_LIBRARY
#  NAMES YAML_CPP
#  PATHS ${YAML_CPP_LIBRARY_DIRS}
#)
#link_directories(${YAML_CPP_LIBRARY_DIRS})

#if(NOT ${YAML_CPP_VERSION} VERSION_LESS "0.5")
#add_definitions(-DHAVE_NEW_YAMLCPP)
#endif(NOT ${YAML_CPP_VERSION} VERSION_LESS "0.5")

################################################################################
# Setup for python modules and scripts
################################################################################

################################################################################
# Declare ROS messages, services and actions
################################################################################

################################################################################
## Declare ROS dynamic reconfigure parameters
################################################################################

################################################################################
# Declare catkin specific configuration to be passed to dependent projects
################################################################################
#catkin_package(
# INCLUDE_DIRS include
# CATKIN_DEPENDS roscpp sensor_msgs geometry_msgs dynamixel_workbench_msgs trajectory_msgs dynamixel_workbench_toolbox cmake_modules
# DEPENDS EIGEN3
#)

################################################################################
# Build
################################################################################
#include_directories(
#  include
#  ${catkin_INCLUDE_DIRS}
#  ${EIGEN3_INCLUDE_DIRS}
#  ${YAML_CPP_INCLUDE_DIRS}
#)

#add_executable(dynamixel_workbench_controllers src/dynamixel_workbench_controllers.cpp src/trajectory_generator.cpp)
#add_dependencies(dynamixel_workbench_controllers ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
#target_link_libraries(dynamixel_workbench_controllers ${catkin_LIBRARIES} ${Eigen3_LIBRARIES} ${YAML_CPP_LIBRARIES})

#add_executable(find_dynamixel src/find_dynamixel.cpp)
#add_dependencies(find_dynamixel ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
#target_link_libraries(find_dynamixel ${catkin_LIBRARIES})

add_executable(find_dynamixel src/find_dynamixel.cpp src/dynamixel_driver.cpp src/dynamixel_item.cpp src/dynamixel_tool.cpp src/dynamixel_workbench.cpp)
ament_target_dependencies(find_dynamixel rclcpp)
target_link_libraries(find_dynamixel dxl_x64_cpp)
#target_include_directories(find_dynamixel PRIVATE /usr/local/include/dynamixel_sdk)

add_executable(dynamixel_workbench_controllers src/dynamixel_workbench_controllers.cpp src/dynamixel_driver.cpp src/dynamixel_item.cpp src/dynamixel_tool.cpp src/dynamixel_workbench.cpp)
#ament_target_dependencies(dynamixel_workbench_controllers rclcpp dynamixel_workbench_msgs sensor_msgs trajectory_msgs)
ament_target_dependencies(dynamixel_workbench_controllers rclcpp dynamixel_workbench_msgs sensor_msgs)
target_link_libraries(dynamixel_workbench_controllers dxl_x64_cpp yaml-cpp)
#target_include_directories(dynamixel_workbench_controllers PRIVATE /usr/local/include/dynamixel_sdk)

#################################################################################
## Install
#################################################################################
install(TARGETS
  find_dynamixel
  dynamixel_workbench_controllers
  DESTINATION lib/${PROJECT_NAME})


#install(TARGETS dynamixel_workbench_controllers find_dynamixel
#  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
#)

#install(DIRECTORY include/${PROJECT_NAME}/
#  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
#)

#install(DIRECTORY launch config
#  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
#)

################################################################################
# Test
################################################################################
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()