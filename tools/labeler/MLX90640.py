import json

"""
This is a class emulating an MLX90640
"""


class MLX90640:
    def __init__(self, video_location):
        self.location = video_location
        self.current_frame = 0
        self.raw_data = None

        self.__load_raw_data()

    def __load_raw_data(self):
        try:
            with open(self.location) as f:
                self.raw_data = json.load(f)["frames"]
                f.close()
                return True
        except FileNotFoundError:
            return False

    def get_frame(self):
        self.current_frame += 1

        return self.raw_data[self.current_frame - 1]
