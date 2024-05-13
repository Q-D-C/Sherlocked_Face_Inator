import replicate
import time

base_prompt = "sketch of {description} img. dark, dramatic, low detail, dressed in alchemist clothes, looking serious"

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
        # Append 'with glasses' if the description does not include it
        person += " with glasses"

    return person

# Function to generate an image based on a prompt


def generate(input_prompt, path, style):
    with open(path, "rb") as input_image_file:
        output = replicate.run(
            "tencentarc/photomaker-style:467d062309da518648ba89d226490e02b8ed09b5abc15026e54e31c5a8cd0769",
            input={
                "prompt": input_prompt,
                "num_steps": 50,
                "style_name": style,
                "input_image": input_image_file,
                "num_outputs": 4,
                "guidance_scale": 5,
                "negative_prompt": "realistic, photo-realistic, worst quality, greyscale, bad anatomy, bad hands, error, text",
                "style_strength_ratio": 35
            }
        )
    print(output)
    
def generate_images():
    # Read the number of players from numplayers.txt
    with open("numPlayers.txt", "r") as numplayers_file:
        num_players = int(numplayers_file.read())

    # Loop through each face image
    for i in range(1, num_players + 1):
        image_path = f"/home/nino/Sherlocked_Face_Inator/FACES/face{i}.png"
        # found_description = check_person(image_path)
        # full_prompt = base_prompt.format(description=found_description)
        # generate(full_prompt, image_path, "Comic book")
        print(image_path)

    # Write to done.txt to indicate completion
    with open("done.txt", "w") as done_file:
        done_file.write("1")


# Main execution block
if __name__ == "__main__":
    while True:
        # Read the scanning complete status from scanningcomplete.txt
        with open("scanningComplete.txt", "r") as scanning_complete_file:
            scanning_complete = scanning_complete_file.read().strip()

    # Check if scanning is complete
        if scanning_complete == "1":
            # Generate images
            generate_images()
        else:
            print("Scanning not complete. Idle...")

        # Wait for some time before checking again
        time.sleep(5)  # Wait for x seconds before checking again
