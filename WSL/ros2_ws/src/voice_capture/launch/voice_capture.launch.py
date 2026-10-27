from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'image_topic',
            default_value='/zed/zed_node/rgb/color/raw/image/compressed'),
        DeclareLaunchArgument('pulse_device', default_value='RDPSource'),
        DeclareLaunchArgument('whisper_model', default_value='small'),
        DeclareLaunchArgument('whisper_device', default_value='cpu'),

        Node(
            package='voice_capture',
            executable='voice_capture_node',
            name='voice_capture_node',
            output='screen',
            parameters=[{
                'image_topic': LaunchConfiguration('image_topic'),
                'pulse_device': LaunchConfiguration('pulse_device'),
                'whisper_model': LaunchConfiguration('whisper_model'),
                'whisper_device': LaunchConfiguration('whisper_device'),
            }],
        ),
    ])
