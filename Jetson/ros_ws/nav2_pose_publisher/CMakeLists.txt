cmake_minimum_required(VERSION 3.8)
project(nav2_pose_publisher)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

add_executable(pose_publisher src/pose_publisher.cpp)

ament_target_dependencies(pose_publisher
  rclcpp
  geometry_msgs
  tf2
  tf2_geometry_msgs
)

install(TARGETS
  pose_publisher
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()

