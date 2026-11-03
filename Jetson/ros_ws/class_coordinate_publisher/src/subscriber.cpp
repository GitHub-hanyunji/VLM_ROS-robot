#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/point.hpp"

class ClassCoordinateSubscriber : public rclcpp::Node
{
public:
  ClassCoordinateSubscriber()
  : Node("class_coordinate_subscriber")
  {
    subscription_ = this->create_subscription<geometry_msgs::msg::Point>(
      "class_coordinates", 10,
      std::bind(&ClassCoordinateSubscriber::coordinateCallback, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "class_coordinate_subscriber 노드 시작됨");
  }

private:
  void coordinateCallback(const geometry_msgs::msg::Point::SharedPtr msg)
  {
    RCLCPP_INFO(
      this->get_logger(),
      "좌표 수신: x=%.3f, y=%.3f, z=%.3f",
      msg->x, msg->y, msg->z);

    // ── TODO: 여기서 받은 좌표로 실제 동작 수행 ──
    // 예: 로봇 이동 명령 생성, Waypoint 추가, 다른 노드로 전달 등
  }

  rclcpp::Subscription<geometry_msgs::msg::Point>::SharedPtr subscription_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ClassCoordinateSubscriber>());
  rclcpp::shutdown();
  return 0;
}