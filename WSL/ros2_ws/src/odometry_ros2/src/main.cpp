#include <rclcpp/rclcpp.hpp>
#include "odometry_ros2/diff_drive_controller.hpp"
#include <chrono>
#include <memory>
#include <string>
#include<iostream>

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    //rclcpp::executors::SingleThreadedExecutor executor;
    auto diff_drive_controller = std::make_shared<robotis::turtlebot3::DiffDriveController>(0.287, 0.033);
    rclcpp::spin(diff_drive_controller);
    //executor.add_node(diff_drive_controller);
    //executor.spin();
    rclcpp::shutdown();
    return 0;
}