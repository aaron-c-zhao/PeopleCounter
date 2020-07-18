import sys
import math
import time
import busio
import board
import numpy as np
from scipy.interpolate import griddata
from colour import Color
import adafruit_mlx90640

import json
import os
from datetime import datetime

# base path 
BASE_PATH = os.environ['PWD']

# count the frames in this scene
frame_counter = 0;

I2C_BUS = busio.I2C(board.SCL, board.SDA, frequency=400000)

# low range of the sensor (this will be blue on the screen)
MINTEMP = 20.0
# high range of the sensor (this will be red on the screen)
MAXTEMP = 40.0
COLORDEPTH = 1024
SENSOR = adafruit_mlx90640.MLX90640(I2C_BUS)

SENSOR.refresh_rate = adafruit_mlx90640.RefreshRate.REFRESH_16_HZ

# pylint: disable=invalid-slice-index
POINTS = [(math.floor(ix / 8), (ix % 8)) for ix in range(0, 64)]
GRID_X, GRID_Y = np.mgrid[0:7:32j, 0:7:32j]
# pylint: enable=invalid-slice-index

# sensor is an 8x8 grid so lets do a square
HEIGHT = 240
WIDTH = 240

# the list of colors we can choose from
BLUE = Color("indigo")
COLORS = list(BLUE.range_to(Color("red"), COLORDEPTH))

# create the array of colors
COLORS = [(int(c.red * 255), int(c.green * 255), int(c.blue * 255)) for c in COLORS]
CONSOLE_COLORS = [
    17,
    18,
    19,
    20,
    21,
    57,
    93,
    129,
    165,
    201,
    200,
    199,
    198,
    197,
    196,
    202,
    208,
    214,
    220,
]


def map_value(x_value, in_min, in_max, out_min, out_max):
    """Maps value of the temperature to color"""
    return (x_value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min


def print_there(console_x, console_y, text, color):
    """ Outputs a colored text to console at coordinates """
    sys.stdout.write("\x1b7\x1b[48;5;%dm" % (color))
    sys.stdout.write("\x1b7\x1b[%d;%df%s\x1b8" % (console_x, console_y, text))

# ask the user for the name of the directory

print('Enter the name of the new directory, press enter to use the current directory:')

name_collision = False;
while not name_collision:
    try:
        dir = input()
        path = os.path.join(BASE_PATH, dir)
        os.makedirs(path)
        name_collision = True
    except OSError:
        print("Directory \"{}\" exists, do you want to use it? y or n".format(dir))
        option = input()
        if option.casefold().startswith('y'):
            name_collision = True
        else:
            print("Please enter the directory name again:")


   

# let the sensor initialize
time.sleep(0.1)

COLOR_RANGE = 1

file_name = ""

while True:
    print("Enter the file name:")
    file_name = input()
    now = datetime.now()
    file_name = file_name + "_" + now.strftime("%H_%M_%S") + '.json'
    frame = {}
    frame_counter = 0
    print("Start recording")
    PIXELS = [0] * 768
    try:
        while True:
            # read the pixels
            try:
                SENSOR.getFrame(PIXELS)
            except ValueError:
                continue
            frame_counter += 1
            frame['frame' + str(frame_counter)] = PIXELS
            
            PIXELS = [map_value(p, MINTEMP, MAXTEMP, 0, COLORDEPTH - 1) for p in PIXELS]
            
            # perform interpolation
            #BICUBIC = griddata(POINTS, PIXELS, (GRID_X, GRID_Y), method="None")

            # do not perform interpolation
            BICUBIC = np.array(PIXELS).reshape(24, 32)
            
            MAXPIXEL = 0
            MINPIXEL = 0

            # draw everything
            Y_CONSOLE = 2
            for ix, row in enumerate(BICUBIC):
                x_console = 2
                for jx, pixel in enumerate(row):
                    color_index = 0
                    if COLOR_RANGE != 0:
                        color_index = int(round((pixel - MINPIXEL) / COLOR_RANGE))
                    if color_index < 0:
                        color_index = 0
                    if color_index > len(CONSOLE_COLORS) - 1:
                        color_index = len(CONSOLE_COLORS) - 1
                    print_there(x_console, Y_CONSOLE * 2 - 2, "  ", CONSOLE_COLORS[color_index])
                    if pixel > MAXPIXEL:
                        MAXPIXEL = pixel
                    if pixel < MINPIXEL:
                        MINPIXEL = pixel
                    x_console += 1
                Y_CONSOLE += 1
            sys.stdout.flush()
            HEAT_RANGE = MAXPIXEL - MINPIXEL
            COLOR_RANGE = HEAT_RANGE / len(CONSOLE_COLORS)
    except KeyboardInterrupt:
        print("Stop recording and writing to the file {}".format(file_name))
        file_name = os.path.join(path, file_name) 
        with open(file_name, "w") as outfile:
            json.dump(frame, outfile, indent = 4)
