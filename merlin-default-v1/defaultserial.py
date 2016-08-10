#!/usr/bin/python
"""
defaultserial.py

This python script utilizes the naive version of the protocol.
The script simply outputs the data from the motes depending on the phase of the protocol their are in.
"""
from datetime import datetime
import serial
ser = serial.Serial(
	port='/dev/ttyUSB0',\
	baudrate=115200,\
	parity=serial.PARITY_NONE,\
	stopbits=serial.STOPBITS_ONE,\
	bytesize=serial.EIGHTBITS,\
	timeout=0)
print("connected to: " + ser.portstr)
f = open("merlin-default.txt", "w")
ser.write("test line\n");
myTime = datetime.utcnow()
while True:
		line = ser.read();
		if line is "*":
			print("start");
			myTime = datetime.utcnow()
			f.write("start: ")
			f.write(datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]);
			f.write("\n")
		elif line is "$":
			while True:
				line2 = ser.read();
				if line2 is "$":
					print("full end");
					f.write("\n");
					f.write("full end: ");
					f.write(datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]);
					f.write("          ");
					timeElapsed = datetime.utcnow() - myTime;
					f.write(str(timeElapsed));
					f.write("\n")
					break;
				print(line2);
				f.write(line2);
		elif line is "#":
			while True:
				line3 = ser.read();
				if line3 is "#":
					f.write("\n");
					f.write("timeout end: ")
					f.write(datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]);
					f.write("          ");
					timeElapsed = datetime.utcnow() - myTime;
					f.write(str(timeElapsed));
					f.write("\n")
					break;
				print(line3);
				f.write(line3);


f.close()
ser.close()