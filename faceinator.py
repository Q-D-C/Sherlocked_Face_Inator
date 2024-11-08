import cv2
import json
import socket
import paho.mqtt.client as mqtt
import threading
import time

camera_active = False
camera_lock = threading.Lock()

BROKER_ADDRESS = "localhost"  # Use your broker address
PORT = 1883  # The default MQTT port, adjust as needed
TOPIC_SUBSCRIBE = "alch/faceinator"
TOPIC_PUBLISH = "alch"
serverTopic = "alch"
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
            reset_states()

    except Exception as e:
        print(f"Error handling message: {e}")

# Update the reset_states function to properly handle game start reset
def reset_states():
    global gameStart, numberPlayers, detection_active, detection_completed
    gameStart = ""
    numberPlayers = 0
    detection_active = False
    detection_completed = False
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

# Flag to control whether detection is active
detection_active = False
detection_completed = False

BLURRYNESS_THRESHOLD = 100.0  # Define a suitable threshold for blurriness

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

def camera_loop():
    global camera_active, detection_active, detection_completed, numberPlayers

    print("Starting camera loop...")

    # Load the pre-trained Haar Cascade for face detection
    try:
        face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
        if face_cascade.empty():
            print("Error: Could not load Haar Cascade. Please check if the XML file is available.")
            return
    except Exception as e:
        print(f"Error initializing Haar Cascade: {e}")
        return

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
            # Convert to grayscale for face detection
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            # Detect faces in the frame
            try:
                faces = face_cascade.detectMultiScale(
                    gray,
                    scaleFactor=1.1,  # Lower scaleFactor to detect more potential faces
                    minNeighbors=8,   # Increase minNeighbors to reduce false positives
                    minSize=(50, 50)  # Minimum size for a face to be considered
                )
                print(f"Faces detected: {len(faces)}")
            except Exception as e:
                print(f"Error during face detection: {e}")
                detection_active = False
                continue

            if len(faces) > 0:
                print(f"Looking for {numberPlayers} amount of faces")
                for i, (x, y, w, h) in enumerate(faces):
                    # Add padding to the face region
                    x_start = max(x - padding, 0)
                    y_start = max(y - padding, 0)
                    x_end = min(x + w + padding, frame.shape[1])
                    y_end = min(y + h + padding, frame.shape[0])

                    # Extract the face ROI with padding
                    face = frame[y_start:y_end, x_start:x_end]
                    
                    # Draw rectangle around detected face (for visualization)
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

        # Display the frame (comment out if running headless)
        try:
            cv2.imshow('Face Detection', frame)
        except Exception as e:
            print(f"Error displaying frame: {e}")
            break

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
