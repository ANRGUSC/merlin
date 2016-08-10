/**
	* File : merlin-lookup-server.c
	* This file is used to configure one of the motes as the server that contains the relevant file ranges for the clients. 
	* This Server initially sets up a “beacon” that sends the number of files (ns) available for the client to receive.  
	* Upon receiving a callback event from the client the server mote prepares to send the requested files over to the client. 
	* This occurs until the client and server are only within the “range” which is simulated by a timer mechanism to limit their interaction.
	* The server contains three processes, server_broadcast, send_files and timerout_proc to perform the functions of the server. 
*/

#include "contiki.h"
#include "net/rime.h"
#include "random.h"
#include <string.h>
#include <stdio.h>
#include "dev/leds.h"

// flag to start/stop broadcasting number of files available
static int flag = 1;
// binary vector of server's files (50 files)
int Vs[] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1};

// event to send once client's ranges are received
static process_event_t ranges_ready;

/*-----------------------------------*/

// naming processes
PROCESS(server_broadcast, "Server Broadcast");
PROCESS(send_files, "Send Files");
PROCESS(timerout_proc, "Timerout Process");

// starting processes
AUTOSTART_PROCESSES(&server_broadcast, &send_files);

/*-----------------------------------*/

// callback for receiving client's needed file ranges
static void server_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	// store received data packet
	uint8_t *range_recv = (uint8_t *)packetbuf_dataptr();  //Get a pointer to the data in the packetbuf. 
	// for debugging purposes
	printf("ranges received from %d.%d: '%d, %d'\n",
		from->u8[0], from->u8[1], range_recv[0], range_recv[1]);


	// allocate event that ranges are received
	ranges_ready = process_alloc_event(); //Allocate a global event number. 

	// start the send files process and pass the received ranges
	process_post(&send_files, ranges_ready, range_recv);

}

// callback for possible acknowledgement
static void ranges_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
}

// naming callbacks
static const struct broadcast_callbacks server_call = {server_recv};
static const struct broadcast_callbacks files_call = {ranges_recv};

// naming broadcasts
static struct broadcast_conn serv_broadcast;
static struct broadcast_conn send_files_broadcast;

/*  server broadcast process
//  The “server_broadcast” process , periodically (every 1.5 seconds ) sends the number of available files which is represented by the binary vector Vs 
//  A flag variable is used to start/stop the broadcast of available files.
*/
PROCESS_THREAD(server_broadcast, ev, data)
{
	// create timer
	static struct etimer et;

	// exit handler
	PROCESS_EXITHANDLER(broadcast_close(&serv_broadcast);)

	// start process
	PROCESS_BEGIN();

	// open broadcast on server broadcast channel
	broadcast_open(&serv_broadcast, 128, &server_call);

	while(1) {
		// if still broadcasting number of files
		if(flag==1) {
			// set the timer to 1.5 seconds
			etimer_set(&et, CLOCK_SECOND * 1.5);

			// wait for the timer to expire
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

			leds_on(LEDS_GREEN);

			// variable for number of available files
			int ns = 0;
			// counter
			int i = 0;

			// count number of available files from binary vector
			for (i=0;i<sizeof(Vs)/sizeof(Vs[0]);i++) {
				if (Vs[i]==1) {
					ns++;
				}
			}

			printf("%d\n", ns);

			// store the number of available files in a packet
			packetbuf_copyfrom(&ns, 10);
			
			
			// send the packet
			broadcast_send(&serv_broadcast);
			// for debugging purposes
			printf("number of server files available sent\n");
			leds_off(LEDS_GREEN);
		}
	}
	// end process
	PROCESS_END();
}

// send files process
/*
   The “send_files” process waits specifically for the callback from the client after it has received the broadcast. 
   The callback function “server_recv”  is used to receive the desired range of files requested by the client and also signal the event that the client is ready to receive the requested ranges.
   The event is denoted with the “ranges_ready” variable. 
   The requested ranges are stored in the “range_recv” pointer. 
   This process then compares the received indexes with the one in the server to see the files ready to be transferred.
*/
PROCESS_THREAD(send_files, ev, data)
{
	// exit handler
	PROCESS_EXITHANDLER(broadcast_close(&send_files_broadcast);)
	// begin process
	PROCESS_BEGIN();
	// open broadcast on send files channel
	broadcast_open(&send_files_broadcast, 129, &files_call);

	while(1) {

		// wait until the ranges are received
		PROCESS_WAIT_EVENT_UNTIL(ev == ranges_ready);

		// flag to stop broadcasting number of files available
		flag = 2;

		// store the range array
		uint8_t *recv_range = (uint8_t *)data;

		// for debugging
		//printf("range received: %d, %d\n", recv_range[0], recv_range[1]);

		// counter
		uint8_t i=0;

		// iterate through the desired range
		for (i=recv_range[0];i<recv_range[1];i++) {
			// if the server has the file
			if (Vs[i]==1) {
				uint8_t file = i;
				// put the file in a packet
				packetbuf_copyfrom(&file, 10);
				// send the packet
				broadcast_send(&send_files_broadcast);
				// debugging
				printf("file %d sent\n", file);
			}
		}
		process_exit(&timerout_proc);
		process_start(&timerout_proc, NULL);
	}
	// end process
	PROCESS_END();
}
//Timerout Process
/*
  This process is used to simulate the limited interaction between the Server and Client. 
  The Timer is started and is run for one second before it generates the timerout event which blocks broadcast process.
*/
PROCESS_THREAD(timerout_proc, ev, data)
{

	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)

	PROCESS_BEGIN();
  
	while(1) {
		// wait until receive warning message
		etimer_set(&et, CLOCK_SECOND * 1);

		// wait for the timer to expire
		PROCESS_WAIT_EVENT_UNTIL(etimer_	expired(&et));

		
		flag = 1;

		break;

  	}
  	exit:
		;
  		PROCESS_END();
}
