/**
* Merlin-client-naive.c
	* This is the version of merlin client which considers largest of the file ranges.
 	* This file is used to setup a merlin client that is used to talk to the server regarding the retrieval of missing files. 
	* The code utilizes 3 processes namely, client_broadcast, recv_files and timerout_start.
	* 2 global variables are used to keep track of the phase the client mote is in.
	* flag : Its used for indication that the initial Ns ranges or files are received
	* state: state of the client based on information received. Ns range, files ranges and files received.
	* 2 callback functions are also used to signal the event threads and modify the flags for processes. 
	* client_recv is a callback function for receiving the broadcast message containing Ns (number of files available). This data is stored in the ns_recv variable.
 	* files_recv is a callback function  for receiving files. The state variable is changed to 2 after the function call.
*/
#include "contiki.h"
#include "net/rime.h"
#include "random.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// flag for if receiving initial Ns or files
static int flag = 1;
// binary vector of client's files (50 files)
int Vc[] = {1,1,0,0,0,1,1,1,1,0,0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,1,0,0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,1,0,0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,1,0,0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,1,1,0,0,0,1,1,1,1,1};

int state = 0;

// constants
int Nf = sizeof(Vc)/sizeof(Vc[0]); // number of files
double Sf = .10; // size of file
double SH = .2; // size of header
double SD = .10; // size of data
double Ci = .1; // number of bytes to represent index

uint8_t files_expecting = 0;
uint8_t files_received = 0;

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

static process_event_t timer_start;

/*-----------------------------------*/

// naming processes
PROCESS(client_broadcast, "Client Broadcast");
PROCESS(recv_files, "Receive Files");
PROCESS(timerout_start, "Start Timerout");

// starting processes
AUTOSTART_PROCESSES(&client_broadcast, &recv_files);

/*-----------------------------------*/

// callback for receiving Ns
static void client_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	start_time = clock_time();
	state = 1;
	// flag for receiving Ns initially
	if (flag==1) {
		// storing Ns from packet
		uint8_t ns_recv = *(uint8_t *)packetbuf_dataptr();
		// for debugging purposes
		
		printf("*");
		printf("Ns received from %d.%d: '%d'\n",
			from->u8[0], from->u8[1], ns_recv);

		// allocating event
		ns_ready = process_alloc_event();

		// send event and Ns to client_broadcast
		process_post(&client_broadcast, ns_ready, &ns_recv);
	}
	
}

// callback for receiving files
static void files_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	// storing file received
	uint8_t file_recv = *(uint8_t *)packetbuf_dataptr();

	state = 2;
	printf("state 2");
	process_exit(&timerout_start);
	// debugging
	printf("file received from %d.%d: '%d'\n",
		from->u8[0], from->u8[1], file_recv);

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

// client_broadcast to send needed files
PROCESS_THREAD(client_broadcast, ev, data)
{
	// exit handler
	PROCESS_EXITHANDLER(broadcast_close(&cli_broadcast);)

	// begin process
	PROCESS_BEGIN();

	// open broadcast
	broadcast_open(&cli_broadcast, 128, &client_call);

	while(1) {
		// wait until received number of available files from server
		PROCESS_WAIT_EVENT_UNTIL(ev == ns_ready);

		// store Ns
		uint8_t ns = *(uint8_t *)data;


		files_expecting = 0;
		files_received = 0;

		// variable to hold number of ranges in Vc
		int num_ranges = 0;
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
		}
		
		uint8_t starts[num_ranges];
		// array to hold end indices
		uint8_t ends[num_ranges];
		
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
				ends[num_ends] = i;
				num_ends++;
			}
		}

		// array to store range sizes
		int diffs[num_ranges];

		// store range sizes
		for (i=0;i<num_ranges;i++) {
			diffs[i] = ends[i]-starts[i];
		}

		// variables for sorting range sizes
		int c;
		int d;
		int swap=0;
		int schange=0;
		int echange=0;
	
		// sort	
		for (c=0; c<num_ranges-1;c++) {
			for (d=0;d<num_ranges-1;d++) {
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

	
		printf("\n");

		
		// send ranges
		for (i=0;i<1;i++) {
			uint8_t range[] = {starts[i], ends[i]};
			files_expecting += ends[i] - starts[i];
			packetbuf_copyfrom(range, 10);
			broadcast_send(&cli_broadcast);
			// debugging
			printf("range sent\n");
		}
		// flag to signal client is ready to receive files
		flag = 2;

		//timer_start = process_alloc_event();

		process_exit(&timerout_start);
		process_start(&timerout_start, NULL);

	}
	// end process
	PROCESS_END();
}

// receive files process
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
		flag = 1;
		// store received file
		int recv_file = *(int *)data;

		files_received++;

		// counter
		int i=0;
		// if the file received was actually missing, change Vc to reflect that additional file
		for (i=0;i<sizeof(Vc)/sizeof(Vc[0]);i++) {
			if(i==recv_file && Vc[i]==0) {
				Vc[i]=1;
			}
		}

		process_exit(&timerout_start);
		process_start(&timerout_start, NULL);

		

		if (files_received == files_expecting) {
			int vcounter = 0;
			int filesRecv = 0;
			for (vcounter=0;vcounter<sizeof(Vc)/sizeof(Vc[0]);vcounter++) {
				if (Vc[vcounter] == 1) {
					filesRecv++;
				}
			}
			printf("$");
			printf("%d", filesRecv);
			printf("$");
			printf("\n");
			printf("received all files!\n");
			process_exit(&timerout_start);
		}
		
	}
	// end process
	PROCESS_END();
}

PROCESS_THREAD(timerout_start, ev, data)
{
	PROCESS_EXITHANDLER(goto exit;)

	PROCESS_BEGIN();
  
	while(1) {

		static struct etimer et;

		etimer_set(&et, CLOCK_SECOND*1.5);

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

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
		state = 0;
		flag = 1;

		break;
  	}
  	exit:
		;
  		PROCESS_END();
}
