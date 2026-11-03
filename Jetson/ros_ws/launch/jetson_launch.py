# import os

# from launch import LaunchDescription
# from launch_ros.actions import Node

# def generate_launch_description():
#     urdf_file = os.path.join(
#         os.getenv('HOME'),
#         'turtlebot3_waffle.urdf'
#     )
#     return LaunchDescription([
#         # dynamixel workbench controllers
#         Node(
#             package='dynamixel_workbench_controllers',
#             executable='dynamixel_workbench_controllers',
#             name='dynamixel_controller',
#         ),
#         # dynamixel workbench operators
#         # Node(
#         #     package='dynamixel_workbench_operators',
#         #     executable='dynamixel_workbench_operators',
#         #     name='dynamixel_workbench_operators',
#         #     prefix='xterm -e',   # xterm -e 사용
#         #     output='screen',
#         # ),
#         # odometry
#         Node(
#             package='odometry_ros2',
#             executable='odometry_ros2',
#             name='odometry_node',
#             output='screen'
#         ),
#         # robot_state_publisher
#         Node(
#             package='robot_state_publisher',
#             executable='robot_state_publisher',
#             name='robot_state_publisher',
#             output='screen',
#             arguments=[urdf_file]
#         ),
#         # lidar
#         Node(
#             package='sllidar_ros2',
#             executable='sllidar_node',
#             name='sllidar_node',
#             output='screen',
#             parameters=[{
#                 'channel_type': 'serial',
#                 'serial_port': '/dev/rplidar',
#                 'serial_baudrate': 460800,
#                 'frame_id': 'laser',
#                 'inverted': False,
#                 'angle_compensate': True,
#                 'scan_mode': 'Standard',
#                 'scan_frequency': 10.0
#             }]
#         )
#     ])


import os

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    urdf_file = os.path.join(
        os.getenv('HOME'),
        'turtlebot3_waffle.urdf'
    )

    # ZED launch
    zed_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('zed_wrapper'),
                'launch',
                'zed_camera.launch.py'
            )
        ),
        launch_arguments={
            'camera_model': 'zed2',
            'publish_tf': 'false',
            'publish_map_tf': 'false'
        }.items()
    )

    return LaunchDescription([
        # ZED Camera
        zed_launch,

        # dynamixel workbench controllers
        Node(
            package='dynamixel_workbench_controllers',
            executable='dynamixel_workbench_controllers',
            name='dynamixel_controller',
        ),

        # odometry
        Node(
            package='odometry_ros2',
            executable='odometry_ros2',
            name='odometry_node',
            output='screen'
        ),

        # robot_state_publisher
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            arguments=[urdf_file]
        ),

        # lidar
        Node(
            package='sllidar_ros2',
            executable='sllidar_node',
            name='sllidar_node',
            output='screen',
            parameters=[{
                'channel_type': 'serial',
                'serial_port': '/dev/rplidar',
                'serial_baudrate': 460800,
                'frame_id': 'laser',
                'inverted': False,
                'angle_compensate': True,
                'scan_mode': 'Standard',
                'scan_frequency': 10.0
            }]
        )
    ])