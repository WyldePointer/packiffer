/*

Copyright (c) 2016-2017, Massoud Asadi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/udp.h> 
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <pcap.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <syslog.h>

// command line argument help
void displayhelp(){

	printf("packiffer -t [tcp interface] -u [udp interface] -c [number of packets to capture]\nmake sure interfaces names are typed correctly\nyou can see interfaces with 'ifconfig -a' command\n"); // help text
	exit(1); // exit program
	
}

struct pcap_pkthdr *header; // pcap.h 
const u_char *pkt_data; // net/ethernet.h

// structure for packets and interfaces
struct packet_interface {
	int arg; // number of packets that taken form command line
	char *tcp_interface; // interface for tcp packets
	char *udp_interface; // interface for udp packets 
};

// Callback function invoked by libpcap for every incoming tcp packet
void packet_handler_tcp(u_char *pdtdumper, const struct pcap_pkthdr *header, const u_char *pkt_data){
    // save the packet on the dump file
    pcap_dump(pdtdumper, header, pkt_data);
}

// Callback function invoked by libpcap for every incoming udp packet
void packet_handler_udp(u_char *pdudumper, const struct pcap_pkthdr *header, const u_char *pkt_data){
    // save the packet on the dump file
    pcap_dump(pdudumper, header, pkt_data);
}

// function for tcp thread
void *functiontcp(void *argtcp){
	
	char errbuf[PCAP_ERRBUF_SIZE]; // error size buffer provided by libpacp	
	struct packet_interface *pacint = (struct packet_interface *)argtcp; // pointer to structure and casting
	syslog(LOG_INFO, "tcp thread using pcap library"); // syslog
	pcap_t *pdt; // pcap for tcp
	pcap_dumper_t *pdtdumper; // pcap dumper for tcp
	pdt = pcap_open_live(pacint->tcp_interface, BUFSIZ, 0, -1, errbuf); // open pcap

	if (pdt == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n",
			pacint->tcp_interface, errbuf);
		exit(2);
	}

	pdtdumper = pcap_dump_open(pdt, pacint->tcp_interface); // save file as interface name
	bpf_u_int32 net = 0; // The IP of our sniffing device
	struct bpf_program fp; // the compiled filter experssion

	if(pcap_compile(pdt, &fp, "tcp", 0, net) == -1){
		fprintf(stderr, "An error occurred while compiling"
			" the pcap filter.\n");
		exit(3);
	} // compile filter

	else { 

		if(pcap_setfilter(pdt, &fp) == -1){ // set filter
			fprintf(stderr, "An error occurred while setting"
				" the pcap filter.\n");
			exit(4);
		}

		else {
			syslog(LOG_INFO, "tcp thread started capturing"); // syslog

			if(pcap_loop(pdt, pacint->arg, packet_handler_tcp, (unsigned char *)pdtdumper) == -1){
				fprintf(stderr, "An error occurred while"
					" processing the packets.\n");
				exit(5);
			} // start capture

			else {
				syslog(LOG_INFO, "tcp thread done"); // syslog		
			}
		}
	}
	return 0;	
}

// function for udp thread
void *functionudp(void *argudp){

	char errbuf[PCAP_ERRBUF_SIZE]; // error size buffer provided by libpcap
	struct packet_interface *pacint = (struct packet_interface *)argudp; // // pointer to structure and casting
	syslog(LOG_INFO, "udp thread using pcap library"); // syslog
	pcap_t *pdu; // pcap for udp
	pcap_dumper_t *pdudumper; // pcap dumper for udp
	pdu = pcap_open_live(pacint->udp_interface, BUFSIZ, 0, -1, errbuf); // open pcap

	if (pdu == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n",
			pacint->udp_interface, errbuf);
		exit(6);
	}

	pdudumper = pcap_dump_open(pdu, pacint->udp_interface); // save file as interface name
	bpf_u_int32 net = 0; // The IP of our sniffing device
	struct bpf_program fp; // the compiled filter expression

	if(pcap_compile(pdu, &fp, "udp", 0, net) == -1){
		fprintf(stderr, "An error occurred while compiling"
			" the pcap filter.\n");
		exit(7);
	} // compile filter

	else {
		if(pcap_setfilter(pdu, &fp) == -1){
			fprintf(stderr, "An error occurred while setting"
				" the pcap filter.\n");
			exit(8);
		} // set filter

		else {
			syslog(LOG_INFO, "udp thread started capturing"); // syslog
			if(pcap_loop(pdu, pacint->arg, packet_handler_udp, (unsigned char *)pdudumper) == -1){
				fprintf(stderr, "An error occurred while"
					" processing the packets.\n");
				exit(9);
			} // start capture
			else {
				syslog(LOG_INFO, "udp thread done"); // syslog
			}
		}
	}
	return 0;	
}

int main(int argc, char **argv){

	struct packet_interface pacint; // declare pacint of type packet_interface structure
	int d;
	while ((d = getopt (argc, argv, "t:u:c:")) != -1)
    	switch (d)
      	{
      	    case 't':
               pacint.tcp_interface = optarg;
               break;
      	    case 'u':
               pacint.udp_interface = optarg;
               break;
      	    case 'c':
               pacint.arg = atoi(optarg);
               break;
            case '?':
               displayhelp();
            default:
               displayhelp();
        }
	
	openlog("creating threads", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0); // open log
	pthread_t pthtcp; // tcp thread def
	pthread_t pthudp; // udp thread def
	printf("\nprocessing ... \n"); // display text during sniffing
	syslog(LOG_INFO, "starting tcp & udp threads."); // syslog
	pthread_create(&pthtcp, NULL, functiontcp, (void *)&pacint); // tcp thread creation
	pthread_create(&pthudp, NULL, functionudp, (void *)&pacint); // udp thread creation
	pthread_join(pthtcp, NULL); // wait for tcp thread to completes
	pthread_join(pthudp, NULL); // wait for udp thread to completes
	pthread_cancel(pthtcp); // kill tcp thread	
	pthread_cancel(pthudp); // kill udp thread
	printf("\ninterfaces sniffed successfully.\n"); // display text after sniffing
	syslog(LOG_INFO, "udp and tcp thread done successfully."); // syslog
	closelog(); // closing log
	return 0; // exit program
}
