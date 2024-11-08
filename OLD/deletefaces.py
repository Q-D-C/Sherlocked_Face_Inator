# A quick script to quickly delete all the captured faces when testing

import os

# Get the current directory
current_directory = os.getcwd()

try:
    # Iterate over all files in the current directory
    for filename in os.listdir(current_directory):
        if filename.endswith(".jpg"):
            # Construct the full path to the file
            file_path = os.path.join(current_directory, filename)

            # Delete the file
            os.remove(file_path)
            # print(f"Deleted: {filename}")

    print("Deletion complete.")
except Exception as e:
    print(f"Error: {e}")
