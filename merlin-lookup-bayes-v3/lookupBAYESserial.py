#!/usr/bin/python
"""
This python script is used to calculate the possible optimal range request lookup for the requested file ranges.
The estimation is done via bayesian estimate wherein a special case of the equation results in 
exponential distribution.

"""
from math import exp
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
f = open("merlin-lookup-BAYES.txt", "w")
#ser.write("test line\n");
diffs = [];
LookUp = [];
ns_val = 0
lmbd = 0.5
Nf = 200
Sf = 0.1
SH = 0.02
SD = 0.08
Ci = 0.1
beta = 2 * Ci * (1 + SH / SD)
MAXcurr = 0.0;
MaxRange = 0;
state = 0;
numInteractions = 0;
totalTime = 0;
gammaK = 2;
gammaTheta = 0.25;
myTime = datetime.utcnow()
while state is 0:
		
		line = ser.read();
		if line:
			print(line),
		if line is '$':
			print(line),
		if line is '*':
			print("-----------------")
			diffs[:] = []
			LookUp[:] = []
			while True:
				line = ser.read();
				if line is 'e':
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
								f.write("          number of ranges sent: ");
								f.write(str(ns_to_send));
								f.write("\n");
								ser.write(chr(ns_to_send + 97))
								ser.write("\n");
								break
				elif liner is '&':
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
							numInteractions += 1;
							totalTime += timeElapsed.total_seconds();
							lmbd = (gammaTheta * (numInteractions * gammaK)) / (1 + gammaTheta * totalTime);
							f.write("          total time: ");
							f.write(str.format("{0:.3f}", totalTime));
							f.write("          interactions: ");
							f.write(str(numInteractions));
							f.write("          new lambda: ");
							f.write(str(lmbd));
							f.write("\n")
							break;
						print(line2);
						f.write(line2);
					break;
				elif liner is '#':
					while True:
						line3 = ser.read();
						if line3 is "#":
							f.write("\n");
							f.write("timeout end: ")
							f.write(datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]);
							f.write("          ");
							timeElapsed = datetime.utcnow() - myTime;
							f.write(str(timeElapsed));
							numInteractions += 1;
							totalTime += timeElapsed.total_seconds() - 1.5;
							lmbd = (gammaTheta * (numInteractions * gammaK)) / (1 + gammaTheta * totalTime);
							f.write("          total time: ");
							f.write(str.format("{0:.3f}", totalTime));
							f.write("          interactions: ");
							f.write(str(numInteractions));
							f.write("          new lambda: ");
							f.write(str(lmbd));
							f.write("\n")
							break;
						print(line3);
						f.write(line3);
					break;
				elif liner:
					print(liner)
ser.close()