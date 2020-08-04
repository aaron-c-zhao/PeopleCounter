import json
import sys
import tkinter as tk
from tkinter import simpledialog

import cv2

from tools.labeler.raw_data_converter import RawDataConverter

"""
1. Change the video you want to label (marked by the TODO)
2. Run the program (data_labeler.py)
3. Insert the number of people in this frame and confirm (the last number will be in the text field by default) 
4. Insert the count of people in the room and confirm (the last number will be in the text field by default)
5. Continue until all frames are labeled (you can keep pressing enter if no changes are to be made to labels)
"""


def ask_label(n):
    s = simpledialog.askinteger("Frame label", "Insert the number of people in this frame", initialvalue=n)
    return n if s is None else s


def ask_count(n):
    s = simpledialog.askinteger("Count label", "Insert the count of people in the room", initialvalue=n)
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
