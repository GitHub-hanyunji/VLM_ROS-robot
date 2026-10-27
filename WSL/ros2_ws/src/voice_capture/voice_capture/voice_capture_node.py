#!/usr/bin/env python3
# voice_capture_node.py
#
# 동작:
#   1. 음성은 무한 대기 (parecord로 짧은 구간씩 반복 녹음)
#   2. faster-whisper로 STT 수행
#   3. "이 물건 어디다 둬야해?" 계열 문장이 인식되면 (공백 무시 비교)
#      최신 카메라 프레임을 이미지로 저장
#   4. 3초 쿨타임 적용 (같은 말을 연속으로 해도 중복 저장 방지)
#
# 전제:
#   - parecord로 녹음 가능한 환경 (WSL: RDPSource / Jetson: 실제 마이크 카드)
#   - faster-whisper 설치됨

import os
import time
import uuid
import subprocess
import threading
from datetime import datetime

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage

import cv_bridge
import cv2

from faster_whisper import WhisperModel


# Whisper 인식 결과에서 공백을 제거한 뒤 비교할 키워드들
# (조사/띄어쓰기가 조금씩 달라져도 걸리도록 여러 표현을 등록)
KEYWORDS = [
    "이물건어디다둬야해",
    "이물건을어디다둬야해",
    "이거어디다둬야해",
    "어디다둬야해",
    "어디에둬야해",
]


def normalize(text: str) -> str:
    # 공백/물음표/마침표/느낌표/쉼표 제거 후 비교
    for ch in [' ', '?', '.', ',', '!']:
        text = text.replace(ch, '')
    return text


class VoiceCaptureNode(Node):
    def __init__(self):
        super().__init__('voice_capture_node')

        # ── 파라미터 ──
        self.declare_parameter(
            'image_topic', '/zed/zed_node/rgb/color/raw/image/compressed')
        self.declare_parameter(
            'save_dir', os.path.join(os.path.expanduser('~'), 'zed_captures'))
        self.declare_parameter('record_seconds', 4.0)   # 한 번에 녹음할 길이
        self.declare_parameter('pulse_device', 'RDPSource')  # WSL: RDPSource, Jetson: 실제 카드명
        self.declare_parameter('whisper_model', 'small')     # tiny/base/small/medium/large-v3
        self.declare_parameter('whisper_device', 'cpu')      # WSL: cpu, Jetson: cuda 권장
        self.declare_parameter('cooldown_sec', 3.0)

        self.image_topic = self.get_parameter('image_topic').value
        self.save_dir = self.get_parameter('save_dir').value
        self.record_seconds = float(self.get_parameter('record_seconds').value)
        self.pulse_device = self.get_parameter('pulse_device').value
        whisper_model_name = self.get_parameter('whisper_model').value
        whisper_device = self.get_parameter('whisper_device').value
        self.cooldown_sec = float(self.get_parameter('cooldown_sec').value)

        os.makedirs(self.save_dir, exist_ok=True)

        # ── 카메라 구독 ──
        self.bridge = cv_bridge.CvBridge()
        self.latest_frame = None

        self.subscription = self.create_subscription(
            CompressedImage, self.image_topic, self.image_callback, 10)

        self.get_logger().info(f'구독 토픽: {self.image_topic}')
        self.get_logger().info(f'저장 경로: {self.save_dir}')

        # ── Whisper 모델 로드 (시간 좀 걸림) ──
        self.get_logger().info(
            f'Whisper 모델 로딩 중 (model={whisper_model_name}, device={whisper_device}) ...')
        self.model = WhisperModel(whisper_model_name, device=whisper_device)
        self.get_logger().info('Whisper 모델 로딩 완료')

        self.last_capture_time = 0.0

        # ── 음성 인식 루프 (별도 스레드, ROS2 spin과 무관) ──
        self.running = True
        self.listen_thread = threading.Thread(target=self.listen_loop, daemon=True)
        self.listen_thread.start()

        self.get_logger().info(
            '음성 인식 대기 시작. "이 물건 어디다 둬야해?" 라고 말하면 캡처됩니다.')

    # ── 콜백: 최신 프레임 저장 ──
    def image_callback(self, msg: CompressedImage):
        try:
            self.latest_frame = self.bridge.compressed_imgmsg_to_cv2(
                msg, desired_encoding='bgr8')
        except Exception as e:
            self.get_logger().error(f'이미지 변환 실패: {e}')

    # ── 음성 인식 무한 루프 ──
    def listen_loop(self):
        tmp_wav = f'/tmp/voice_capture_{uuid.uuid4().hex}.wav'
        while self.running and rclpy.ok():
            self.record_audio(tmp_wav)
            text = self.transcribe(tmp_wav)
            if text:
                self.get_logger().info(f'인식된 문장: {text}')
                self.check_keyword(text)

        if os.path.exists(tmp_wav):
            os.remove(tmp_wav)

    # ── parecord로 짧게 녹음 ──
    def record_audio(self, path: str):
        cmd = [
            'parecord',
            f'--device={self.pulse_device}',
            '--rate=16000',
            '--channels=1',
            '--format=s16le',
            '--file-format=wav',
            path,
        ]
        try:
            proc = subprocess.Popen(
                cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(self.record_seconds)
            proc.terminate()
            proc.wait(timeout=2)
        except Exception as e:
            self.get_logger().error(f'녹음 실패: {e}')

    # ── faster-whisper STT ──
    def transcribe(self, path: str) -> str:
        if not os.path.exists(path) or os.path.getsize(path) == 0:
            return ''
        try:
            segments, _ = self.model.transcribe(path, language='ko')
            text = ''.join(seg.text for seg in segments)
            return text.strip()
        except Exception as e:
            self.get_logger().error(f'STT 실패: {e}')
            return ''

    # ── 키워드 검사 + 쿨타임 ──
    def check_keyword(self, text: str):
        normalized = normalize(text)
        if any(k in normalized for k in KEYWORDS):
            now = time.time()
            if now - self.last_capture_time > self.cooldown_sec:
                self.last_capture_time = now
                self.get_logger().info('키워드 감지! 이미지 저장')
                self.save_image()
            else:
                self.get_logger().info('키워드 감지했지만 쿨타임 중이라 무시')

    # ── 이미지 저장 (원본 C++ 코드와 동일한 파일명 규칙) ──
    def save_image(self):
        if self.latest_frame is None:
            self.get_logger().warn('저장할 이미지가 없습니다 (아직 이미지 수신 전)')
            return
    
        filename = os.path.join(self.save_dir, "image.png")
    
        success = cv2.imwrite(filename, self.latest_frame)
    
        if success:
            self.get_logger().info(f'저장됨: {filename}')
        else:
            self.get_logger().error('이미지 저장 실패')

    def destroy_node(self):
        self.running = False
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = VoiceCaptureNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
