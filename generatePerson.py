import replicate
import time
import os
import requests
from PIL import Image
from replicate.exceptions import ModelError

base_prompt_epic = "sketch of {description} img. dark, dramatic, low detail, dressed in alchemist clothes, looking serious"
base_prompt_sketch = "a rough sketch of a {description} img, alchemist clothes, dressed as an alchemist, unrefined, with pencil strokes, solid background, magical setting, two colors"

selfContained = False
MAX_RETRIES = 5

# Function to check if the person in the image wears glasses
def hasGlasses(path):
    with open(path, "rb") as image_file:
        output = replicate.run(
            "andreasjansson/blip-2:f677695e5e89f8b236e52ecd1d3f01beb44c34606419bcc19345e046d8f786f9",
            input={
                "image": image_file,
                "caption": False,
                "context": "there is one person",
                "question": "does this person wear glasses, yes or no",
                "temperature": 1,
                "use_nucleus_sampling": False
            }
        )
    return output

# Function to identify the type of person in the image
def whatPerson(path):
    with open(path, "rb") as image_file:
        output = replicate.run(
            "andreasjansson/blip-2:f677695e5e89f8b236e52ecd1d3f01beb44c34606419bcc19345e046d8f786f9",
            input={
                "image": image_file,
                "caption": False,
                "context": "there is one person",
                "question": "what kind of person is in the picture",
                "temperature": 1,
                "use_nucleus_sampling": False
            }
        )
    return output

def whatHair(path):
    with open(path, "rb") as image_file:
        output = replicate.run(
            "andreasjansson/blip-2:f677695e5e89f8b236e52ecd1d3f01beb44c34606419bcc19345e046d8f786f9",
            input={
                "image": image_file,
                "caption": False,
                "context": "there is one person",
                "question": "what is this persons hair color and length",
                "temperature": 1,
                "use_nucleus_sampling": False
            }
        )
    return output

def whatEye(path):
    with open(path, "rb") as image_file:
        output = replicate.run(
            "andreasjansson/blip-2:f677695e5e89f8b236e52ecd1d3f01beb44c34606419bcc19345e046d8f786f9",
            input={
                "image": image_file,
                "caption": False,
                "context": "there is one person",
                "question": "What is this persons eye color",
                "temperature": 1,
                "use_nucleus_sampling": False
            }
        )
    return output

# Function to check and update the person description based on information
def check_person(path):
    glasses = hasGlasses(path)  # Check if the person in the image has glasses
    person = whatPerson(path)  # Get the description of the person
    hair = whatHair(path)
    eye = whatEye(path)
    
    if 'glasses' not in person.lower() and glasses == "yes":
        # Append 'with glasses' if the description does not include it
        person += " with glasses"
    person += " with "
    person += hair
    person += " and "
    person += eye
    person += " eyes"
    
    return person

# Function to save an image from a URL to a local path
def save_image(url, path):
    response = requests.get(url)
    with open(path, 'wb') as file:
        file.write(response.content)

# Function to overlay one image on top of another
def overlay_image(background_path, overlay_path, output_path, position=(0, 0)):
    background = Image.open(background_path)
    overlay = Image.open(overlay_path)
    background.paste(overlay, position, overlay)
    background.save(output_path)

# Function to generate an image based on a prompt with retry mechanism
def generate_image_with_retries(generate_function, *args, **kwargs):
    retries = 0
    while retries < MAX_RETRIES:
        try:
            generate_function(*args, **kwargs)
            break
        except ModelError as e:
            print(f"Error: {e}. Retrying... ({retries + 1}/{MAX_RETRIES})")
            retries += 1
            time.sleep(2)
    else:
        print("Failed to generate image after several retries.")

# Function to generate an image based on a prompt
def generate_epic(input_prompt, path, style, output_dir, overlay_path=None, image_index=0):
    with open(path, "rb") as input_image_file:
        output = replicate.run(
            "tencentarc/photomaker-style:467d062309da518648ba89d226490e02b8ed09b5abc15026e54e31c5a8cd0769",
            input={
                "prompt": input_prompt,
                "num_steps": 50,
                "style_name": style,
                "input_image": input_image_file,
                "num_outputs": 1,
                "guidance_scale": 5,
                "negative_prompt": "realistic, photo-realistic, worst quality, greyscale, bad anatomy, bad hands, error, text, hat, wizard hat",
                "style_strength_ratio": 35
            }
        )
    if output is None:
        print(f"Error: No output received for prompt: {input_prompt}")
        return
    for idx, image_url in enumerate(output):
        epic_image_path = os.path.join(output_dir, f"epic_{image_index}.png")
        save_image(image_url, epic_image_path)
        if overlay_path:
            overlay_output_path = os.path.join(output_dir, f"epic_framed_{image_index}.png")
            overlay_image(epic_image_path, overlay_path, overlay_output_path)

def generate_sketch(input_prompt, path, output_dir, image_index=0):
    with open(path, "rb") as input_image_file:
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
        print(f"Error: No output received for prompt: {input_prompt}")
        return
    for idx, image_url in enumerate(output):
        sketch_image_path = os.path.join(output_dir, f"sketch_{image_index}.png")
        save_image(image_url, sketch_image_path)

def generate_images(overlay_path):
    # Read the number of players from numPlayers.txt
    with open("numPlayers.txt", "r") as numplayers_file:
        num_players = int(numplayers_file.read())
        print("amount of pictures to generate: ", num_players)
    
    output_dir_epic = "epic_pictures"
    output_dir_sketch = "sketch_pictures"
    os.makedirs(output_dir_epic, exist_ok=True)
    os.makedirs(output_dir_sketch, exist_ok=True)
    
    # Loop through each face image
    for i in range(1, num_players + 1):
        image_path = f"face_{i}.jpg"
        print(image_path)
        found_description = check_person(image_path)
        full_prompt_epic = base_prompt_epic.format(description=found_description)
        print("epic prompt", i, " is ", full_prompt_epic)
        #generate_image_with_retries(generate_epic, full_prompt_epic, image_path, "Cinematic", output_dir_epic, overlay_path, image_index=i)
        #full_prompt_sketch = base_prompt_sketch.format(description=found_description)
        #print("sketch prompt", i, " is ", full_prompt_sketch)
        #generate_image_with_retries(generate_sketch, full_prompt_sketch, image_path, output_dir_sketch, image_index=i)
        
    # Write to done.txt to indicate completion
    with open("done.txt", "w") as done_file:
        print("updating done.txt")
        done_file.write("1")
        if selfContained:
            with open("scanningComplete.txt", "w") as scanning_complete_file:
                print("updating scanningcomplete.txt")
                scanning_complete_file.write("0")

# Main execution block
if __name__ == "__main__":
    while True:
        # Read the scanning complete status from scanningComplete.txt
        with open("scanningComplete.txt", "r") as scanning_complete_file:
            scanning_complete = scanning_complete_file.read().strip()

        # Check if scanning is complete
        if scanning_complete == "1":
            # Generate images with overlay
            overlay_path = "frame.png"  # Path to the overlay image
            generate_images(overlay_path)
        else:
            print("Scanning not complete. Idle...")

        # Wait for some time before checking again
        time.sleep(5)  # Wait for x seconds before checking again
