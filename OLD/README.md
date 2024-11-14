## Configure WiFi Networks on Static IP

### If Setting Up at the Location:

```bash
sudo nmcli dev wifi connect 'SSID' password 'password'
sudo nmcli connection modify SSID ipv4.method manual ipv4.addresses "192.168.1.100/24" ipv4.gateway "192.168.1.1" ipv4.dns "8.8.8.8 8.8.4.4"
```

### If Setting Up Off-Site:
```bash
sudo nmcli dev wifi connect 'home-SSID' password 'home-password'
sudo nmcli connection add type wifi ifname wlan0 con-name remoteSSID ssid 'remote-SSID'
sudo nmcli connection modify RemoteSSID wifi-sec.key-mgmt wpa-psk
sudo nmcli connection modify RemoteSSID wifi-sec.psk 'remote-password'
sudo nmcli connection modify RemoteSSID ipv4.method manual ipv4.addresses "192.168.1.100/24" ipv4.gateway "192.168.1.1" ipv4.dns "8.8.8.8 8.8.4.4"
sudo nmcli connection up remoteSSID
```


## Update your Raspberry Pi and install Git:

```sh
sudo apt update
sudo apt upgrade -y
sudo apt install git -y
```

### Clone the Sherlocked_Face_Inator repository:

```sh
git clone https://github.com/Q-D-C/Sherlocked_Face_Inator.git
cd Sherlocked_Face_Inator
```

## Install Dependencies for MQTT

Install the Mosquitto and json development libraries:

```sh
sudo apt install libmosquitto-dev -y
sudo apt install nlohmann-json3-dev -y
```

## Compile `mqtt.cpp`

Compile the `mqtt.cpp` file:

```sh
gcc -std=c++14 -g mqtt.cpp -o mqtt -lstdc++ -lmosquitto
```

## Install Dependencies for `herken.cpp`

Install OpenCV development libraries:

```sh
sudo apt install libopencv-dev -y
```

Compile the `herken.cpp` file:

```sh
g++ -o herken herken.cpp `pkg-config --cflags --libs opencv4` -std=c++14
```

## Set Up Python Environment for `generatePerson.py`

Install `python3-venv` if not already installed:

```sh
sudo apt install python3-venv -y
```

Create and activate a virtual environment:

```sh
python3 -m venv venv
source venv/bin/activate
```

Install the required Python packages:

```sh
pip install replicate requests pillow
```

## Set Up Replicate API

To use the Replicate API, you need an API token. Follow these steps to set it up:

1. Sign up for a Replicate account at [replicate.com](https://replicate.com/).
2. Obtain your API token from the [API token page](https://replicate.com/account/api-tokens).

Set the API token as an environment variable:

```sh
export REPLICATE_API_TOKEN=your_replicate_api_token_here
```

Replace `your_replicate_api_token_here` with your actual API token.

## Create Required Text Files

Create the necessary text files:

```sh
touch done.txt scanningComplete.txt numberPlayers.txt gameStart.txt
```

## Start the MQTT Broker

Install and start the Mosquitto MQTT broker:

```sh
sudo apt update
sudo apt install mosquitto mosquitto-clients -y
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto
```

## Set up Shared Folder

Install the Samba package on your Raspberry Pi to enable file sharing:

```bash
sudo apt install samba samba-common-bin -y
```

Create a folder that will be shared across the network and set the appropriate permissions for the shared folder so that it can be accessed and modified by anyone:

```bash
mkdir pictures
mkdir pictures/epic_pictures
mkdir pictures/sketch_pictures
sudo chmod 777 pictures
```

Edit the Samba configuration file:

```bash
sudo nano /etc/samba/smb.conf
```

At the bottom of the file, add the following configuration:

```ini
[FaceInator]
path = /home/pi/Sherlocked_Face_Inator/pictures
browseable = yes
writeable = yes
guest ok = yes
force user = pi
create mask = 0777
directory mask = 0777
public = yes
```

- path: The directory to be shared.
- writeable: Allows users to write to the folder.
- create mask and directory mask: Ensure files and directories are fully accessible.
- public and guest ok: Allow guest access without requiring authentication.

Save and close the file by pressing Ctrl + X, then Y, and press Enter.

Restart the Samba service to apply the new configuration:

```bash
sudo systemctl restart smbd
```

You’ll need the Raspberry Pi’s IP address to access the shared folder:

```bash
hostname -I
```
Take note of the IP address (e.g., 192.168.1.50).

On your macOS device:

Open Finder.
Press Cmd + K or go to Go > Connect to Server.
Enter the following in the "Server Address" field:

```bash
smb://<your-raspberry-pi-ip-address>/shared_folder
```

Click Connect.
Select Guest if prompted for a username and password.
The shared folder will now appear in Finder, and you can read/write files to it.

To automatically mount the shared folder on macOS every time you log in:
Connect to the folder using the steps above.
Go to System Preferences > Users & Groups.
Select your user and go to the Login Items tab.
Drag the mounted shared folder from your desktop into the Login Items list.

## Create and Enable Service for the Project
Create a systemd service to manage the MQTT and OpenCV project files:

```bash
sudo nano /etc/systemd/system/face_inator.service
```

Add the following to the face_inator.service:

```ini
[Unit]
Description=Run Sherlocked_Face_Inator at boot
After=network.target

[Service]
ExecStart=/home/pi/Sherlocked_Face_Inator/start.sh
WorkingDirectory=/home/pi/Sherlocked_Face_Inator
StandardOutput=inherit
StandardError=inherit
Restart=always
User=pi

[Install]
WantedBy=multi-user.target
```

- ExecStart: Path to a start.sh script that initiates the components.
- Restart: Ensures the service automatically restarts if it fails.

Create the start.sh Script. This script will handle starting necessary components like MQTT, OpenCV, and any other processes.

```bash
nano /home/pi/Sherlocked_Face_Inator/start.sh
```

Add the following content:

```bash
#!/bin/bash

# Start the Mosquitto MQTT broker
mosquitto &

# Start the MQTT process
/home/pi/Sherlocked_Face_Inator/mqtt &

# Start the OpenCV process
/home/pi/Sherlocked_Face_Inator/herken &

# Start the Python process
source /home/pi/Sherlocked_Face_Inator/venv/bin/activate
export REPLICATE_API_TOKEN=your_replicate_api_token_here
python3 /home/pi/Sherlocked_Face_Inator/generatePerson.py
```

Make it executable:

```bash
chmod +x /home/pi/Sherlocked_Face_Inator/start.sh
```

Reload systemd and enable the new service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable face_inator.service
sudo systemctl start face_inator.service
sudo systemctl status face_inator.service
```

Schedule a daily reboot using cron:

```bash
sudo crontab -e
```

Add the following line to reboot the system at midnight:

```bash
0 0 * * * /sbin/reboot
```

