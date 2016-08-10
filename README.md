# MERLIN
## Synopsis
This Code is the implementation of the MERLIN protocol.The primarily hardware used to test the protocol was on Tmote Sky Devices with Contiki OS. 
This was used to factor in the computational time sensitivity of the implementation.  
Here the file availability is denoted with an array of ones and zeros where one represents the file is present and zero, it’s not.
At the beginning of the interaction, once the client receives the server’s number of available files, the client uses its file indicator array to determine 
all of its needed file ranges and sorts them in decreasing order.With these ranges and the number of available server files, the client can begin to use 
the MERLIN equation to calculate the expected number of files received based on the number of ranges it requests. Once the client has iterated over every 
possible number of ranges to request, it chooses the range that maximizes the expected value of files received. Finally, the client requests those ranges from the server
 in the form of start and end index pairs. Once the server receives the range pairs from the client, the server uses its own indicator array to check 
if it has the files specified in the client’s requested ranges. For any of the files it has within these ranges, the server transmits the files back to the client.
## Installation
* This Protocol was tested on Instant Contiki 3.0 along with Tmote Sky Devices. 
* Please ensure that you have the latest version of msp430-gcc (GCC) 4.6.3  installed to support the tmote sky devices.
* Please ensure that the serial interface of Host Machine is also connect via Virtual Serial Ports of the VM machine.
* To get started please copy the code in the rime folder of home/Contiki-3.0/examples/rime. 
   
 The protocol has been implemented with five versions to test its performance with various precision and speed. 
 The merlin-lookup-server.c code is the server code that works with all five versions of the protocol implementation.
 There are 5 other python scripts and 5 C programs to implement various versions of the protocol.
    * The first version of the protocol can be identified with the files merlin-client.c  and defaultserial.py 
 This version tries to estimate the optimal request range of files with the client mote. The python script utilizes the serial interface API to read and log the results of the test.

    * The second version of the protocol can be identified with the files merlin-client-naive.c and avgserial.py 
 This version of the protocol just requests the single largest file each time. Here again the computation is handled by the client mote itself.

    * The third version of the protocol is identified with the files merlin-lookup-BAYES.c and lookupBAYESserial.py 
 In this version, This python script (which acts as a remote server) is used to calculate the possible optimal range request lookup for the requested file ranges. 
 The estimation is done via bayesian estimate     wherein a special case of the equation results in exponential distribution. In all calculations 
 we assume the encounter durations follow an exponential distribution with a mean of two seconds. 
 Therefore, the  rate parameter lambda for the exponential distribution and the MERLIN equation is set to 0.5.

   * The fourth version of the protocol is identified with the files merlin-lookup-MLE.c and python script lookupMLEserial.py. 
 In this version, the python script acts as a remote server with creates a lookup table for the possible ranges of files the client mote can request.
 The estimation is done with the help of maximum likelihood estimation parameter.
 The final version of the protocol contains the files merlin-lookup-client.c and myserial.c . This version is similar to the pervious version except the version does not have a learning component, just the lookup table for faster message exachanges between server and client.   

## Compilation and Running Code:
On the command line please write

    make TARGET=sky merlin-lookup-server.upload

This will compile the server code to the Tmote sky device.  Now with the server mote containing batteries, will run as standalone device. 
Similar to the server mote, compile and upload code to the client mote, it should be plugged in the usb port. The python script corresponding to the protocol version should be run to monitor the activity and collect results.
    The order of execution:
     1. upload the merlin-lookup-server code into the server mote
     2. upload your choice of client code into the client mote
     3. plug the client into your laptop USB
     4. from your laptop run the associated python code
     5. once you start the python script, hit reset on both motes to restart the interactions so that the full interaction is monitored on the computer

Then once you stop the python code it should create the output files with all of the information about the interactions


File description:
File 1:     myserial.py 
This is used as a backend for the MERLIN protocol data collection from client and server interactions.
This python script is used to PySerial API to access the serial port. 

Parameters :
•	port – Device name ttyUSB0.
•	baudrate (int) – Baud rate of 115200
•	bytesize – Number of data bits.  EIGHTBITS
•	parity – Enable parity checking: PARITY_NONE 
•	stopbits – Number of stop bits: STOPBITS_ONE 
•	timeout (float) – Set a read timeout value: 0
File : merlin-lookup-server.c
 This file is used to configure one of the motes as the server that contains the relevant file ranges for the clients. 
 This Server initially sets up a “beacon” that sends the number of files (ns) available for the client to receive.  Upon receiving a callback event from the 
 client the server mote prepares to send the requested files over to the client. This occurs until the client and server are only within the “range” which 
 is simulated by a timer mechanism to limit their interaction.
 The server contains three processes, server_broadcast, send_files and timerout_proc to perform the above functions of the server.
 
Process : server_broadcast 
 The “server_broadcast” process , periodically (every 1.5 seconds ) sends the number of available files which is represented by the binary vector Vs . 
 A flag variable is used to start/stop the broadcast of available files.

Process: send_files
 The “send_files” process waits specifically for the callback from the client after it has received the broadcast. The callback function “server_recv”  
 is used to receive the desired range of files requested by  the client and also signal the event that the client is ready to receive the requested ranges. 
 The event is denoted with the “ranges_ready” variable. The requested ranges are stored in the “range_recv” pointer. This process then compares the received 
 indexes with the one in the server to see the files ready to be transferred.

Process: timerout_proc
 This process is used to simulate the limited interaction between the Server and Client. The Timer is started and is run for one second before it generates the
 timerout event which blocks broadcast process.

Merlin-lookup-client.c
This file is used to setup a merlin client that is used to talk to the server regarding the retrieval of missing files. 
The code utilizes 4 processes namely, generate_ranges, send_ranges, recv_files and timerout_proc.
3 global variables are used to keep track of the phase the client mote is in 
Flag : Its used for indication that the initial Ns ranges or files are received
State: state of the client based on information received. Ns range, files ranges and files received.
Timer_Flag: indication of timerout process
2 callback functions are also used to signal the event threads and modify the flags for processes. 
client_recv is a callback function for receiving the broadcast message containing Ns (number of files available). This data is stored in the ns_recv variable.
 files_recv is a callback function  for receiving files. The state variable is changed to 2 after the function call.

Process: generate_ranges
This process finds the number of ranges needed for the client. This is stored in the variable num_ranges.  
The process also stores the start and end index of the missing file ranges and stores them in an array called starts[] and ends[].
After the ranges and their start and end indices are determined, we sort the range according to the sizes .



 Process: send_ranges
In this process , we begin broadcasting the ranges to the server after waiting for the event serial_line_event_message . The flag is set to 2 after the required file ranges have been sent.
Process: recv_files
This process , the event files_ready is a signal for the process to begin its execution. 
Process: timerout_proc
This process is used to simulate the limited interation between the client and server.
The timer is set to a 1.5 seconds until expiration. After which all flags are reset to zero and the process generate_ranges is signalled to asynchronous schedule.

## Reference
Amber Bhargava, Spencer Congero, Timothy Ferrell, Alex Jones, Leo Linsky, Jayashree Mohan, and Bhaskar Krishnamachari, [“Optimizing Downloads over Random Duration Links in Mobile Networks“](http://anrg.usc.edu/www/papers/Merlin_ICCCN2016_PID1178168.pdf), 25th International Conference on Computer Communication and Networks, August 2016.


