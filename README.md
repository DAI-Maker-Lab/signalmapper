# signalmapper
signalMapper is a simple device based on a Seeeduino Stalker v3 and an Adafruit FONA 808 breakout board. signalMapper polls the cellular network associated with the SIM in the FONA for signal strength and records that strength along with the latitude and longitude of the reading. This data can then be easily uploaded to a site like cartoDB.com to generate a chloropleth or other map of cellular signal strength. The project was developed to help field teams scope sites for FONA-based sensor arrays such as the Hidrosónico. 

An Arduino with an SD card breakout board or shield could be used instead of the Stalker, though some pin changes may be necessary depending on the board. 
