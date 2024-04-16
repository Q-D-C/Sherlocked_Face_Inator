import replicate

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

# Function to check and update the person description based on glasses information
def check_person(path):
    glasses = hasGlasses(path)  # Check if the person in the image has glasses
    person = whatPerson(path)  # Get the description of the person

    if 'glasses' not in person.lower() and glasses == "yes":
        person += " with glasses"  # Append 'with glasses' if the description does not include it

    return person

# Function to generate an image based on a prompt
def generate(input_prompt, path):
    with open(path, "rb") as input_image_file:
        output = replicate.run(
            "tencentarc/photomaker-style:467d062309da518648ba89d226490e02b8ed09b5abc15026e54e31c5a8cd0769",
            input={
                "prompt": input_prompt,
                "num_steps": 50,
                "style_name": "Comic book",
                "input_image": input_image_file,
                "num_outputs": 1,
                "guidance_scale": 5,
                "negative_prompt": "realistic, photo-realistic, worst quality, greyscale, bad anatomy, bad hands, error, text",
                "style_strength_ratio": 35
            }
        )
    print(output)

# Main execution block
if __name__ == "__main__":
    base_prompt = "sketch of {description} img. dark, dramatic, low detail, dressed in alchemist clothes, looking serious"
    image_path = "/home/nino/Sherlocked_Face_Inator/FACES/face_1.jpg"
    found_description = check_person(image_path)
    full_prompt = base_prompt.format(description=found_description)
    print(full_prompt)
    generate(full_prompt, image_path)
