"""
    RTX 4070 SUPER PC
    
    conda create -n vllm_qwen25vl python=3.10 -y
    conda activate vllm_qwen25vl

    pip install vllm
    pip install llmcompressor

    ---

    Jetson AGX Orin 64GB

    sudo apt update
    sudo apt install -y nvidia-container curl
    curl https://get.docker.com | sh && sudo systemctl --now enable docker
    sudo nvidia-ctk runtime configure --runtime=docker

    sudo apt install -y jq
    sudo jq '. + {"default-runtime": "nvidia"}' /etc/docker/daemon.json 2>/dev/null | \
      sudo tee /etc/docker/daemon.json.tmp && \
      sudo mv /etc/docker/daemon.json.tmp /etc/docker/daemon.json

    sudo systemctl daemon-reload
    sudo systemctl restart docker

    git clone https://github.com/dusty-nv/jetson-containers
    cd jetson-containers
    bash install.sh
    jetson-containers run $(autotag vllm)


"""

import json
import re
from transformers import Qwen2_5_VLForConditionalGeneration, AutoProcessor
from llmcompressor import oneshot
from llmcompressor.modifiers.quantization import GPTQModifier
from datasets import Dataset

model_path = "./qwen2.5-vl-merged"

model = Qwen2_5_VLForConditionalGeneration.from_pretrained(
    model_path,
    torch_dtype="auto",
    device_map="auto",
)
processor = AutoProcessor.from_pretrained(model_path)

NUM_CALIBRATION_SAMPLES = 256
MAX_SEQ_LENGTH = 2048

with open("/home/airlab/fine-tune/Dataset/result/train_qwen_airlab.json", "r", encoding="utf-8") as f:
    raw_data = json.load(f)

def extract_text(example):
    parts = []
    for turn in example["conversations"]:
        text = turn["value"]
        text = re.sub(r"<image>\n?", "", text)
        role = "user" if turn["from"] == "human" else "assistant"
        parts.append(f"{role}: {text}")
    return {"text": "\n".join(parts)}

processed = [extract_text(ex) for ex in raw_data]
ds = Dataset.from_list(processed)
ds = ds.shuffle(seed=42).select(range(min(NUM_CALIBRATION_SAMPLES, len(ds))))

def tokenize(example):
    return processor.tokenizer(
        example["text"],
        padding=False,
        max_length=MAX_SEQ_LENGTH,
        truncation=True,
    )

ds = ds.map(tokenize, remove_columns=ds.column_names)

recipe = GPTQModifier(
    targets="Linear",
    scheme="W4A16",
    ignore=["lm_head", "re:.*visual.*"],
)

oneshot(
    model=model,
    recipe=recipe,
    dataset=ds,
    max_seq_length=MAX_SEQ_LENGTH,
    num_calibration_samples=NUM_CALIBRATION_SAMPLES,
    output_dir="./qwen2.5-vl-vllm",
)