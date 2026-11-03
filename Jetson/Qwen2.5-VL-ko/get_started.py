# huggingface - Qwen2.5-VL-3B-Instruct // Get Started
# https://huggingface.co/Qwen/Qwen2.5-VL-3B-Instruct
# git - mxnseo

# conda env
"""
    -- RTX 4070 PC --

    conda create -n qwen25vl python=3.10 -y
    conda activate qwen25vl

    pip install torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu121

    # transformers (pip version -> qwen2_5_vl KeyError)
    pip install git+https://github.com/huggingface/transformers accelerate

    # Qwen VL util
    pip install qwen-vl-utils[decord]==0.0.8

    # if fine-tuning model load
    pip install peft


    -- Jetson AGX Orin --

    conda create -n qwen25vl python=3.10 -y
    conda activate qwen25vl

    pip install torch-2.5.0-cp310-cp310-linux_aarch64.whl
    pip install torchvision-0.20.0-cp310-cp310-linux_aarch64.whl

    # transformers (pip version -> qwen2_5_vl KeyError)
    pip install git+https://github.com/huggingface/transformers accelerate

    # Qwen VL util
    pip install qwen-vl-utils==0.0.8

    pip install "numpy<2"
"""

from transformers import AutoProcessor, Qwen2_5_VLForConditionalGeneration
import torch
import time

# Inference Time Check
# def measure_inference(func):
#     def wrapper(*args, **kwargs):
#         if torch.cuda.is_available():
#             torch.cuda.synchronize()
#         t_start = time.perf_counter()

#         result = func(*args, **kwargs)

#         if torch.cuda.is_available():
#             torch.cuda.synchronize()
#         t_end = time.perf_counter()

#         elapsed = t_end - t_start
#         print(f"\n[{func.__name__}] Inference Time: {elapsed:.3f} s")
#         if torch.cuda.is_available():
#             print(f"VRAM: {torch.cuda.memory_allocated() / 1e9:.2f} GB")

#         if isinstance(result, tuple) and len(result) == 2:
#             actual_result, n_tokens = result
#             if isinstance(n_tokens, int):
#                 print(f"Tokens: {n_tokens} | {n_tokens / elapsed:.1f} tok/s")
#             return actual_result

#         return result
#     return wrapper



# vllm model load
import base64
from vllm import LLM, SamplingParams

QUANT_MODEL_PATH = "./vLLM_Qwen2.5-VL/qwen2.5-vl-vllm"

llm = None

# only vLLM
# MERGED_MODEL_PATH = "./vLLM_Qwen2.5-VL/qwen2.5-vl-merged"

sampling_params = SamplingParams(temperature=0.0, max_tokens=512)

def to_image_url(source: str) -> str:
    if source.startswith("http://") or source.startswith("https://"):
        return source
    with open(source, "rb") as f:
        b64 = base64.b64encode(f.read()).decode("utf-8")
    return f"data:image/jpeg;base64,{b64}"

# ---


# Simple Inference (eng)

# @measure_inference
def simple_inference():
    messages = [
        {
            "role": "system",
            "content": "Answer only used english"
        },
        {
            "role": "user",
            "content": [
                {"type": "image", "url": "https://huggingface.co/datasets/huggingface/documentation-images/resolve/main/bee.jpg"},
                {"type": "text", "text": "Can you describe this image?"},
            ]
        },
    ]

    inputs = processor.apply_chat_template(
        messages,
        add_generation_prompt=True,
        tokenize=True,
        return_dict=True,
        return_tensors="pt",
    ).to(model.device, dtype=torch.bfloat16)

    generated_ids = model.generate(**inputs, do_sample=False, max_new_tokens=512)

    input_len = inputs.input_ids.shape[-1]
    n_tokens = generated_ids.shape[-1] - input_len

    generated_ids_trimmed = [
        out[len(in_):] for in_, out in zip(inputs.input_ids, generated_ids)
    ]
    generated_texts = processor.batch_decode(
        generated_ids_trimmed,
        skip_special_tokens=True,
        clean_up_tokenization_spaces=False,
    )
    print(generated_texts[0])

    return generated_ids[0], n_tokens


SYSTEM_PROMPT = (
    "당신은 연구실 물건 위치 안내 AI입니다.\n"
    "물품별 보관 구역:\n"
    "a구역: DC어댑터, HDMI, 글루건, 멀티미터, 배터리, 스테이플러, 시리얼통신케이블, 케이블타이\n"
    "b구역: 드라이버, 망치, 서포터, 전동드릴, 펜치\n"
    "c구역: 밴드, 붕대, 연고\n"
    "d구역: SSD, USB\n"
    "목록에 있는 물건이면:\n"
    "[클래스: 물건명]\n"
    "물건명입니다. 현재 (윤지 자리/민서 자리)에 있으며 (a/b/c/d)구역으로 이동해야 합니다.\n"
    "목록에 없는 물건이면:\n"
    "[클래스: 미등록]\n"
    "시각적 특징 설명. 가장 유사한 (a/b/c/d)구역으로 이동해야 합니다."
)

# Simple Inference (ko)

# @measure_inference
def korean_inference():
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "image_url", "image_url": {"url": to_image_url("A.png")}},
                {"type": "text", "text": f"{SYSTEM_PROMPT}\n\n이 물건 어디에 둬야해? (한국어로 답변해주세요.)"},
            ]
        },
    ]


    outputs = llm.chat(messages, sampling_params=sampling_params)
    generated_text = outputs[0].outputs[0].text
    n_tokens = len(outputs[0].outputs[0].token_ids)

    print(generated_text)

    return generated_text, n_tokens


# ── 추가: 음성인식(STT) 쪽에서 (이미지 + 문장)을 HTTP로 받아서 처리하는 함수 ──
# korean_inference()와 로직은 동일하되, 이미지 경로와 질문 문장을 인자로 받도록만 확장
# @measure_inference
def infer_from_request(image_path: str, user_text: str):
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "image_url", "image_url": {"url": to_image_url(image_path)}},
                {"type": "text", "text": f"{SYSTEM_PROMPT}\n\n{user_text}"},
            ]
        },
    ]

    outputs = llm.chat(messages, sampling_params=sampling_params)
    generated_text = outputs[0].outputs[0].text
    n_tokens = len(outputs[0].outputs[0].token_ids)

    print(generated_text)

    return generated_text, n_tokens




# Object recognition (ko, for project)

# @measure_inference
def object_recognition():
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "image", "path": "hard.jpg"},
                {"type": "text", "text": "이게 뭔가요? 물건 이름과 어떤 상황인가요?"},
            ]
        },
    ]

    inputs = processor.apply_chat_template(
        messages,
        add_generation_prompt=True,
        tokenize=True,
        return_dict=True,
        return_tensors="pt",
    ).to(model.device, dtype=torch.bfloat16)

    generated_ids = model.generate(**inputs, do_sample=False, max_new_tokens=64)

    input_len = inputs.input_ids.shape[-1]
    n_tokens = generated_ids.shape[-1] - input_len

    generated_ids_trimmed = [
        out[len(in_):] for in_, out in zip(inputs.input_ids, generated_ids)
    ]
    generated_texts = processor.batch_decode(
        generated_ids_trimmed,
        skip_special_tokens=True,
        clean_up_tokenization_spaces=False,
    )
    print(generated_texts[0])

    return generated_ids[0], n_tokens




# Multi-image Inference (ko)

# @measure_inference
def multi_image_inference():
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "text", "text": "두 이미지의 공통점은 무엇인가요?"},
                {"type": "image", "url": "https://huggingface.co/datasets/huggingface/documentation-images/resolve/main/bee.jpg"},
                {"type": "image", "url": "https://huggingface.co/datasets/huggingface/documentation-images/resolve/0052a70beed5bf71b92610a43a52df6d286cd5f3/diffusers/rabbit.jpg"},
            ]
        },
    ]

    inputs = processor.apply_chat_template(
        messages,
        add_generation_prompt=True,
        tokenize=True,
        return_dict=True,
        return_tensors="pt",
    ).to(model.device, dtype=torch.bfloat16)

    generated_ids = model.generate(**inputs, do_sample=False, max_new_tokens=64)

    input_len = inputs.input_ids.shape[-1]
    n_tokens = generated_ids.shape[-1] - input_len

    generated_ids_trimmed = [
        out[len(in_):] for in_, out in zip(inputs.input_ids, generated_ids)
    ]
    generated_texts = processor.batch_decode(
        generated_ids_trimmed,
        skip_special_tokens=True,
        clean_up_tokenization_spaces=False,
    )
    print(generated_texts[0])

    return generated_ids[0], n_tokens


if __name__ == "__main__":
    # Jetson -> gpu-memory_utilization=0.7
    # vLLM + llmcompressor
    llm = LLM(
        model=QUANT_MODEL_PATH,
        quantization="compressed-tensors",
        max_model_len=4096,
        gpu_memory_utilization=0.3,
        limit_mm_per_prompt={"image": 1}
    )

    # llm = LLM(
    #     model=MERGED_MODEL_PATH,
    #     max_model_len=4096,
    #     gpu_memory_utilization=0.85,
    #     limit_mm_per_prompt={"image": 2}
    # )

    # simple_inference()
    # korean_inference()
    # korean_inference()
    # object_recognition()
    # multi_image_inference()

    # ── 여기서부터 "받는 것" 추가: 위 결과가 정상 로딩된 뒤에만 서버 시작 ──
    # fastapi/uvicorn 설치 없이 표준 라이브러리(http.server)만 사용.
    # 어차피 같은 Jetson, 같은 파일시스템이라 이미지는 업로드 없이 "경로"만 주고받음.
    import json
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

    class InferHandler(BaseHTTPRequestHandler):
        def do_POST(self):
            if self.path != "/infer":
                self.send_response(404)
                self.end_headers()
                return

            try:
                length = int(self.headers.get("Content-Length", 0))
                body = self.rfile.read(length)
                data = json.loads(body)

                image_path = data["image_path"]
                text = data["text"]

                answer, n_tokens = infer_from_request(image_path, text)
                payload = json.dumps(
                    {"answer": answer, "n_tokens": n_tokens}
                ).encode("utf-8")

                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

            except Exception as e:
                print(f"추론 실패: {e}")
                payload = json.dumps({"error": str(e)}).encode("utf-8")
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

        def log_message(self, format, *args):
            # 기본 접근 로그는 끄고, 필요한 로그는 infer_from_request 안에서 print로 출력
            pass

    server = ThreadingHTTPServer(("0.0.0.0", 8000), InferHandler)
    print("서버 시작: http://0.0.0.0:8000/infer")
    server.serve_forever()
