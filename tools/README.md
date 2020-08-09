# TOOLS
## MLX90640(AMG8833)_TO_RASPBERRYPI_CONNECTER.PY

A script which is used to collect data on raspberry pi with corresponding sensor. The script can display the image in the termial through SSH. The data will be saved as JSON format:

```json
{
 	"frames":[
    [],
    [],
	...
],
	"raw_frames"(only melexis has this key): [
        [],
        [],
        ...
    ]

}
```

the Melexis script will also save the EEPROM data as eeprome_xxxxxx.json



## ADAFRUIT_MLX90640.PY

The driver for the Melexis sensor. should be put in the folder  ~/.local/lib/phthon3.7/site-packags/ and replace the file with the same name in that folder. 

## labeler/data_labeler.py

A data labeling tool to label the number of people in each frame and the ground truth count of room at every frame. Before running, check that the video folder and filename are correct and also the export location. Then run the program. At each frame, the labeler will ask for two values:
1. the number of people in this frame
2. the count of people in the room until this frame.
At each frame, the input box for the labels will have the previous value as default value, so it is possible to keep pressing Enter to quickily forward the video without reinserting each frame the label. The export format of the label is a json file following this structure:
```json
{
    "num_people": [],
    "room_count": []
}
```
The export file will have the same name as the input video name and will be saved in the given export folder.
