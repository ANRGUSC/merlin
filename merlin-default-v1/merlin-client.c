
/**
	* merlin client code
	* This implementation is the merlin protocol with local calculation of the optimal request range.
  
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


// constants
double Nf = sizeof(Vc)/sizeof(Vc[0]); // number of files
double Sf = 0.10; // size of file
double lambda = 0.5;	// 0.05 - strict (not many files requested at one time)		0.02 - moderate		0.01 - free (many files requested at one time)
double SH = 0.02; // size of header
double SD = 0.08; // size of data
double Ci = 0.1; // number of bytes to represent index

int state = 0;

uint8_t files_expecting = 0;
uint8_t files_received = 0;

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
	state = 1;
	// flag for receiving Ns initially
	if (flag==1) {
		// storing Ns from packet
		uint8_t ns_recv = *(uint8_t *)packetbuf_dataptr();
		printf("*");
		// for debugging purposes
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

		printf("recv Ns: %d", ns);

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
		// debugging
		printf("\nnumber of ranges needed: %d\n", num_ranges);

		// array to hold start indices
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
		
		// ----- calculate Rb to request (based off the special case of E[S(R)] for an exponential encounter distribution, where Ft(t) = 1-exp(-lambda * t)	(equation 7 in merlin paper)

		// assigning constants
		double alpha = (ns * Sf) / Nf;
		
		double SHSD = (double)(SH/SD);
		
		double beta = 2.0 * (double)Ci * (1.0 + SHSD);
		
		double max_expected = 0;
		int max_ranges = 0;

		for (i=0;i<num_ranges+1;i++) {
			int Ur = 0;
			int j = 0;
			// calculate U(R)
			for (j=0; j<i; j++) {
				Ur += ends[j]-starts[j];
			}

			// ----- numerical method for evaluating exp (because exp/pow are unsupported in contiki)

			double lbr = lambda * beta * i;
			double lau = lambda * alpha * Ur;

			int k = 0;
			double elbr = 1;
			for (k=0;k<(int)lbr;k++) {
				elbr *= 2.71828;
			}
			elbr = 1 / elbr;
			
			double lbr_diff = lbr - (int)lbr;
			double ep_lbr = ((((lbr_diff/5 + 1) * lbr_diff/4 + 1) * lbr_diff/3 + 1) * lbr_diff/2 + 1) * lbr_diff + 1;
			ep_lbr = 1/ep_lbr;

			double elau = 1;

			for (k=0;k<(int)lau;k++) {
				elau *= 2.71828;
			}

			elau = 1 / elau;
			
			double lau_diff = lau - (int)lau;
			double ep_lau = ((((lau_diff/5 + 1) * lau_diff/4 + 1) * lau_diff/3 + 1) * lau_diff/2 + 1) * lau_diff + 1;
			ep_lau = 1/ ep_lau;

			// expected value E[S(R)]
			double expected = (elbr * ep_lbr * (1 - elau * ep_lau)) / lambda;

			// debugging
			printf("E[S(R)] for R = %d: %d\n", i, (int)expected);
			// find and store max on each interation
			if (expected > max_expected) {
				max_expected = expected;
				max_ranges = i;
			}
		}

		// debugging
		printf("maximum ranges to request : %d\n", max_ranges);

		// send ranges
		for (i=0;i<max_ranges;i++) {
			uint8_t range[] = {starts[i], ends[i]};
			files_expecting += ends[i] - starts[i];
			packetbuf_copyfrom(range, 10);
			broadcast_send(&cli_broadcast);
			// debugging
			printf("range sent\n");
		}
		// flag to signal client is ready to receive files
		flag = 2;

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
		//flag = 1;
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
			
			flag = 1;
		}

		process_exit(&timerout_start);
		process_start(&timerout_start, NULL);
		

	}
	// end process
	PROCESS_END();
}

PROCESS_THREAD(timerout_start, ev, data)
{
	PROCESS_EXITHANDLER(goto exit;)

	PROCESS_BEGIN();
  
	while(1) {

		printf("time start");

		// wait until receive warning message
		//PROCESS_WAIT_EVENT_UNTIL(ev == timer_start);

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

		printf("time end");
		
		break;
  	}
  	exit:
		;
  		PROCESS_END();
}
