#include <memory>
#include <thread>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "tf2/LinearMath/Quaternion.h"

using namespace std::chrono_literals;

class Nav2PosePublisher : public rclcpp::Node
{
public:
  Nav2PosePublisher()
  : Node("nav2_pose_publisher_cpp")
  {
    auto qos = rclcpp::QoS(10);

    init_pub_ =
      create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
        "/initialpose", qos);
  }

  void publish_once()
  {
    RCLCPP_INFO(get_logger(), "Waiting for AMCL subscriber...");

    while (rclcpp::ok() && init_pub_->get_subscription_count() == 0)
    {
      rclcpp::spin_some(this->get_node_base_interface());
      std::this_thread::sleep_for(100ms);
    }

    RCLCPP_INFO(get_logger(), "AMCL subscriber connected.");

    geometry_msgs::msg::PoseWithCovarianceStamped msg;

    msg.header.frame_id = "map";

    msg.pose.pose.position.x = 0.0;
    msg.pose.pose.position.y = 0.0;
    msg.pose.pose.position.z = 0.0;

    tf2::Quaternion q;
    q.setRPY(0.0, 0.0, 3.14159);

    msg.pose.pose.orientation.x = q.x();
    msg.pose.pose.orientation.y = q.y();
    msg.pose.pose.orientation.z = q.z();
    msg.pose.pose.orientation.w = q.w();

    msg.pose.covariance[0]  = 0.25;
    msg.pose.covariance[7]  = 0.25;
    msg.pose.covariance[35] = 0.068;

    RCLCPP_INFO(get_logger(), "Publishing initial pose...");

    for (int i = 0; i < 20 && rclcpp::ok(); i++)
    {
      msg.header.stamp = now();

      init_pub_->publish(msg);

      rclcpp::spin_some(this->get_node_base_interface());

      std::this_thread::sleep_for(200ms);
    }

    RCLCPP_INFO(get_logger(), "Initial pose published.");
  }

private:
  rclcpp::Publisher<
      geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr init_pub_;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<Nav2PosePublisher>();

  node->publish_once();

  std::this_thread::sleep_for(1s);

  rclcpp::shutdown();

  return 0;
}