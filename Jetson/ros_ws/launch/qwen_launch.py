from launch import LaunchDescription
from launch.actions import ExecuteProcess, TimerAction
from launch_ros.actions import Node


def generate_launch_description():

    # 1. Python (conda)
    qwen = ExecuteProcess(
        cmd=[
            "bash",
            "-c",
            """
            source /home/jetson/anaconda3/etc/profile.d/conda.sh
            conda activate stt_whisper
    
            source /opt/ros/humble/setup.bash
            source /home/jetson/ros2_ws/install/setup.bash
    
            python /home/jetson/ros2_ws/src/qwen_voice_capture.py
            """
        ],
        output="screen"
    )

    # 2. publisher.cpp
    publisher = Node(
        package="class_coordinate_publisher",
        executable="publisher",
        name="publisher",
        output="screen"
    )

    return LaunchDescription([
        qwen,

        TimerAction(
            period=5.0,
            actions=[
                publisher
            ]
        )
    ])