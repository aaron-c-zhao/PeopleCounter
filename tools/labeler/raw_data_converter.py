import os

import numpy as np
from PIL import Image

from tools.labeler.MLX90640 import MLX90640

def convert_to_image(f, min_temp, max_temp):
    """
    Converts the frame to greyscale image
    :param f: the frame with temperature information
    :param min_temp: the minimum temperature (Celsius) mapped to black
    :param max_temp: the maximum temperature (Celsius) mapped to white
    :return: the greyscale image of the frame
    """
    result = Image.new('L', (32, 24), "black")
    for y in range(24):
        for x in range(32):
            result.putpixel((x, y), __temp_to_greyscale(f[32 * (23 - y) + x], min_temp, max_temp))
    # result = Image.new('L', (8, 8), "black")
    # for y in range(8):
    #     for x in range(8):
    #         result.putpixel((y, x), __temp_to_greyscale(f[8 * y + x], min_temp, max_temp))
    result = np.array(result)

    return result.astype('uint8')


def __temp_to_greyscale(temp, min_temp, max_temp):
    if temp <= min_temp:
        return 0
    if temp >= max_temp:
        return 255
    return int((temp - min_temp) / (max_temp - min_temp) * 255)


class RawDataConverter:
    def __init__(self, relative_path, range_min=20, range_max=30):
        """
        Creates a converter that given the path to the video frames coverts temperature data into image.
        :param relative_path: relative path from current file
        :param range_min: the minimum temperature (Celsius) mapped to black
        :param range_max: the maximum temperature (Celsius) mapped to white
        """

        self.min_temp = range_min  # low threshold temp in C
        self.max_temp = range_max  # high threshold temp in C

        # TODO remove temp functionality (DONT DELETE THIS YET)
        self.raw_frame = None

        self.sensor = MLX90640(
        os.path.join(os.path.normpath(os.path.join(os.path.join(os.path.dirname(__file__), os.pardir), os.pardir)),
                         relative_path))
        # self.sensor = GridEye(
        #     os.path.join(os.path.normpath(os.path.join(os.path.join(os.path.dirname(__file__), os.pardir), os.pardir)),
        #                  relative_path))

    def get_frame(self):
        """
        Get a frame (image) from the sensor. Returns None if there are no more frame in the video.
        :return: the next frame
        """
        frame = self.sensor.get_frame()
        # mlx.cleanup() # used once we get the sensor

        if frame is None:
            return None

        img = convert_to_image(frame, self.min_temp, self.max_temp)

        self.raw_frame = frame

        return img
