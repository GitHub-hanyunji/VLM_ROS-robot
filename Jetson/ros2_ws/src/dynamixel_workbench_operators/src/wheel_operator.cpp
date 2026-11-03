#if defined(__linux__) || defined(__APPLE__)
  #include <fcntl.h>          // FILE control
  #include <termios.h>        // Terminal IO
#elif defined(_WIN32)
  #include <conio.h>
#endif

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <iostream>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"

#define ESC_ASCII_VALUE             0x1b
#define FORWARD                     0x77
#define BACKWARD                    0x78
#define LEFT                        0x61
#define RIGHT                       0x64
#define STOPS                       0x73
#define STDIN_FILENO 0
int getch(void)
{
  #if defined(__linux__) || defined(__APPLE__)

    struct termios oldt, newt;
    int ch;

    tcgetattr( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 1;
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );

    return ch;

  #elif defined(_WIN32) || defined(_WIN64)
    return _getch();
  #endif
}

int kbhit(void)
{
  #if defined(__linux__) || defined(__APPLE__)
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF)
    {
      ungetc(ch, stdin);
      return 1;
    }
    return 0;
  #elif defined(_WIN32)
    return _kbhit();
  #endif
}

using namespace std::chrono_literals;

class DynamixelWorkbenchOperator : public rclcpp::Node
{
public:
    DynamixelWorkbenchOperator() : Node("dynamixel_workbench_operator"), count_(0),lin_vel_step(0.01), ang_vel_step(0.1)
    {
        auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(10));
        dynamixel_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", qos_profile);
        timer_ = this->create_wall_timer(100ms, std::bind(&DynamixelWorkbenchOperator::publish_velcmd_msg, this));
        std::string msg =
        "\n\
        Control Your Mobile Robot! \n\
        --------------------------- \n\
        Moving around:\n\
                w\n\
          a    s    d\n\
                x\n\
        \n\
        w/x : increase/decrease linear velocity\n\
        a/d : increase/decrease angular velocity\n\
        \n\
        s : force stop\n\
        \n\
        CTRL-C to quit\n\
        ";

        RCLCPP_INFO(this->get_logger(),"%s", msg.c_str());
    }

private:
    void publish_velcmd_msg()
    {
        static auto twist_msg = geometry_msgs::msg::Twist();
        if (kbhit()){
          char c = getch();
          if (c == FORWARD)
          {
            twist_msg.linear.x += lin_vel_step;
          }
          else if (c == BACKWARD)
          {
            twist_msg.linear.x -= lin_vel_step;
          }
          else if (c == LEFT)
          {
            twist_msg.angular.z += ang_vel_step;
          }
          else if (c == RIGHT)
          {
            twist_msg.angular.z -= ang_vel_step;
          }
          else if (c == STOPS)
          {
            twist_msg.linear.x  = 0.0f;
            twist_msg.angular.z = 0.0f;
          }
          else
          {
            twist_msg.linear.x  = twist_msg.linear.x;
            twist_msg.angular.z = twist_msg.angular.z;
          }
        }            
        RCLCPP_INFO(this->get_logger(), "Published message: %lf,%lf", twist_msg.linear.x, twist_msg.angular.z);
        dynamixel_publisher_->publish(twist_msg);
    }       
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr dynamixel_publisher_;
    double lin_vel_step;
    double ang_vel_step;
    size_t count_;
};
int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<DynamixelWorkbenchOperator>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
