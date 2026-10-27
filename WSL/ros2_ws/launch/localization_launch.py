import os

from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
		# rviz 설정 파일 경로
    rviz_config = os.path.join(
        os.getenv('HOME'),
        'ros2_localization.rviz'
    )
    return LaunchDescription([
        # Nav2 Map Server
        Node(
            package='nav2_map_server',
            executable='map_server',
            parameters=[{
                'yaml_filename': '/home/linux/map.yaml'
            }]
        ),
        # AMCL (Adaptive Monte Carlo Localization)
        Node(
            package='nav2_amcl',
            executable='amcl',
            name='amcl',
            output='screen',
            parameters=[{
                'use_sim_time': False,           # 실제 시간 사용
                'scan_topic': 'scan',            # LaserScan 토픽 이름
                'base_frame_id': 'base_link',    # 로봇 기준 프레임
                'odom_frame_id': 'odom',         # 오도메트리 프레임
                'global_frame_id': 'map',        # 전역 지도 프레임
                'set_initial_pose': True,        # RViz에서 초기 위치 설정 허용
            }],
        ),
        # rviz2 실행
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
            arguments=['-d', rviz_config]   # 저장된 RViz 설정 파일 로드
        ),
        
        # Nav2 Lifecycle Manager (Localization 전용)
        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            name='lifecycle_manager_localization',
            output='screen',
            parameters=[{
                'autostart': True,  # launch 시 자동 시작
                # 관리할 lifecycle 노드 목록
                'node_names': ['map_server', 'amcl']
            }]
        ),
    ])