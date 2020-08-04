import json
import sys
import tkinter as tk
from tkinter import simpledialog

import cv2

from tools.labeler.raw_data_converter import RawDataConverter

"""
1. Look at each TODO and modify the variables to your needs
2. Run the program
3. Press space (any key != from 'e' or Esc) to skip current frame
4. Press 'e' to save frame to test set and input the label (number of people).
   The console will show you the current number of frame there are for each label. (so you can make a balanced test set)
5. Press Esc to quit the program and save to file.

This program will automatically load the export file if it already exists and create a new one from scratch if not.
This allows you to close and save and come back when needed.
"""


def ask_label(n):
    s = simpledialog.askinteger("Frame label", "Insert the number of people in this frame", initialvalue=n)
    return n if s is None else s


def ask_count(n):
    s = simpledialog.askinteger("Count label", "Insert the number of people in the room", initialvalue=n)
    return n if s is None else s


relative_path = 'data/mlx90640/vertical/'
export_file = "../../data/labels/vertical/"

# TODO change name of video file
video_name = 'One_person_25c_09_36_08.json'

rdc = RawDataConverter(relative_path + video_name, 20, 40)
frame_n = 0
num_people = 0
room_count = 0
result = {"num_people": [], "room_count": []}  # if not os.path.isfile(export_file) else eval(open(export_file).read())

ROOT = tk.Tk()
ROOT.withdraw()
cv2.namedWindow("image")
# cv2.resizeWindow("image", 320, 240)

try:
    while True:
        # get a frame
        img = rdc.get_frame()
        # break if there are no more frames
        if img is None:
            break

        # show image in window
        cv2.imshow("image", cv2.resize(img, (320, 240)))

        print("frame " + str(frame_n))
        frame_n += 1

        num_people = ask_label(num_people)
        room_count = ask_count(room_count)

        result["num_people"].append(num_people)
        result["room_count"].append(room_count)

        key = cv2.waitKey(0) & 0xFF

        if key == 27:  # Esc key to stop
            break
        # elif key == ord('e'):  # e to add this frame to test dataset
        #     print("Current number of frames per label = ", counter)
        #     n_people = ask_label()  # ask for number of people in frame
        #     counter[n_people] += 1
        #     result["frames"].append(rdc.raw_frame)
        #     result["labels"].append(n_people)
        #     continue

except FileNotFoundError:
    print("Error! Your progress will be saved.")
finally:
    # write result to file
    json = json.dumps(result)
    f = open(export_file + video_name, "w+")
    f.write(json)
    f.close()

    cv2.destroyWindow("image")
    cv2.waitKey(1)
    cv2.waitKey(1)
    cv2.waitKey(1)

    sys.exit()
