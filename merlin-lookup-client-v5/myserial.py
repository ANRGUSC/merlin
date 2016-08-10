#!/usr/bin/python
"""
myserial.py
This script is used in the backend to set up a serial connection for the server 
and also access data from client and server motes
The script also serves as a remote server to estimate the optimum range for the 
client request files as a lookup list.
"""
from math import exp
from datetime import datetime
import serial
#configure the serial connection
ser = serial.Serial(
	port='/dev/ttyUSB0',\
	baudrate=115200,\
	parity=serial.PARITY_NONE,\
	stopbits=serial.STOPBITS_ONE,\
	bytesize=serial.EIGHTBITS,\
	timeout=0)
print("connected to: " + ser.portstr)
#write the serial data to a file called merlin-lookup.txt
f = open("merlin-lookup.txt", "w")
#ser.write("test line\n");
#merlin parameters
diffs = [];
LookUp = [];
ns_val = 0
lmbd = 0.5
#number of files
Nf = 200  
#size of file
Sf = 0.1
#size of header
SH = 0.02
#size of data
SD = 0.08
# number if bytes to represent index
Ci = 0.1
beta = 2 * Ci * (1 + SH / SD)
MAXcurr = 0.0;
MaxRange = 0;
state = 0;
myTime = datetime.utcnow()
while state is 0:
		#display contents according to the state of the client
		line = ser.read();
		if line:
			print(line),
		if line is '$':
			#client is in the generate_ranges process
			print(line),
		if line is '*':
			#ranges are being sorted
			print("-----------------")
			diffs[:] = []
			LookUp[:] = []
			while True:
				line = ser.read();
				if line is 'e':
					#ranges are sorted
					break
				diffs.extend(line),

			# start calculating Ns lookup
			
			for Ns in range(0, Nf):
				MAXcurr = 0
				for Rb in range(0, len(diffs)):
					alpha = (Ns * Sf) / Nf
					Ur = 0
					for num in range(0, Rb):
						Ur = Ur + int(diffs[num])
					curr = (exp(-1 * beta * Rb) * (1 - exp(-1 * lmbd * alpha * Ur))) / lmbd
					if curr > MAXcurr:
						MAXcurr = curr
						MaxRange = Rb
				LookUp.extend([MaxRange])
		
			print(LookUp)
			s = '';
			while True:
				liner = ser.read();
				if liner is '@':
					#callback received for Ns
					f.write("start: ")
					myTime = datetime.utcnow()
					f.write(datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]);
					f.write("\n")
					while True:
						liner2 = ser.readline();
						if liner2:
							if liner2 is not '\n':
								ns_val = int(liner2)
								print("stored" + liner2)
								ns_to_send = LookUp[ns_val]
								#ns_to_send = 2;
								print(ns_to_send)
								ser.write(chr(ns_to_send + 97))
								ser.write("\n");
								break
				elif liner is '&':
					#received files from server
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
					break;
				elif liner is '#':
					#timer expired
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
					break;
				elif liner:
					print(liner)

ser.close()