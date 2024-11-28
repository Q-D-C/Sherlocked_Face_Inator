import os
import sys
import replicate
from datetime import datetime
from PIL import Image
import requests

# Constants for prompts
BASE_PROMPT_EPIC = "sketch of {description} img. dark, dramatic, low detail, dressed in alchemist clothes, looking serious"
BASE_PROMPT_SKETCH = "a rough sketch of {description} img, alchemist clothes, dressed as an alchemist, unrefined, with pencil strokes, solid background, magical setting, two colors"
MAX_RETRIES = 5

# Function to save an image from a URL to a local path
def save_image(url, path):
    response = requests.get(url)
    with open(path, 'wb') as file:
        file.write(response.content)

# Function to generate an epic image
def generate_epic(input_prompt, input_image_path, output_path):
    with open(input_image_path, "rb") as input_image_file:
        output = replicate.run(
            "tencentarc/photomaker-style:467d062309da518648ba89d226490e02b8ed09b5abc15026e54e31c5a8cd0769",
            input={
                "prompt": input_prompt,
                "num_steps": 50,
                "style_name": "Cinematic",
                "input_image": input_image_file,
                "num_outputs": 1,
                "guidance_scale": 5,
                "negative_prompt": "realistic, photo-realistic, worst quality, greyscale, bad anatomy, bad hands, error, text, hat, wizard hat",
                "style_strength_ratio": 35
            }
        )
    if output is None:
        raise ValueError("No output received from the model.")
    save_image(output[0], output_path)

# Function to generate a sketch image
def generate_sketch(input_prompt, input_image_path, output_path):
    with open(input_image_path, "rb") as input_image_file:
        output = replicate.run(
            "tencentarc/photomaker:ddfc2b08d209f9fa8c1eca692712918bd449f695dabb4a958da31802a9570fe4",
            input={
                "prompt": input_prompt,
                "num_steps": 50,
                "style_name": "(No style)",
                "input_image": input_image_file,
                "num_outputs": 1,
                "guidance_scale": 3,
                "negative_prompt": "text, nsfw, realistic, refined, hat, wand, bad anatomy, big breasts, bad hands, text, wizard hat, error, missing fingers, extra digit, fewer digits, cropped, jpeg artifacts, signature, watermark, username, blurry, nude, hats, wizard hats",
                "style_strength_ratio": 30
            }
        )
    if output is None:
        raise ValueError("No output received from the model.")
    save_image(output[0], output_path)

# Retry mechanism for generating images
def generate_with_retries(generate_function, *args, **kwargs):
    retries = 0
    while retries < MAX_RETRIES:
        try:
            generate_function(*args, **kwargs)
            break
        except Exception as e:
            print(f"Error: {e}. Retrying... ({retries + 1}/{MAX_RETRIES})")
            retries += 1
    else:
        print("Failed to generate image after several retries.")
        sys.exit(1)

# Main function
def main():
    if len(sys.argv) < 4:
        print("Usage: python3 generateVIP.py [style] [num_images] [path_to_image] [description]")
        sys.exit(1)

    style = sys.argv[1].lower()
    num_images = int(sys.argv[2]) if sys.argv[2].isdigit() else 1  # Default to 1 if invalid
    image_path = sys.argv[3]
    description = sys.argv[4] if len(sys.argv) > 4 else "a mysterious alchemist"

    # Validate inputs
    if style not in ["epic", "sketch"]:
        print("Error: Supported styles are 'epic' and 'sketch'.")
        sys.exit(1)

    if not os.path.exists(image_path):
        print(f"Error: File '{image_path}' does not exist.")
        sys.exit(1)

    # Generate the prompt
    if style == "epic":
        prompt = BASE_PROMPT_EPIC.format(description=description)
    else:  # style == "sketch"
        prompt = BASE_PROMPT_SKETCH.format(description=description)

    # Define output directory as the same directory as the input image
    input_dir = os.path.dirname(image_path)
    input_name, input_ext = os.path.splitext(os.path.basename(image_path))

    # Generate the specified number of images
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    for i in range(1, num_images + 1):
        output_filename = f"{input_name}_{style}_{timestamp}_v{i}.png"
        output_path = os.path.join(input_dir, output_filename)

        # Generate the image
        if style == "epic":
            generate_with_retries(generate_epic, prompt, image_path, output_path)
        else:  # style == "sketch"
            generate_with_retries(generate_sketch, prompt, image_path, output_path)

        print(f"Generated {style} image saved to {output_path}")

if __name__ == "__main__":
    main()
