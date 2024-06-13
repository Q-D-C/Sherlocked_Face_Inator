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
