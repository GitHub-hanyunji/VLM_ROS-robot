from peft import PeftModel
from transformers import Qwen2_5_VLForConditionalGeneration, AutoProcessor

base_model_path = "Qwen/Qwen2.5-VL-3B-Instruct"
lora_path = "../../fine-tune/Qwen-VL-Series-Finetune/output/qwen25vl-airlab-lora"
merged_path = "./qwen2.5-vl-merged"

model = Qwen2_5_VLForConditionalGeneration.from_pretrained(
    base_model_path, 
    torch_dtype="auto", 
    device_map="cpu"
)

model = PeftModel.from_pretrained(model, lora_path)
model = model.merge_and_unload()
model.save_pretrained(merged_path)

processor = AutoProcessor.from_pretrained(base_model_path)
processor.save_pretrained(merged_path)