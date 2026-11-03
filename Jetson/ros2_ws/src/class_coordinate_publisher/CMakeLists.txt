cmake_minimum_required(VERSION 3.8)
project(class_coordinate_publisher)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(publisher src/publisher.cpp)
 
ament_target_dependencies(publisher
  rclcpp
  std_msgs
  geometry_msgs
)
 
target_link_libraries(publisher nlohmann_json::nlohmann_json)

add_executable(subscriber src/subscriber.cpp)
 
ament_target_dependencies(subscriber
  rclcpp
  geometry_msgs
)

install(TARGETS
  publisher
  subscriber
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
