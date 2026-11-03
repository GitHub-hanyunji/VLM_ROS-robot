# Qwen2.5-VL-3B-Instruct 추론 예제

Qwen2.5-VL-3B-Instruct 모델 셋업부터 한국어 추론 예제 작성 코드.  
가상환경 구축, 추론 시간 측정, 한국어 프롬프트 활용 포함.  

---

## 스펙

| 항목 | 내용 |
|---|---|
| OS | Ubuntu 22.04 LTS |
| Python | 3.10 |
| GPU | RTX 4070 |
| CUDA | 12.1 |
| 모델 | Qwen/Qwen2.5-VL-3B-Instruct |
| Attention | SDPA (`sdpa`) |

---

## 환경 셋업

### 1. conda 가상환경 생성

```bash
conda create -n qwen25vl python=3.10 -y
conda activate qwen25vl
```

### 2. PyTorch 설치 (CUDA 12.1)

```bash
pip install torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu121
```

### 3. transformers 설치

```bash
# pip 버전 설치 시 qwen2_5_vl KeyError 발생 -> 반드시 GitHub 최신본 설치
pip install git+https://github.com/huggingface/transformers accelerate
```

### 4. Qwen VL 유틸 설치

```bash
pip install qwen-vl-utils[decord]==0.0.8
```

---

## 모델 로드

```python
from transformers import AutoProcessor, Qwen2_5_VLForConditionalGeneration
import torch

model_path = "Qwen/Qwen2.5-VL-3B-Instruct"

processor = AutoProcessor.from_pretrained(model_path)
model = Qwen2_5_VLForConditionalGeneration.from_pretrained(
    model_path,
    torch_dtype=torch.bfloat16,
    attn_implementation="sdpa",  # flash_attention_2 대신 sdpa 사용
    device_map="auto"
)
```

> `flash_attention_2`도 쓸 수 있으나 현재 예제에서는 `sdpa`로 설정함.  
> 바꾸고 싶으면 `attn_implementation="flash_attention_2"` 로 교체.

---

## 추론 시간 측정 데코레이터

CUDA 동기화 후 `perf_counter`로 실측 시간을 재고 VRAM 사용량도 같이 출력함.

```python
import time

def measure_inference(func):
    def wrapper(*args, **kwargs):
        if torch.cuda.is_available():
            torch.cuda.synchronize()
        t_start = time.perf_counter()

        result = func(*args, **kwargs)

        if torch.cuda.is_available():
            torch.cuda.synchronize()
        t_end = time.perf_counter()

        elapsed = t_end - t_start
        print(f"\n[{func.__name__}] Inference Time: {elapsed:.3f} s")
        if torch.cuda.is_available():
            print(f"VRAM: {torch.cuda.memory_allocated() / 1e9:.2f} GB")
        return result
    return wrapper
```

측정 대상 함수에 `@measure_inference` 달면 끝.

---

## 디코딩 방식

SmolVLM2와 달리 Qwen2.5-VL는 입력 토큰을 출력에서 직접 잘라냄.

```python
generated_ids_trimmed = [
    out[len(in_):] for in_, out in zip(inputs.input_ids, generated_ids)
]
generated_texts = processor.batch_decode(
    generated_ids_trimmed,
    skip_special_tokens=True,
    clean_up_tokenization_spaces=False,
)
```

---

## 추론 예제

### 단일 이미지 추론 (영어)

```python
@measure_inference
def simple_inference():
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "image", "url": "https://...bee.jpg"},
                {"type": "text", "text": "Can you describe this image?"},
            ]
        },
    ]
    ...

simple_inference()
```

### 단일 이미지 추론 (한국어)

```python
@measure_inference
def korean_inference():
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "image", "url": "https://...bee.jpg"},
                {"type": "text", "text": "이 이미지에 무엇이 있나요?"},
            ]
        },
    ]
    ...

korean_inference()
```

### 객체 인식 (한국어, 로컬 이미지)

짧은 응답이 필요한 실용 태스크. `max_new_tokens=16`으로 제한해서 빠르게 뽑음.

```python
@measure_inference
def object_recognition():
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "image", "path": "hard.jpg"},   # 로컬 파일
                {"type": "text", "text": "이게 뭔가요? 물건 이름과 어떤 상황인가요?"},
            ]
        },
    ]
    ...

object_recognition()
```

### 다중 이미지 추론 (한국어)

이미지 두 장을 동시에 넘겨 비교/분석 가능.

```python
@measure_inference
def multi_image_inference():
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "text", "text": "두 이미지의 공통점은 무엇인가요?"},
                {"type": "image", "url": "https://...bee.jpg"},
                {"type": "image", "url": "https://...rabbit.jpg"},
            ]
        },
    ]
    ...

multi_image_inference()
```

---

## 출력 예시

```
[simple_inference] Inference Time: 2.104 s
VRAM: 6.23 GB

[korean_inference] Inference Time: 2.381 s
VRAM: 6.25 GB

[object_recognition] Inference Time: 0.873 s
VRAM: 6.21 GB

[multi_image_inference] Inference Time: 3.017 s
VRAM: 6.48 GB
```

> 수치는 입력 길이 및 시스템 상태에 따라 달라질 수 있음.

---

## 주의사항

- `transformers`를 pip으로 설치하면 `qwen2_5_vl` KeyError 뜸. 반드시 GitHub 최신본으로 설치할 것
- `device_map="auto"` 사용 시 멀티 GPU 환경에서 자동 분배됨
- `flash_attention_2`는 Ampere 이상 GPU(RTX 30xx / 40xx)에서만 동작

---

## 참고

- 모델: [Qwen/Qwen2.5-VL-3B-Instruct](https://huggingface.co/Qwen/Qwen2.5-VL-3B-Instruct)
- transformers: [huggingface/transformers](https://github.com/huggingface/transformers)
