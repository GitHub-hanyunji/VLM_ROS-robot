#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#define STDIN_FILENO 0

namespace fs = std::filesystem;

int getch(void)
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

bool kbhit(void)
{
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
        return true;
    }
    return false;
}

class ImageSaver : public rclcpp::Node
{
public:
  ImageSaver() : Node("image_saver")
  {
    this->declare_parameter<std::string>(
      "image_topic", "/zed/zed_node/rgb/color/raw/image/compressed");
    this->declare_parameter<std::string>(
      "save_dir", std::string(getenv("HOME")) + "/zed_captures");

    std::string topic = this->get_parameter("image_topic").as_string();
    save_dir_ = this->get_parameter("save_dir").as_string();

    if (!fs::exists(save_dir_)) {
      fs::create_directories(save_dir_);
    }

    subscription_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
      topic, rclcpp::SensorDataQoS(),
      std::bind(&ImageSaver::image_callback, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "구독 토픽: %s", topic.c_str());
    RCLCPP_INFO(this->get_logger(), "저장 경로: %s", save_dir_.c_str());
    RCLCPP_INFO(this->get_logger(), "터미널에서 's' 키: 저장, 'q' 키: 종료");
  }

  void spin_loop()
  {
    rclcpp::Rate rate(30);
    while (rclcpp::ok()) {
      rclcpp::spin_some(this->get_node_base_interface());

      if (kbhit()) {
        int key = getch();
        if (key == 's' || key == 'S') {
          save_image();
        } else if (key == 'q' || key == 'Q') {
          RCLCPP_INFO(this->get_logger(), "종료합니다");
          break;
        }
      }

      rate.sleep();
    }
  }

private:
  void image_callback(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
  {
    try {
      cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, "bgr8");
      latest_frame_ = cv_ptr->image;
    } catch (cv_bridge::Exception & e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge 예외: %s", e.what());
    }
  }

  void save_image()
  {
    if (latest_frame_.empty()) {
      RCLCPP_WARN(this->get_logger(), "저장할 이미지가 없습니다 (아직 이미지 수신 전)");
      return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S")
        << "_" << std::setfill('0') << std::setw(3) << ms.count();

    std::string filename = save_dir_ + "/capture_" + oss.str() + ".png";
    cv::imwrite(filename, latest_frame_);
    RCLCPP_INFO(this->get_logger(), "저장됨: %s", filename.c_str());
  }

  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr subscription_;
  cv::Mat latest_frame_;
  std::string save_dir_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ImageSaver>();
  node->spin_loop();
  rclcpp::shutdown();
  return 0;
}