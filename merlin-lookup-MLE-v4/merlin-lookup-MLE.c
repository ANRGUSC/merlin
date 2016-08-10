/**
	* Merlin-lookup-MLE.c
	* This file is the version that implements the MLE criteria to estimate the best range to request. lookupMLEserial.py is the corresponding python script that runs with the code.   
	* This file is used to setup a merlin client that is used to talk to the server regarding the retrieval of missing files. 
	* The code utilizes 4 processes namely, generate_ranges, send_ranges, recv_files and timerout_proc.
	* 3 global variables are used to keep track of the phase the client mote is in.
	* flag : Its used for indication that the initial Ns ranges or files are received
	* state: state of the client based on information received. Ns range, files ranges and files received.
	* 2 callback functions are also used to signal the event threads and modify the flags for processes. 
	* client_recv is a callback function for receiving the broadcast message containing Ns (number of files available). This data is stored in the ns_recv variable.
 	* files_recv is a callback function  for receiving files. The state variable is changed to 2 after the function call.


 */
#include "contiki.h"
#include "dev/serial-line.h"
#include "net/rime.h"
#include "random.h"
#include "dev/leds.h"
#include "dev/uart1.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// flag for if receiving initial Ns or files
static int flag = 0;
static int state = 0;
static int timer_flag = 0;
// binary vector of client's files (50 files)
uint8_t Vc[] = {1,1,0,0,0,1,1,1,1,0,0,1,0,1,0,0,0,0,1,1,
1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,
1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,1,0,
0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,
0,0,0,0,0,0,1,0,0,1,1,1,0,0,0,1,1,1,1,1,
1,1,0,0,0,1,1,1,1,0,0,1,0,1,0,0,0,0,1,1,
1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,
1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,1,0,
0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,
0,0,0,0,0,0,1,0,0,1,1,1,0,0,0,1,1,1,1,1};

// constants
int Nf = sizeof(Vc)/sizeof(Vc[0]); // number of files
double Sf = .10; // size of file
double SH = .2; // size of header
double SD = .10; // size of data
double Ci = .1; // number of bytes to represent index

int files_expecting = 0;
int files_received = 0;

uint8_t ns_range = 0;

uint8_t starts[sizeof(Vc)/sizeof(Vc[0])] = {0};
uint8_t ends[sizeof(Vc)/sizeof(Vc[0])] = {0};
uint8_t diffs[sizeof(Vc)/sizeof(Vc[0])] = {0};

uint8_t Ns_Lookup[sizeof(Vc)/sizeof(Vc[0])] = {0};

uint8_t num_ranges = 0;




// MLE
double lambda = 0.5;
int num_samples = 1;
double current_sample = 0.0;
static clock_time_t start_time;
static clock_time_t end_time;


// event to send once receives number of available files from server
static process_event_t ns_ready;
// event to send once received files
static process_event_t files_ready;

static process_event_t gen_ranges;


/*-----------------------------------*/
// naming processes
PROCESS(generate_ranges, "Generate Ranges");
//PROCESS(recv_lookup, "Receive Ns Lookup");
PROCESS(send_ranges, "Send Ranges");
PROCESS(recv_files, "Receive Files");
PROCESS(timerout_proc, "Timerout process");

// starting processes
AUTOSTART_PROCESSES(&generate_ranges,&send_ranges,&recv_files);

/*-----------------------------------*/


// callback for receiving Ns
static void client_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	printf("%");
	//start_time = clock_time();
	//state = 1;
	// flag for receiving Ns initially
	if (flag==1) {
		leds_on(LEDS_BLUE);
		leds_off(LEDS_RED);
		// storing Ns from packet
		uint8_t ns_recv = *(uint8_t *)packetbuf_dataptr();
		// for debugging purposes
		printf("Ns '%d'\n", ns_recv);
		leds_off(LEDS_BLUE);

		printf("@");
		printf("%d\n", ns_recv);
	}
	else {
		leds_on(LEDS_RED);

	}
	
}

// callback for receiving files
static void files_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	process_exit(&timerout_proc);
	// storing file received
	uint8_t file_recv = *(uint8_t *)packetbuf_dataptr();

	state = 2;
	//printf("state 2");

	// debugging
	printf("%d", file_recv);

	// allocating event
	files_ready = process_alloc_event();

	// sending event and file number received to recv_files process
	process_post(&recv_files, files_ready, &file_recv);

}

// naming broadcast callbacks
static const struct broadcast_callbacks client_call = {client_recv};
static const struct broadcast_callbacks files_call = {files_recv};

// naming broadcasts
static struct broadcast_conn cli_broadcast;
static struct broadcast_conn files_broadcast;

// ********************************** //

PROCESS_THREAD(generate_ranges, ev, data)
{
	PROCESS_EXITHANDLER(goto exit;)

	PROCESS_BEGIN();
  
	while(1) {

		if (state == 0) {
		printf("$");

		// counter
		int i=0;

		// find number of needed ranges
		for (i=0; i<sizeof(Vc)/sizeof(Vc[0]); i++) {
			if (i==0 && Vc[i]==0) {
				num_ranges++;
				continue;
			}
			if ((Vc[i-1]==1) && (Vc[i]==0)) {
				num_ranges++;
			}
			starts[i] = 0;
			ends[i] = 0;
			diffs[i] = 0;
		}
		
		// count number of start and end indices (for debugging, make sure equal)
		int num_starts = 0;
		int num_ends = 0;

		// store start and end indices in array
		for (i=0; i<sizeof(Vc)/sizeof(Vc[0]); i++) {
			if (i==0 && Vc[i]==0) {
				starts[num_starts] = i;
				num_starts++;
				continue;
			}
			if ((Vc[i-1]==1) && (Vc[i]==0)) {
				starts[num_starts] = i;
				num_starts++;
			}
			else if ((Vc[i]==1) && (Vc[i-1]==0)) {
				if(i==0) {
					continue;
				}
				ends[num_ends] = i;
				num_ends++;
			}
		}

		if (num_starts == 0) {
			printf("\n\n##### DONE #####\n\n");
			break;
		}

		// store range sizes
		for (i=0;i<num_starts;i++) {
			diffs[i] = ends[i]-starts[i];
		}

		// print sizes of ranges for debugging
		printf("ranges: ");

		for (i=0;i<num_starts;i++) {
			if(i==num_starts-1) {
				printf("%d\n", diffs[i]);
			}
			else {
				printf("%d ,", diffs[i]);
			}
		}

		// variables for sorting range sizes
		int c;
		int d;
		uint8_t swap=0;
		uint8_t schange=0;
		uint8_t echange=0;

		printf("ranges starts ends %d %d %d", num_ranges, num_starts, num_ends);
	
		// sort	
		for (c=0; c<num_starts-1;c++) {
			for (d=0;d<num_starts-1;d++) {
				if (diffs[d] < diffs[d+1]) {
					swap = diffs[d];
					diffs[d]=diffs[d+1];
					diffs[d+1]=swap;
	
					schange = starts[d];
					starts[d]=starts[d+1];
					starts[d+1]=schange;
	
					echange = ends[d];
					ends[d]=ends[d+1];
					ends[d+1]=echange;
				}
			}
		}

		printf("high diff: %d", diffs[0]);

		// print ranges sorted for debugging
		printf("*");		

		for (i=0;i<num_starts;i++) {
			if(i==num_starts-1) {
				printf("%d", diffs[i]);
			}
			else {
				printf("%d", diffs[i]);
			}
		}

	
		printf("e");
		state = 1;
		flag = 1;
		PROCESS_WAIT_EVENT_UNTIL(ev == gen_ranges);
		}

  	}
  	exit:
		;
  		PROCESS_END();
}

// ***** //
/*

*/


PROCESS_THREAD(send_ranges, ev, data)
{
	// exit handler
	PROCESS_EXITHANDLER(broadcast_close(&cli_broadcast);)

	// begin process
	PROCESS_BEGIN();

	// open broadcast
	broadcast_open(&cli_broadcast, 128, &client_call);

	while(1) {
		// wait until received number of available files from server
				
		PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);
		files_expecting = 0;
		files_received = 0;
		char* tester = (char *)data;
		printf("'%d'", ((uint8_t)tester[0] - 97));

		ns_range = (uint8_t)tester[0] - 97;
		printf("%d", ns_range);

		flag = 0;
		int i = 0;
		// send ranges
		for (i=0;i<ns_range;i++) {
			uint8_t range[] = {starts[i], ends[i]};
			files_expecting += ends[i] - starts[i];
			packetbuf_copyfrom(range, 10);
			broadcast_send(&cli_broadcast);
			// debugging
			printf("range %d %d sent\n", starts[i], ends[i]);
		}
		process_start(&timerout_proc, NULL);
		// flag to signal client is ready to receive files
		flag = 2;

	}
	// end process
	PROCESS_END();
}


PROCESS_THREAD(recv_files, ev, data)
{
	// exit handler
	PROCESS_EXITHANDLER(broadcast_close(&files_broadcast);)
	// begin process
	PROCESS_BEGIN();
	// open broadcast
	broadcast_open(&files_broadcast, 129, &files_call);

	while(1) {
		// wait until receive files
		PROCESS_WAIT_EVENT_UNTIL(ev == files_ready);
		// flag to signal client is ready to start cycle over again
		// store received file
		uint8_t recv_file = *(uint8_t *)data;
		files_received++;
		
		// counter
		int i=0;
		// if the file received was actually missing, change Vc to reflect that additional file
		for (i=0;i<sizeof(Vc)/sizeof(Vc[0]);i++) {
			if(i==recv_file && Vc[i]==0) {
				Vc[i]=1;
			}
		}
		
		// if got all files
		if (files_received == files_expecting) {
			state = 0;
			
			int vcounter = 0;
			int filesRecv = 0;
			for (vcounter=0;vcounter<sizeof(Vc)/sizeof(Vc[0]);vcounter++) {
				if (Vc[vcounter] == 1) {
					filesRecv++;
				}
			}
			printf("&");
			printf("%d", filesRecv);
			printf("&");
			printf("\n");
			printf("received all files!\n");
			

			process_exit(&timerout_proc);
			

			gen_ranges = process_alloc_event();

			// send event and Ns to client_broadcast
			process_post(&generate_ranges, gen_ranges, NULL);
		}
		else {
			process_exit(&timerout_proc);
			process_start(&timerout_proc, NULL);
		}

	}
	// end process
	PROCESS_END();
}

PROCESS_THREAD(timerout_proc, ev, data)
{

	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)

	PROCESS_BEGIN();
  
	while(1) {
		// wait until receive warning message
		etimer_set(&et, CLOCK_SECOND * 1.5);

		// wait for the timer to expire
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		
		state = 0;
		int vcounter = 0;
		int filesRecv = 0;
		for (vcounter=0;vcounter<sizeof(Vc)/sizeof(Vc[0]);vcounter++) {
			if (Vc[vcounter] == 1) {
				filesRecv++;
			}
		}
		printf("#");
		printf("%d", filesRecv);
		printf("#");
		printf("\n");
		// allocating event
		gen_ranges = process_alloc_event();

		// send event and Ns to client_broadcast
		process_post(&generate_ranges, gen_ranges, NULL);
		break;

  	}
  	exit:
		;
  		PROCESS_END();
}
