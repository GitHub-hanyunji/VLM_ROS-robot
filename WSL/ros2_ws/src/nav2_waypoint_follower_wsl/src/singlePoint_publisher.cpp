// #include <memory>
// #include <vector>
// #include <fcntl.h>
// #include <termios.h>
// #include <tuple>
// #include <stdlib.h>
// #include <stdio.h>
// #include <iostream>

// #include "rclcpp/rclcpp.hpp"  
// #include "geometry_msgs/msg/point.hpp"
// #include "rclcpp_action/rclcpp_action.hpp"      // Action Client 사용

// #include "nav2_msgs/action/navigate_to_pose.hpp"
// #include "geometry_msgs/msg/pose_stamped.hpp"
// #include "tf2/LinearMath/Quaternion.h"


// class SinglePointClient : public rclcpp::Node
// {
// public:
//   // using
//   using NavigateToPose = nav2_msgs::action::NavigateToPose;
//   using GoalHandleNavigateToPose =rclcpp_action::ClientGoalHandle<NavigateToPose>;


//   SinglePointClient(): Node("single_goal_client")
//   {
//     nav_client_ =rclcpp_action::create_client<NavigateToPose>(this, "navigate_to_pose");
//     coordinate_sub_ =
//         this->create_subscription<geometry_msgs::msg::Point>(
//             "class_coordinates",
//             10,
//             std::bind(
//                 &SinglePointClient::coordinateCallback,
//                 this,
//                 std::placeholders::_1));
//     RCLCPP_INFO(get_logger(), "Waiting for coordinates on 'class_coordinates' topic...");
//     //send_single_goal(-2.0,0.0,-3.13);
//   }

// private:
//   rclcpp::Subscription<geometry_msgs::msg::Point>::SharedPtr coordinate_sub_;
//   rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;

//   bool goal_active_ = false;
//   bool returning_home_ = false;   // 지금 보낸 goal이 "귀환"인지 구분하는 플래그

//   // 홈(초기 위치) 좌표 - 실제 환경에 맞게 수정하세요
//   const double home_x_   = -0.016;
//   const double home_y_   = -0.023;
//   const double home_yaw_ = 3.14;

//   void coordinateCallback(const geometry_msgs::msg::Point::SharedPtr msg)
//   {

//     if (msg->x == 0.0 && msg->y == 0.0 && msg->z == 0.0) {
//       return;
//     }

//     if (goal_active_) {
//       RCLCPP_WARN(
//           get_logger(),
//           "Previous goal is still active. Ignoring new coordinate (%.3f, %.3f, %.3f)",
//           msg->x, msg->y, msg->z);
//       return;
//     }

//     RCLCPP_INFO(
//         get_logger(),
//         "Receive Goal : (%.3f, %.3f, %.3f)",
//         msg->x, msg->y, msg->z);
    
//     returning_home_ = false;
//     send_single_goal(msg->x, msg->y, msg->z);   // z를 yaw로 사용
    
    
//   }
//   void send_single_goal(double x, double y, double yaw)
//   {
//     if (!nav_client_->wait_for_action_server(std::chrono::seconds(5))) {
//         RCLCPP_ERROR(get_logger(), "NavigateToPose server not available");
//         return;
//     } 
//     NavigateToPose::Goal goal;
//     goal.pose.header.frame_id = "map";
//     goal.pose.header.stamp = now();   

//     goal.pose.pose.position.x = x;
//     goal.pose.pose.position.y = y;    

//     tf2::Quaternion q;
//     q.setRPY(0, 0, yaw);
//     goal.pose.pose.orientation.x = q.x();
//     goal.pose.pose.orientation.y = q.y();
//     goal.pose.pose.orientation.z = q.z();
//     goal.pose.pose.orientation.w = q.w(); 

//     auto options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
//     options.result_callback =std::bind(&SinglePointClient::nav_result_callback, this, std::placeholders::_1);   
//     nav_client_->async_send_goal(goal, options);
//     RCLCPP_INFO(get_logger(), "Single goal sent");
//   }

//   void nav_result_callback(const GoalHandleNavigateToPose::WrappedResult & result)
//   {
//     if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
//       RCLCPP_INFO(get_logger(), "Goal reached!");

//       if (!returning_home_) {
//         // 목표 지점 도착 → 홈으로 복귀 시작
//         RCLCPP_INFO(get_logger(), "Returning to home position...");
//         returning_home_ = true;
//         goal_active_ = false;   // send_single_goal 안에서 다시 true로 세팅됨
//         send_single_goal(home_x_, home_y_, home_yaw_);
//         return;   // 아래 goal_active_ = false 를 건너뜀 (홈 goal이 진행중이므로)
//       } else {
//         // 홈 도착 완료
//         RCLCPP_INFO(get_logger(), "Arrived back at home position.");
//       }
//     } else {
//       RCLCPP_WARN(get_logger(), "Navigation failed");
//     }

//     goal_active_ = false;
//   }

// };
// int main(int argc, char ** argv)
// {
//   rclcpp::init(argc, argv);
//   auto node = std::make_shared<SinglePointClient>();
//   rclcpp::spin(node);
//   rclcpp::shutdown();
//   return 0;
// }
#include <memory>
#include <vector>
#include <string>
#include <fcntl.h>
#include <termios.h>
#include <tuple>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>



#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
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

    // 메시지(class_message)를 먼저 구독 -> 최신 메시지를 저장해둠
    message_sub_ =
        this->create_subscription<std_msgs::msg::String>(
            "class_message",
            10,
            std::bind(
                &SinglePointClient::messageCallback,
                this,
                std::placeholders::_1));

    coordinate_sub_ =
        this->create_subscription<geometry_msgs::msg::Point>(
            "class_coordinates",
            10,
            std::bind(
                &SinglePointClient::coordinateCallback,
                this,
                std::placeholders::_1));

    status_pub_ =
        this->create_publisher<std_msgs::msg::String>(
            "navigation_status", 10);
        
    RCLCPP_INFO(get_logger(), "Waiting for coordinates on 'class_coordinates' topic...");
    //send_single_goal(-2.0,0.0,-3.13);
  }

private:
  rclcpp::Subscription<geometry_msgs::msg::Point>::SharedPtr coordinate_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr message_sub_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;

  bool goal_active_ = false;
  bool returning_home_ = false;   // 지금 보낸 goal이 "귀환"인지 구분하는 플래그

  // publisher 쪽에서 class_message가 class_coordinates보다 먼저(또는 거의 동시에)
  // publish된다는 전제 하에, 가장 최근 받은 메시지를 들고 있다가
  // coordinateCallback에서 같이 사용한다.
  std::string last_message_;

  // 홈(초기 위치) 좌표 - 실제 환경에 맞게 수정하세요
  const double home_x_   = -0.016;
  const double home_y_   = -0.023;
  const double home_yaw_ = 3.14;

  void messageCallback(const std_msgs::msg::String::SharedPtr msg)
  {
    last_message_ = msg->data;
    RCLCPP_INFO(get_logger(), "Message received: %s", last_message_.c_str());
  }

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
        "Receive Goal : (%.3f, %.3f, %.3f), message: %s",
        msg->x, msg->y, msg->z, last_message_.c_str());

    returning_home_ = false;
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

    goal_active_ = true;
    auto options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
    options.result_callback =std::bind(&SinglePointClient::nav_result_callback, this, std::placeholders::_1);
    nav_client_->async_send_goal(goal, options);
    RCLCPP_INFO(get_logger(), "Single goal sent");
  }
  void nav_result_callback(const GoalHandleNavigateToPose::WrappedResult & result)
  {
      if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {

          RCLCPP_INFO(get_logger(), "Goal reached!");

          // class_message가 있는 목표인 경우만 Python에 알림
          if (!last_message_.empty()) {

              std_msgs::msg::String status_msg;
              status_msg.data = "arrived|" + last_message_;
              status_pub_->publish(status_msg);

              RCLCPP_INFO(
                get_logger(),
                "Published: %s",
                status_msg.data.c_str());

              // 한 번 사용했으면 비움
              last_message_.clear();
          }

      } else {
          RCLCPP_WARN(get_logger(), "Navigation failed");
      }

      goal_active_ = false;
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