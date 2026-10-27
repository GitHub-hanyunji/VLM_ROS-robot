import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import TimerAction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource

from launch_ros.actions import Node


def generate_launch_description():

    # Navigation2 launch
    nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('turtlebot3_navigation2'),
                'launch',
                'navigation2.launch.py'
            )
        )
    )

    pose_node = Node(
        package='nav2_pose_publisher',
        executable='pose_publisher',
        name='pose_publisher',
        output='screen'
    )

    # 한 개의 goal만 이동
    singlePoint_node = Node(
        package='nav2_waypoint_follower_wsl',
        executable='singlePoint_publisher',
        name='singlePoint_publisher',
        output='screen'
    )

    return LaunchDescription([
        # 먼저 Navigation2 실행
        nav2_launch,

        # 2초 후 초기 위치 설정
        TimerAction(
            period=10.0,
            actions=[pose_node]
        ),

        # 10초 후 Goal 전송
        TimerAction(
            period=20.0,
            actions=[singlePoint_node]
        )
    ])