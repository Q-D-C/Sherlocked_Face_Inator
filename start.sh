#!/bin/bash

# Activate the Python environment
source /home/pi/Sherlocked_Face_Inator/venv/bin/activate
export REPLICATE_API_TOKEN=your_replicate_api_token_here

# Start the Python faceinator script
python3 /home/pi/Sherlocked_Face_Inator/faceinator.py
