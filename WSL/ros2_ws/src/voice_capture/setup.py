from setuptools import find_packages, setup

package_name = 'voice_capture'

setup(
    name=package_name,
    version='0.0.1',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/voice_capture.launch.py']),
    ],
    install_requires=['setuptools', 'faster-whisper'],
    zip_safe=True,
    maintainer='jetson',
    maintainer_email='you@example.com',
    description='음성 인식으로 지정 문장을 감지해 카메라 프레임을 저장하는 패키지',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'voice_capture_node = voice_capture.voice_capture_node:main',
        ],
    },
)
