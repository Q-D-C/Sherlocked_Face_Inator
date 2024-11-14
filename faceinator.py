# source venv/bin/activate
# mosquitto_sub -v -t '#'
# https://cedalo.com/blog/mqtt-subscribe-publish-mosquitto-pub-sub-example/
# mosquitto_pub -h localhost -t alch/faceinator -m "{\"sender\":\"server\",\"numPlayers\":\"3\",\"method\":\"put\"}" -q 1
# mosquitto_pub -h localhost -t alch/faceinator -m "{\"sender\":\"server\",\"numPlayers\":\"1\",\"method\":\"put\"}" -q 1
# mosquitto_pub -h localhost -t alch/faceinator -m "{\"sender\":\"server\", \"method\":\"put\", \"outputs\":[{\"id\":1, \"value\":1}]}" -q 1

import cv2
import json
import socket
import paho.mqtt.client as mqtt
import threading
import time
import os
import numpy as np
import replicate
import requests
from PIL import Image
from replicate.exceptions import ModelError
from datetime import datetime  # Import datetime for timestamp

# Base prompts for generating AI-enhanced images
base_prompt_epic = "sketch of {description} img. dark, dramatic, low detail, dressed in alchemist clothes, looking serious"
base_prompt_sketch = "a rough sketch of a {description} img, alchemist clothes, dressed as an alchemist, unrefined, with pencil strokes, solid background, magical setting, two colors"

# Define the number of retries for generating images
MAX_RETRIES = 5  # The number of times to retry if the image generation fails

# Replicate API Token
REPLICATE_API_TOKEN = os.getenv("REPLICATE_API_TOKEN", "[THIS IS A PRETEND TOKEN DO NOT USE THIS ONE THANK YOUS]")  # Replace this with your actual token

if not REPLICATE_API_TOKEN or REPLICATE_API_TOKEN == "YOUR_REPLICATE_API_TOKEN":
    raise ValueError("Please set the REPLICATE_API_TOKEN environment variable to a valid token.")

# Set up replicate client authentication using the token
os.environ["REPLICATE_API_TOKEN"] = REPLICATE_API_TOKEN

overlay_path = "frame.png"  # Path to the overlay image

# YOLO Model Configurations
YOLO_CONFIG_PATH = "models/yolov4-tiny-3l.cfg"          # Path to the YOLOv4 config file
YOLO_WEIGHTS_PATH = "models/yolov4-tiny-3l_best.weights"     # Path to the YOLOv4 weights file
YOLO_CONFIDENCE_THRESHOLD = 0.5          # Confidence threshold to filter weak detections
YOLO_NMS_THRESHOLD = 0.4                 # Non-Maximum Suppression threshold

BLURRYNESS_THRESHOLD = 1500.0  # Define a suitable threshold for blurriness

# Flag to control whether detection is active
detection_active = False
detection_completed = False

camera_active = False
camera_lock = threading.Lock()

BROKER_ADDRESS = "localhost"  # Use your broker address
PORT = 1883  # The default MQTT port, adjust as needed
TOPIC_SUBSCRIBE = "alch/faceinator"
TOPIC_PUBLISH = "alch"
serverTopic = "alch/faceinator"
_cfg_name = "faceinator"
gameStart = ""
numberPlayers = 0  # Use int type for easier processing

#### START OF MQTT ####

# Function to get the IP address of the local machine
def get_ip_address():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(1)
        s.connect(("8.8.8.8", 80))
        ip_address = s.getsockname()[0]
        s.close()
        return ip_address
    except Exception as e:
        print(f"Error getting IP address: {e}")
        return "Unknown"

# Function to send the information message
def send_info_message(client):
    ip = get_ip_address()
    message = {
        "sender": _cfg_name,
        "connected": True,
        "ip": ip,
        "version": "v0.2.0",
        "method": "info",
        "trigger": "startup"
    }
    client.publish(serverTopic, json.dumps(message))
    print(f"Info message sent: {message}")

# Function to send the disconnection message
def send_disconnection_message(client):
    message = {
        "sender": _cfg_name,
        "connected": False,
        "method": "info"
    }
    client.publish(serverTopic, json.dumps(message))
    print(f"Disconnection message sent: {message}")

# Function to ask for the number of players
def ask_players(client):
    message = {
        "sender": _cfg_name,
        "numPlayers": None,  # None in Python represents a null value in JSON
        "method": "get"
    }
    client.publish("alch/game", json.dumps(message))
    print(f"Ask players message sent: {message}")

def write_to_file(data, filename):
    with open(filename, 'w') as file:
        file.write(data)
    print(f"Wrote to {filename}: {data}")

# Function to create a formatted message
def make_message(sender, method, id, state):
    return json.dumps({
        "sender": sender,
        "method": method,
        "id": id,
        "state": state
    })

# Function to stop the camera safely
def stop_camera():
    global camera_active
    with camera_lock:
        camera_active = False
    print("Camera activity stopped.")

def handle_message(data):
    global gameStart, numberPlayers, detection_completed

    try:
        if "sender" in data and data["sender"] == _cfg_name:
            print("Message from self, ignoring.")
            return

        if "outputs" in data and isinstance(data["outputs"], list):
            for output in data["outputs"]:
                if "id" in output and "value" in output:
                    id = output["id"]
                    value = output["value"]
                    if id == 1 and value == 1:
                        print("Game start command received.")
                        write_to_file("1", "gameStart")
                        message = make_message(_cfg_name, "info", 1, 1)
                        client.publish(serverTopic, message)
                        gameStart = "1"
                        detection_completed = False  # Allow a new detection round
                        ask_players(client)

        elif "numPlayers" in data:
            try:
                numberPlayers = int(data["numPlayers"])  # Convert numPlayers to integer
                print(f"Value for key numPlayers: {numberPlayers}")
                write_to_file(str(numberPlayers), "numPlayers")
            except ValueError:
                print("Error: numPlayers is not a valid integer.")

        elif "method" in data and data["method"] == "get" and "info" in data and data["info"] == "system":
            ip = get_ip_address()
            message = {
                "sender": _cfg_name,
                "connected": True,
                "ip": ip,
                "version": "v0.1.0",
                "method": "info",
                "trigger": "request"
            }
            client.publish(serverTopic, json.dumps(message))

        elif "method" in data and data["method"] == "put" and "outputs" in data and data["outputs"] == "reset":
            print("Reset command received.")
            message = make_message(_cfg_name, "info", 1, 0)
            client.publish(serverTopic, message)
            #reset_states()

    except Exception as e:
        print(f"Error handling message: {e}")

# Update the reset_states function to properly handle game start reset
def reset_states():
    global gameStart, numberPlayers, detection_active, detection_completed
    gameStart = ""
    numberPlayers = 0
    detection_active = False
    detection_completed = False
    message = make_message(_cfg_name, "info", 1, 0)
    client.publish(serverTopic, message)
    print(f"Reset state message sent: {message}")
    print("States have been reset.")
        
        # Called when the client connects to the broker
def on_connect(client, userdata, flags, rc):
    """Called when the client connects to the broker."""
    if rc == 0:
        print("Connected successfully")
        # Subscribing to a topic on successful connection
        client.subscribe(TOPIC_SUBSCRIBE)
        send_info_message(client)
    else:
        print(f"Connect failed with code {rc}")

# Called when a message is received from the broker
def on_message(client, userdata, msg):
    if msg.payload:
        json_str = msg.payload.decode('utf-8')
        print(f"Received message on topic: {msg.topic}")
        print(f"Message payload: {json_str}")

        try:
            received_data = json.loads(json_str)
            print(f"Parsed JSON: {json.dumps(received_data, indent=4)}")
            handle_message(received_data)
        except json.JSONDecodeError as e:
            print(f"Error parsing JSON: {e}")
    else:
        print(f"Message with empty payload received on topic: {msg.topic}")

# Called when a message is published
def on_publish(client, userdata, mid):
    print(f"Message {mid} published")

# Called when the client disconnects from the broker
def on_disconnect(client, userdata, rc):
    send_disconnection_message(client)
    if rc != 0:
        print("Unexpected disconnection")
    else:
        print("Disconnected successfully")

#### END OF MQTT ####


#### START OF OPENCV ####

def load_yolo_model():
    """
    Load the YOLO model from the configuration and weights files.
    """
    net = cv2.dnn.readNetFromDarknet(YOLO_CONFIG_PATH, YOLO_WEIGHTS_PATH)
    net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
    net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)
    return net

# Initialize the YOLO model
yolo_net = load_yolo_model()

def check_blurriness(image):
    """
    Checks if the given image is blurry by calculating the variance of the Laplacian.
    :param image: The image to check.
    :return: The variance of the Laplacian of the image.
    """
    # Convert the image to grayscale
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

    # Compute the Laplacian of the grayscale image
    laplacian = cv2.Laplacian(gray, cv2.CV_64F)

    # Compute the variance of the Laplacian
    mean, stddev = cv2.meanStdDev(laplacian)
    variance = stddev[0][0] ** 2

    return variance

def get_output_names(net):
    """
    Get the names of the output layers of the network.
    """
    layer_names = net.getLayerNames()
    try:
        # If getUnconnectedOutLayers() returns a list of lists
        output_layers = net.getUnconnectedOutLayers()
        if isinstance(output_layers, np.ndarray):
            output_layers = output_layers.flatten().tolist()
        output_names = [layer_names[i - 1] for i in output_layers]
    except TypeError:
        # If getUnconnectedOutLayers() returns a single integer or behaves differently
        output_layers = net.getUnconnectedOutLayers()
        if isinstance(output_layers, np.ndarray):
            output_names = [layer_names[i[0] - 1] for i in output_layers]
        else:
            output_names = [layer_names[output_layers - 1]]
    
    return output_names


def camera_loop():
    global camera_active, detection_active, detection_completed, numberPlayers

    print("Starting camera loop...")

    # Open the camera
    print("Attempting to open the camera...")
    cap = cv2.VideoCapture(0)

    if not cap.isOpened():
        print("Error: Could not open camera.")
        return

    print("Camera opened successfully.")

    face_count = 0
    padding = 20  # Padding around the detected face to ensure the face is fully in the cropped image

    while camera_active:
        # Read a frame from the camera
        ret, frame = cap.read()
        if not ret:
            print("Failed to capture frame, stopping camera.")
            break

        if detection_active:
            # Prepare the frame for YOLO
            blob = cv2.dnn.blobFromImage(frame, 1 / 255.0, (416, 416), [0, 0, 0], swapRB=True, crop=False)
            yolo_net.setInput(blob)
            
            # Perform forward pass to get the YOLO detections
            outs = yolo_net.forward(get_output_names(yolo_net))

            # Extract bounding boxes from YOLO detections
            frame_height, frame_width = frame.shape[:2]
            class_ids = []
            confidences = []
            boxes = []

            for out in outs:
                for detection in out:
                    scores = detection[5:]
                    class_id = np.argmax(scores)
                    confidence = scores[class_id]

                    # Only consider detections with confidence above the threshold
                    if confidence > YOLO_CONFIDENCE_THRESHOLD:
                        center_x = int(detection[0] * frame_width)
                        center_y = int(detection[1] * frame_height)
                        width = int(detection[2] * frame_width)
                        height = int(detection[3] * frame_height)
                        x = int(center_x - width / 2)
                        y = int(center_y - height / 2)

                        # Adding padding to the bounding box
                        x_start = max(x - padding, 0)
                        y_start = max(y - padding, 0)
                        x_end = min(x + width + padding, frame_width)
                        y_end = min(y + height + padding, frame_height)

                        boxes.append([x_start, y_start, x_end - x_start, y_end - y_start])
                        confidences.append(float(confidence))
                        class_ids.append(class_id)

            # Apply Non-Maximum Suppression to filter overlapping boxes
            indices = cv2.dnn.NMSBoxes(boxes, confidences, YOLO_CONFIDENCE_THRESHOLD, YOLO_NMS_THRESHOLD)

            if len(indices) > 0:
                print(f"Looking for {numberPlayers} amount of faces")
                for i in indices.flatten():
                    x, y, w, h = boxes[i]
                    
                    # Extract the face ROI
                    x_start = max(x, 0)
                    y_start = max(y, 0)
                    x_end = min(x + w, frame_width)
                    y_end = min(y + h, frame_height)
                    face = frame[y_start:y_end, x_start:x_end]

                    # Draw a rectangle around the detected face for visualization
                    cv2.rectangle(frame, (x_start, y_start), (x_end, y_end), (255, 0, 0), 2)

                    face_filename = f"face_{face_count}.jpg"
                    try:
                        # Validate the ROI size to make sure it's reasonable before saving
                        if face.shape[0] > 0 and face.shape[1] > 0:
                            cv2.imwrite(face_filename, face)
                            print(f"Face {face_count} saved as {face_filename}")
                            face_count += 1
                        else:
                            print(f"Invalid face dimensions for face {face_count}, skipping save.")
                    except Exception as e:
                        print(f"Error saving face image: {e}")
                        detection_active = False
                        break

                    # Stop detection after detecting the required number of faces
                    if face_count >= numberPlayers:
                        print(f"Captured required number of faces ({numberPlayers}), stopping detection.")
                        detection_active = False
                        detection_completed = True  # Mark detection as completed
                        face_count = 0  # Reset face count for the next round of detection
                        break

                # After capturing faces, check if any are blurry
                if detection_completed:
                    is_a_face_blurry = False
                    for i in range(numberPlayers):
                        filename = f"face_{i}.jpg"
                        face = cv2.imread(filename)
                        if face is not None:
                            blurriness = check_blurriness(face)
                            print(f"Blurriness of face number {i} is {blurriness}")
                            if blurriness <= BLURRYNESS_THRESHOLD:
                                is_a_face_blurry = True
                                break

                    if is_a_face_blurry:
                        # Delete captured faces if any are too blurry
                        print("One or more faces are too blurry, deleting all captured faces.")
                        for i in range(numberPlayers):
                            filename = f"face_{i}.jpg"
                            try:
                                os.remove(filename)
                                print(f"Deleted file: {filename}")
                            except FileNotFoundError:
                                print(f"File not found: {filename}")
                        
                        # Reset the detection to try capturing again
                        detection_completed = False
                        detection_active = True
                    else:
                        print("All captured faces are clear.")
                        # Send MQTT message to indicate generation is in progress
                        message = make_message(_cfg_name, "info", 1, 2)
                        client.publish(serverTopic, message)
                        print(f"Generation in progress message sent: {message}")
                        generate_images()

        # Display the frame (comment out if running headless)
        #try:
        #    cv2.imshow('Face Detection', frame)
        #except Exception as e:
        #    print(f"Error displaying frame: {e}")
        #    break

        # Stop if 'q' is pressed
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # Release the camera and close windows before exiting the function
    cap.release()
    cv2.destroyAllWindows()
    print("Camera feed stopped")

# Function to start the camera loop (to be run in a separate thread)
def start_camera_loop():
    global camera_active
    if not camera_active:
        print("Starting camera loop...")
        camera_active = True
        camera_thread = threading.Thread(target=camera_loop)
        camera_thread.daemon = True  # Mark thread as a daemon so it closes properly
        camera_thread.start()

# Function to activate face detection
def start_detection():
    global detection_active
    if not detection_active:
        print("Starting face detection...")
        detection_active = True

# Function to stop face detection
def stop_detection():
    global detection_active
    if detection_active:
        print("Stopping face detection...")
        detection_active = False

#### END OF OPENCV ####

#### START OF AI ####

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
                "question": "what is this person's hair color and length",
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
                "question": "What is this person's eye color",
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
        person += " with glasses"
    person += f" with {hair} and {eye} eyes"
    
    return person

# Function to save an image from a URL to a local path
def save_image(url, path):
    response = requests.get(url)
    with open(path, 'wb') as file:
        file.write(response.content)

# Function to generate an AI-enhanced image based on a prompt
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

# Function to generate an image based on the given prompt and style
def generate_epic(input_prompt, path, style, output_dir, image_index=0):
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
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")  # Get current timestamp
    for idx, image_url in enumerate(output):
        epic_image_path = os.path.join(output_dir, f"epic_{image_index}_{timestamp}.png")
        save_image(image_url, epic_image_path)
        if overlay_path:
            overlay_output_path = os.path.join(output_dir, f"epic_framed_{image_index}_{timestamp}.png")
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
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")  # Get current timestamp
    for idx, image_url in enumerate(output):
        sketch_image_path = os.path.join(output_dir, f"sketch_{image_index}_{timestamp}.png")
        save_image(image_url, sketch_image_path)

# Function to generate images after face detection
def generate_images():
    output_dir_epic = "pictures/epic_pictures"
    output_dir_sketch = "pictures/sketch_pictures"
    os.makedirs(output_dir_epic, exist_ok=True)
    os.makedirs(output_dir_sketch, exist_ok=True)
    
    # Loop through each face image and generate AI-enhanced images
    for i in range(numberPlayers):
        image_path = f"face_{i}.jpg"
        found_description = check_person(image_path)
        
        # Generate Epic and Sketch Images
        full_prompt_epic = base_prompt_epic.format(description=found_description)
        print(f"Generating epic image for face {i} with prompt: {full_prompt_epic}")
        generate_image_with_retries(generate_epic, full_prompt_epic, image_path, "Cinematic", output_dir_epic, image_index=i)
        
        full_prompt_sketch = base_prompt_sketch.format(description=found_description)
        print(f"Generating sketch image for face {i} with prompt: {full_prompt_sketch}")
        generate_image_with_retries(generate_sketch, full_prompt_sketch, image_path, "(No style)", output_dir_sketch, image_index=i)

	# After successfully generating images, reset internal states and send reset message
    reset_states()

#### END OF AI ####

#### START OF MAIN LOOP ####

# Create an MQTT client instance
client = mqtt.Client("PythonClient")

# Attach the callback functions to the client
client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.on_disconnect = on_disconnect

# Connect to the broker
try:
    client.connect(BROKER_ADDRESS, PORT)
except Exception as e:
    print(f"Failed to connect: {e}")
    exit(1)

# Start a background loop that listens for messages and handles reconnections
client.loop_start()
start_camera_loop()

# Tell server we are ready to rumble
message = make_message(_cfg_name, "info", 1, 0)
client.publish(serverTopic, message)

# Main loop for processing 
try:
    while True:
        # Check if conditions are met to start detection
        if gameStart == "1" and numberPlayers > 0 and not detection_active and not detection_completed:
            print("Conditions met to start face detection.")
            start_detection()

        # Custom logic for handling other program logic or MQTT events
        time.sleep(1)  # Add a short delay to reduce CPU usage
except KeyboardInterrupt:
    print("Interrupted by user")
    camera_active = False  # Stop the camera thread

# Stop the loop and disconnect cleanly
client.loop_stop()
client.disconnect()
