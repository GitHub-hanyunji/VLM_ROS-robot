/*******************************************************************************
* Copyright 2018 ROBOTIS CO., LTD.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/* Authors: Taehun Lim (Darby) */

//#include <ros/ros.h>
#include "rclcpp/rclcpp.hpp"
#include <dynamixel_workbench_controllers/dynamixel_workbench.h>
//#include "/home/ncslab/ros2_ws/install/dynamixel_workbench_toolbox/include/dynamixel_workbench_toolbox/dynamixel_workbench.h"
#define BAUDRATE_NUM 7

int main(int argc, char *argv[]) 
{
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("example_node");

  std::string port_name = "/dev/ttyUSBMotor";

  /*if (argc < 2)
  {
    printf("Please set '-port_name'arguments for connected Dynamixels\n");
    return 0;
  }
  else
  {
    port_name = argv[1];
  }*/

  DynamixelWorkbench dxl_wb;

  const char *log;
  bool result = false;

  uint8_t scanned_id[100];
  uint8_t dxl_cnt = 0;

  uint32_t baudrate[BAUDRATE_NUM] = {9600, 57600, 115200, 1000000, 2000000, 3000000, 4000000};
  uint8_t range = 253;

  uint8_t index = 0;

  while (index < BAUDRATE_NUM)
  {
    result = dxl_wb.init(port_name.c_str(), baudrate[index], &log);
    if (result == false)
    {
      //ROS_WARN("%s", log);
      RCLCPP_WARN(node->get_logger(),"%s", log);
      //ROS_WARN("Failed to init(%d)", baudrate[index]);
      RCLCPP_WARN(node->get_logger(),"Failed to init(%d)", baudrate[index]);
    }
    else
    {
        //ROS_INFO("Succeed to init(%d)", baudrate[index]);
        RCLCPP_INFO(node->get_logger(),"Succeed to init(%d)", baudrate[index]);

      dxl_cnt = 0;
      for (uint8_t num = 0; num < 100; num++) scanned_id[num] = 0;

      //ROS_INFO("Wait for scanning...");
      RCLCPP_INFO(node->get_logger(),"Wait for scanning...");
      result = dxl_wb.scan(scanned_id, &dxl_cnt, range, &log);
      if (result == false)
      {
        //ROS_WARN("%s", log);
        RCLCPP_WARN(node->get_logger(),"%s", log);
        //ROS_WARN("Failed to scan");
        RCLCPP_WARN(node->get_logger(),"Failed to scan");
      }
      else
      {
        //ROS_INFO("Find %d Dynamixels", dxl_cnt);
        RCLCPP_INFO(node->get_logger(),"Find %d Dynamixels", dxl_cnt);

        for (int cnt = 0; cnt < dxl_cnt; cnt++)
          //ROS_INFO("id : %d, model name : %s", scanned_id[cnt], dxl_wb.getModelName(scanned_id[cnt]));
          RCLCPP_INFO(node->get_logger(),"id : %d, model name : %s", scanned_id[cnt], dxl_wb.getModelName(scanned_id[cnt]));
      }
    }
    
    dxl_wb.~DynamixelWorkbench();
    index++;
  }

  return 0;
}
