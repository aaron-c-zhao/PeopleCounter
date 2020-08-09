# TOOLS
## MLX90640(AMG8833)_TO_RASPBERRYPI_CONNECTER.PY

A script which is used to collect data on raspberry pi with corresponding sensor. The data will be saved as JSON format:

```json
[
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

]
```

the Melexis script will also save the EEPROM data as eeprome_xxxxxx.json



## ADAFRUIT_MLX90640.PY

The driver for the Melexis sensor. should be put in the folder  ~/.local/lib/phthon3.7/site-packags/ and replace the file with the same name in that folder. 

