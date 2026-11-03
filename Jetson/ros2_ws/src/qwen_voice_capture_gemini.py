# qwen_voice_gemini_live.py
#
# 기존 faster-whisper(STT) + gTTS(TTS) 를 Gemini Live API 로 교체한 버전.
#   - STT: gemini-3.5-transcribe-live  (전용 실시간 전사 모델, 답변 안 함 / 텍스트만)
#   - TTS: gemini-3.8-live             (네이티브 오디오, 받은 문장을 그대로 읽게 함)
# 키워드 판단 -> 카메라 캡처 -> vLLM(Qwen) 호출 -> 확인 질문 -> UDP 전송 흐름은 그대로.
#
# 설치:
#   pip install google-genai requests numpy
#   (rclpy, opencv 는 ROS2 환경 것 사용 / faster-whisper, gTTS, mpg123 는 더 이상 필요 없음)
#   재생: pulseaudio-utils 의 pacat 사용 (paplay 가 되던 환경이면 이미 있음)
#
# 실행 전:
#   export GEMINI_API_KEY="..."
#   vllm_server.py 가 먼저 떠 있어야 함

import os
import re
import socket
import asyncio
import subprocess
import threading

import requests
import numpy as np
import cv2

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage
from std_msgs.msg import String

from google import genai
from google.genai import types


# ─────────────────────────────── 설정 ───────────────────────────────

VLLM_SERVER_URL = "http://localhost:8000/infer"

CLASS_PUBLISHER_HOST = "127.0.0.1"
CLASS_PUBLISHER_PORT = 6001

# Gemini Live 모델
STT_MODEL = "gemini-3.5-transcribe-live"
TTS_MODEL = "gemini-3.8-live"
TTS_VOICE = "Kore"

# 전사 정확도를 올리고 싶은 단어 (구역 이름, 물건 이름 등 추가)
STT_CUSTOM_VOCAB = ["어디다 놔야 해", "어디에 둬야 해", "구역", "안내해줘", "케이블타이"]

# transcribe-live 세션은 최대 10분 -> 그 전에 스스로 재연결
STT_SESSION_SEC = 9 * 60

# 마이크 (arecord 로 raw PCM 스트리밍)
# MIC_DEVICE = "hw:0,0"            # arecord -l 로 확인한 카드,디바이스
# MIC_DEVICE = "plughw:0,0"
MIC_DEVICE = "plughw:CARD=Set,DEV=0"
MIC_RATE = 16000
CHUNK_BYTES = 3200               # 100ms (16000Hz * 2byte * 0.1s)

# 스피커 (Gemini 출력은 항상 24kHz 16bit mono PCM)
# PLAYER_CMD = ["pacat", "--playback", "--raw",
#               "--rate=24000", "--channels=1", "--format=s16le"]
# 스피커 (USB Audio Device: card 0, device 0)
PLAYER_CMD = [
    "aplay", "-q",
    "-D", "plughw:0,0",
    "-t", "raw",
    "-f", "S16_LE",
    "-r", "24000",
    "-c", "1"
]
# PLAYER_CMD = ["aplay", "-q", "-t", "raw", "-f", "S16_LE", "-r", "24000", "-c", "1"]

TTS_TIMEOUT_SEC = 30
TTS_TAIL_SEC = 0.4               # TTS 끝난 뒤 잔향이 마이크로 들어가지 않게 잠깐 대기

IMAGE_SAVE_PATH = os.path.join(os.path.expanduser("~"), "zed_captures", "image.png")
SOUND_DIR = "/usr/share/sounds/freedesktop/stereo"


# ─────────────────────────────── 키워드 ───────────────────────────────

KEYWORDS = [
    "이거어디다놔야해", "이물건어디다놔야해", "이물건을어디다놔야해",
    "이거어디다둬야해", "이물건어디다둬야해", "어디다놔야해", "어디다둬야해",
    "어디에놔야해", "어디에둬야해", "이거어디에놔요", "어디", "어디에다놔",
    "이거어디에다놓아야해", "이거어디",
]

YES_KEYWORDS = [
    "예", "네", "응", "그래", "좋아", "좋습니다", "가자", "안내해줘",
    "좋은데", "고고", "렛츠고", "안내해", "응그래",
]

NO_KEYWORDS = [
    "아니", "아니요", "아니야", "싫어", "싫어요", "괜찮아", "괜찮아요",
    "필요없어", "안내하지마", "집에가", "응아니야",
]


def normalize(text: str) -> str:
    for ch in [' ', '?', '.', ',', '!']:
        text = text.replace(ch, '')
    return text


def is_yes(text: str) -> bool:
    n = normalize(text)
    return any(k in n for k in YES_KEYWORDS)


def is_no(text: str) -> bool:
    n = normalize(text)
    return any(k in n for k in NO_KEYWORDS)


def is_target_phrase(text: str) -> bool:
    n = normalize(text)
    return any(k in n for k in KEYWORDS)


# ─────────────────────────────── 기존 유틸 (변경 없음) ───────────────────────────────

def play_ready_sound(sound_name="complete.oga"):
    sound_path = os.path.join(SOUND_DIR, sound_name)
    if not os.path.exists(sound_path):
        print(f"효과음이 없습니다: {sound_path}")
        return
    subprocess.run(["paplay", sound_path],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def save_current_frame(camera_node) -> bool:
    if camera_node.latest_frame is None:
        print("저장할 이미지가 없습니다 (아직 카메라 프레임 수신 전)")
        return False
    os.makedirs(os.path.dirname(IMAGE_SAVE_PATH), exist_ok=True)
    cv2.imwrite(IMAGE_SAVE_PATH, camera_node.latest_frame)
    print(f"이미지 저장됨: {IMAGE_SAVE_PATH}")
    return True


def call_vllm_server(image_path: str, text: str) -> str:
    try:
        payload = {"image_path": image_path, "text": text}
        resp = requests.post(VLLM_SERVER_URL, json=payload, timeout=60)
        resp.raise_for_status()
        return resp.json().get("answer", "")
    except Exception as e:
        print(f"vLLM 서버 호출 실패: {e}")
        return ""


def extract_class_name(answer: str) -> str:
    m = re.search(r"\[클래스[:：]\s*([^\]]+)\]", answer)
    return m.group(1).strip() if m else ""


def extract_destination(answer: str) -> str:
    m = re.search(r"([A-Za-z가-힣]+구역)", answer)
    return m.group(1).strip() if m else ""


def extract_tts_text(answer: str) -> str:
    m = re.search(r"\[클래스[:：].*?\]\s*(.*)", answer, re.DOTALL)
    if m:
        return m.group(1).strip() + " 안내해 드릴까요?"
    return ""


def send_class_name(class_name: str):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(class_name.encode("utf-8"),
                    (CLASS_PUBLISHER_HOST, CLASS_PUBLISHER_PORT))
        sock.close()
        print(f"[UDP 전송] '{class_name}' -> {CLASS_PUBLISHER_HOST}:{CLASS_PUBLISHER_PORT}")
    except Exception as e:
        print(f"UDP 전송 실패: {e}")


# ─────────────────────────────── Gemini Live 음성 I/O ───────────────────────────────

class GeminiVoice:
    """
    마이크 -> transcribe-live 로 계속 스트리밍해서 확정 문장을 text_q 에 넣고,
    speak() 호출 시 Live 모델로 문장을 읽게 해서 스피커로 재생.
    TTS 재생 중에는 마이크 전송을 멈춰 자기 목소리를 인식하지 않게 함.
    """

    def __init__(self):
        self.client = genai.Client()   # GEMINI_API_KEY 환경변수 사용
        self.loop: asyncio.AbstractEventLoop | None = None

        self.audio_q: asyncio.Queue[bytes] = asyncio.Queue(maxsize=50)
        self.text_q: asyncio.Queue[str] = asyncio.Queue()
        self.mic_muted = False
        self._stream_ended = False
        self.tts_lock = asyncio.Lock()

        self.stt_config = types.LiveConnectConfig(
            response_modalities=["TEXT"],
            input_audio_transcription=types.AudioTranscriptionConfig(
                language_codes=["ko-KR"],
                custom_vocabulary=STT_CUSTOM_VOCAB,
            ),
        )

        self.tts_config = types.LiveConnectConfig(
            response_modalities=["AUDIO"],
            speech_config=types.SpeechConfig(
                voice_config=types.VoiceConfig(
                    prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name=TTS_VOICE)
                )
            ),
            output_audio_transcription=types.AudioTranscriptionConfig(),
            system_instruction=types.Content(parts=[types.Part(text=(
                "너는 한국어 음성 합성기다. 사용자가 보낸 문장을 한 글자도 바꾸지 말고 "
                "자연스러운 한국어로 그대로 소리 내어 읽어라. "
                "대답, 인사, 설명, 질문을 절대 덧붙이지 마라."
            ))]),
        )

    # ── 마이크 ──
    async def mic_worker(self):
        while True:
            proc = await asyncio.create_subprocess_exec(
                "arecord", "-q", "-D", MIC_DEVICE, "-f", "S16_LE",
                "-r", str(MIC_RATE), "-c", "1", "-t", "raw",
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL,
            )
            print(f"[MIC] 녹음 시작 ({MIC_DEVICE})")
            try:
                while True:
                    chunk = await proc.stdout.readexactly(CHUNK_BYTES)
                    if self.audio_q.full():
                        self.audio_q.get_nowait()      # 밀리면 오래된 것 버림
                    self.audio_q.put_nowait(chunk)
            except asyncio.IncompleteReadError:
                print("[MIC] arecord 종료됨 -> 1초 후 재시작")
            finally:
                if proc.returncode is None:
                    proc.kill()
                    await proc.wait()
            await asyncio.sleep(1)

    # ── STT ──
    async def stt_worker(self):
        while True:
            try:
                async with self.client.aio.live.connect(
                        model=STT_MODEL, config=self.stt_config) as session:
                    print("[STT] Gemini Live 전사 세션 연결됨")
                    send_t = asyncio.create_task(self._stt_send(session))
                    recv_t = asyncio.create_task(self._stt_recv(session))
                    done, pending = await asyncio.wait(
                        {send_t, recv_t}, timeout=STT_SESSION_SEC,
                        return_when=asyncio.FIRST_COMPLETED)
                    for t in pending:
                        t.cancel()
                    await asyncio.gather(*pending, return_exceptions=True)
                    for t in done:
                        if t.exception():
                            raise t.exception()
                    print("[STT] 세션 교체 (재연결)")
            except asyncio.CancelledError:
                raise
            except Exception as e:
                print(f"[STT] 세션 오류: {e} -> 2초 후 재연결")
                await asyncio.sleep(2)

    async def _stt_send(self, session):
        while True:
            chunk = await self.audio_q.get()
            if self.mic_muted:
                if not self._stream_ended:
                    await session.send_realtime_input(audio_stream_end=True)
                    self._stream_ended = True
                continue
            self._stream_ended = False
            await session.send_realtime_input(
                audio=types.Blob(data=chunk, mime_type=f"audio/pcm;rate={MIC_RATE}"))

    async def _stt_recv(self, session):
        while True:
            async for msg in session.receive():
                if msg.go_away:
                    print("[STT] 서버가 세션 종료 예고 -> 재연결")
                    return
                sc = msg.server_content
                if not sc or not sc.input_transcription:
                    continue
                text = (sc.input_transcription.text or "").strip()
                if text and not self.mic_muted:
                    await self.text_q.put(text)

    # ── TTS ──
    async def speak(self, text: str):
        if not text:
            return
        async with self.tts_lock:
            self.mic_muted = True
            try:
                await asyncio.to_thread(play_ready_sound)   # 오디오 장치 깨우기
                await asyncio.wait_for(self._speak_once(text), TTS_TIMEOUT_SEC)
            except Exception as e:
                print(f"TTS 실패: {e}")
            finally:
                await asyncio.sleep(TTS_TAIL_SEC)
                while not self.audio_q.empty():          # 재생 중 쌓인 소리 버림
                    self.audio_q.get_nowait()
                self.mic_muted = False

    async def _speak_once(self, text: str):
        player = await asyncio.create_subprocess_exec(
            *PLAYER_CMD, stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        spoken = []
        try:
            async with self.client.aio.live.connect(
                    model=TTS_MODEL, config=self.tts_config) as session:
                await session.send_client_content(
                    turns=types.Content(role="user", parts=[types.Part(text=text)]),
                    turn_complete=True)
                async for msg in session.receive():
                    sc = msg.server_content
                    if not sc:
                        continue
                    if sc.model_turn:
                        for part in sc.model_turn.parts:
                            if part.inline_data and part.inline_data.data:
                                player.stdin.write(part.inline_data.data)
                                await player.stdin.drain()
                    if sc.output_transcription and sc.output_transcription.text:
                        spoken.append(sc.output_transcription.text)
                    if sc.turn_complete:
                        break
        finally:
            player.stdin.close()
            await player.wait()
        print(f"[TTS] {''.join(spoken).strip() or text}")

    def speak_from_thread(self, text: str):
        """ROS 콜백 스레드 등 asyncio 루프 밖에서 호출할 때 사용"""
        return asyncio.run_coroutine_threadsafe(self.speak(text), self.loop)


# ─────────────────────────────── ROS 노드 ───────────────────────────────

class CameraFrameNode(Node):
    def __init__(self, image_topic: str, voice: GeminiVoice):
        super().__init__('qwen_camera_frame_node')
        self.voice = voice
        self.latest_frame = None

        self.subscription = self.create_subscription(
            CompressedImage, image_topic, self.image_callback, 10)
        self.status_sub = self.create_subscription(
            String, "navigation_status", self.navigation_callback, 10)
        self.get_logger().info(f'구독 토픽: {image_topic}')

    def image_callback(self, msg: CompressedImage):
        try:
            np_arr = np.frombuffer(msg.data, dtype=np.uint8)
            image = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
            if image is None:
                self.get_logger().error("JPEG 디코딩 실패")
                return
            self.latest_frame = image
        except Exception as e:
            self.get_logger().error(f'이미지 변환 실패: {e}')

    def navigation_callback(self, msg: String):
        if not msg.data.startswith("arrived|"):
            return
        _, message = msg.data.split("|", 1)
        print("목적지 도착!")

        # 안내 멘트가 끝난 뒤 홈 복귀 (ROS 스레드는 막지 않음)
        fut = self.voice.speak_from_thread(message)

        def _go_home(_):
            print("복귀 시작")
            send_class_name("home")
        fut.add_done_callback(_go_home)


# ─────────────────────────────── 대화 로직 ───────────────────────────────

async def dialog_loop(voice: GeminiVoice, camera_node: CameraFrameNode):
    waiting_confirm = False
    pending_class_name = ""
    print("음성 인식 대기 시작.")

    while rclpy.ok():
        text = await voice.text_q.get()
        print(f"인식된 문장: {text}")

        if waiting_confirm:
            # '응 아니야' 같은 경우 때문에 부정을 먼저 검사
            if is_no(text):
                await asyncio.to_thread(play_ready_sound)
                print("사용자가 안내를 거절했습니다.")
                send_class_name("home")
                waiting_confirm, pending_class_name = False, ""
            elif is_yes(text):
                await asyncio.to_thread(play_ready_sound)
                print("사용자가 안내를 요청했습니다.")
                send_class_name(pending_class_name)
                waiting_confirm, pending_class_name = False, ""
            else:
                print("응답을 이해하지 못했습니다. 다시 말씀해주세요.")
            continue

        if not is_target_phrase(text):
            print("-> 대상 문장 아님, 버림")
            continue

        print("-> 대상 문장 감지!")
        await voice.speak("물건 인식을 시작합니다.")
        print("-> 캡처 진행")
        await asyncio.to_thread(play_ready_sound, "camera-shutter.oga")

        if not await asyncio.to_thread(save_current_frame, camera_node):
            continue

        answer = await asyncio.to_thread(call_vllm_server, IMAGE_SAVE_PATH, text)
        if not answer:
            continue
        print(f"[VLM 응답] {answer}")

        tts_text = extract_tts_text(answer)
        if tts_text:
            await voice.speak(tts_text)

        class_name = extract_class_name(answer)
        if class_name == "미등록":
            destination = extract_destination(answer)
            if destination:
                pending_class_name, waiting_confirm = destination, True
            else:
                print("응답에서 목적지를 찾지 못했습니다.")
        elif class_name:
            pending_class_name, waiting_confirm = class_name, True
        else:
            print("응답에서 [클래스: ...] 형식을 찾지 못했습니다.")


async def async_main(voice: GeminiVoice, camera_node: CameraFrameNode):
    voice.loop = asyncio.get_running_loop()
    tasks = [
        asyncio.create_task(voice.mic_worker()),
        asyncio.create_task(voice.stt_worker()),
    ]
    try:
        await dialog_loop(voice, camera_node)
    finally:
        for t in tasks:
            t.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)


if __name__ == "__main__":
    rclpy.init()
    voice = GeminiVoice()
    camera_node = CameraFrameNode(
        image_topic="/zed/zed_node/rgb/color/raw/image/compressed", voice=voice)

    ros_thread = threading.Thread(target=rclpy.spin, args=(camera_node,), daemon=True)
    ros_thread.start()

    try:
        asyncio.run(async_main(voice, camera_node))
    except KeyboardInterrupt:
        pass
    finally:
        camera_node.destroy_node()
        rclpy.shutdown()


