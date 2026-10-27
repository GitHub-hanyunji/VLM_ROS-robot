#include <memory>
#include <vector>
#include <fcntl.h>
#include <termios.h>
#include <tuple>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include "rclcpp/rclcpp.hpp"  
#include "geometry_msgs/msg/point.hpp"
#include "rclcpp_action/rclcpp_action.hpp"      // Action Client 사용

#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "tf2/LinearMath/Quaternion.h"


class SinglePointClient : public rclcpp::Node
{
public:
  // using
  using NavigateToPose = nav2_msgs::action::NavigateToPose;
  using GoalHandleNavigateToPose =rclcpp_action::ClientGoalHandle<NavigateToPose>;


  SinglePointClient(): Node("single_goal_client")
  {
    nav_client_ =rclcpp_action::create_client<NavigateToPose>(this, "navigate_to_pose");
    coordinate_sub_ =
        this->create_subscription<geometry_msgs::msg::Point>(
            "class_coordinates",
            10,
            std::bind(
                &SinglePointClient::coordinateCallback,
                this,
                std::placeholders::_1));
    RCLCPP_INFO(get_logger(), "Waiting for coordinates on 'class_coordinates' topic...");
    //send_single_goal(-2.0,0.0,-3.13);
  }

private:
  rclcpp::Subscription<geometry_msgs::msg::Point>::SharedPtr coordinate_sub_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;

  bool goal_active_ = false;
//   double x=-2.0;
//   double y=0.0;
//   double yaw=-3.13;
  void coordinateCallback(const geometry_msgs::msg::Point::SharedPtr msg)
  {

    if (msg->x == 0.0 && msg->y == 0.0 && msg->z == 0.0) {
      return;
    }
  
    if (goal_active_) {
      RCLCPP_WARN(
          get_logger(),
          "Previous goal is still active. Ignoring new coordinate (%.3f, %.3f, %.3f)",
          msg->x, msg->y, msg->z);
      return;
    }

    RCLCPP_INFO(
        get_logger(),
        "Receive Goal : (%.3f, %.3f, %.3f)",
        msg->x, msg->y, msg->z);

    send_single_goal(msg->x, msg->y, msg->z);   // z를 yaw로 사용
  }
  void send_single_goal(double x, double y, double yaw)
  {
    if (!nav_client_->wait_for_action_server(std::chrono::seconds(5))) {
        RCLCPP_ERROR(get_logger(), "NavigateToPose server not available");
        return;
    } 
    NavigateToPose::Goal goal;
    goal.pose.header.frame_id = "map";
    goal.pose.header.stamp = now();   

    goal.pose.pose.position.x = x;
    goal.pose.pose.position.y = y;    

    tf2::Quaternion q;
    q.setRPY(0, 0, yaw);
    goal.pose.pose.orientation.x = q.x();
    goal.pose.pose.orientation.y = q.y();
    goal.pose.pose.orientation.z = q.z();
    goal.pose.pose.orientation.w = q.w(); 

    auto options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
    options.result_callback =std::bind(&SinglePointClient::nav_result_callback, this, std::placeholders::_1);   
    nav_client_->async_send_goal(goal, options);
    RCLCPP_INFO(get_logger(), "Single goal sent");
  }

  void nav_result_callback(const GoalHandleNavigateToPose::WrappedResult & result)
  {
    if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
      RCLCPP_INFO(get_logger(), "Goal reached!");
    } else {
      RCLCPP_WARN(get_logger(), "Navigation failed");
    }
  }

};
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<SinglePointClient>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
