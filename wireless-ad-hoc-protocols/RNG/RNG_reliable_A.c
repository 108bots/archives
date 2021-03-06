/*
RNG node Program
 * Author: Hemanth Srinivasan
 * Year: 2006
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <math.h>
//To prevent problems in global variable updation and access - using semaphore
#include <pthread.h>

#define SERVER_PORT 6061
#define MONITOR_SERVER_PORT 7071
#define NON_RNG_TIMEOUT 5
#define MAX_RETRANSMITS 3
#define RETRANSMIT_TIMEOUT 10


struct Node {

	char name[3]; //name of this node
	char num_Neighbor[4]; //no: of neighbors for this node
	struct nodeList *neighbor_List; //neighbor names and their distances
};

struct nodeList {

	char node_Name[3]; //neighbor name
	char distance[6]; //distance

};

struct Message {

		char message_Type; //H- Hello ,B - Broadcast, U - unused
		char source[3];
		char retransmit_source[3];
		char seq_Num[10];
		char data[256];


};
struct source_sequence {

		char name;
		int sequencenum;
		int sent;

};
struct StatMessage {

		
		char source[3];
		char broadcastCnt[3];
		char receivedCnt[10];
		char consumedPower[10];


};



/*char *ipaddr_list[] = {
	                     "10.110.92.150", //A
					     "10.110.92.151", //B
						 "10.110.92.152", //C
						 "10.110.92.153", //D
						 "10.110.92.154", //E
						 "10.110.92.155", //F
						 "10.110.92.156", //G
						 "10.110.92.157"  //H
                       };
*/

pthread_mutex_t safe_mutex;


char **ipaddr_list;



//reliability
int retransmit_count = 0; //for relay
int source_transmit_count = 0; //for originating source
volatile sig_atomic_t alarm_alive = 1, relay_alarm_alive = 1; /* control variable for alarm function  */
int source_max_rng_index = -1;

//measurement variables
int MaxNB, CNB;  //RBT variables
unsigned int seed;
int max_Delay;
int recv_count = 0;
int broadcast_count = 0;
float total_power_consumed = 0;
int neighborsBroadcastCnt = 0;
int	neighborsRecvCnt = 0;
float neighborsConsumedPwt = 0;
int orphanNodes = 0;



// Global Variables
int total_Nodes;
float f_Prob; // failure probability
int attenuation;
float max_Tx_Range;
float tx_Power; // r(u) for this node
int count = 0; //number of neighbors
int *connectionState;
int *hello_status;
struct Node *peer_node;
struct Node cur_node;
int hello_cnt = 0; //total number of hellos recevied
int con_cnt = 0; //total number of connected peers
int peer_num; //number of neighbors
int RNG_flag = 0; //will be set only after RNG computation is completed
struct source_sequence sq[10];
//int prev_seq_num = 0;
float *tempasl;
int sequence_index = 0;


//Rng computation variables
//assoicated neighbor list
float *asl;
float *sortedNDist;
char *sortedNName;

float *connect_sortedNDist;
char *connect_sortedNName;

int *send_list;


//Server variables
struct sockaddr_in serv_addr;
struct timeval timeout;
char opt_val= '1';
int listenfd, peer_fd;
int *connected_list;  /* Array of connected sockets to know who we are talking to */
int maxsock;
fd_set mon_socks;
int loop_flag = 0;
int socks_read, listnum;
struct sockaddr_in PeerAddress;
int sin_size = sizeof(struct sockaddr_in);
char peer_addr[17];
int valid_peer = 0;
struct sigaction sigs;
int recv_num;
char *recv_data;
struct Message recv_message;
struct Message retransmit_message;
struct Message broadcast_queue[5];

float rebroadcast_dist;
 
//Monitor client variables

char *mrecv_data;
struct StatMessage mrecv_message;

//Client variables
struct sockaddr_in client_addr;
int *sockfd;
int seek;
int numconnects;
struct sigaction sigc;
int numconnects;
struct Message send_message;
int maxin_socks	; 
fd_set in_socks;
int	insocks_read;
struct timeval intimeout;
float src_broadcast_dist;
int seqCnt=0;

void collectStatistics(void);
void print_Statistics(void);

//for mutex stuff
int IS_CONNECTED(int);
void SET_CONNECTED(int);


//function declarations
void parse_input(char []);
void sigchild_handler(int a) {while(wait(NULL) > 0);}
void process_data(void);
void print_message(struct Message);
void hello_packet_processing(int);
void broadcast_packet_processing(void);
void print_node(struct Node);
void sendHello(void);
void get_node_address(int);
//RNG Computation related
void rngComputation(void);
float find_v_w_dist(char ,char );
void maxDistIndexFunction(void); //sorted descending
void minDistIndexFunction(void); //sorted ascending
int commonNeighborCheck(char,char);

//Broadcast functionality //sands
void sendBroadcast();
struct Message send_message;


void *serverFunctionThread(void *arg);
void *clientFunctionThread(void *arg);
void *serverMonitorThread(void *arg);

void *retransmitThread(void *arg);
//general functionalities
void itoa(int , char []);
void find_common_neighbors(float, int []);
void rebroadcast (int, float);

// Related to non rng functionality 
void nonrng_alarm_handler(int);
void send_rebroadcast(void);
void nonrng_alarm_handler(int);
void rng_alarm_handler(int);
int calculate_probability(float);
void compute_consumed_power(float);
//reliability
void source_retransmit_handler(int);
void retransmit_broadcast(void);
void retransmit_broadcast_handler(int);

int main (int argc, char *argv[]) {

 int i, j, k, con_id;
 pid_t pid;
 char self_id[3], in_buf[257];

 pthread_t clientThread_id;
 pthread_t serverThread_id;

 
 pthread_t serverMonitorThread_id;

 void *cexit_status,*sexit_status;
 int clientvalue,servervalue;

 clientvalue = 42;
 servervalue = 54;


 if (argc != 4) {

   printf("\nUsage: rngProtocol <seed> <max_delay> <self ID> \n");
   exit(0);

 }

  strcpy(self_id, argv[3]);
  strcpy(cur_node.name, self_id);

  seed = atoi(argv[1]);
  max_Delay = atoi(argv[2]);
 
 printf("\nReading Input File...\n");
 //function call to parse the input file and load the data structures
 parse_input(self_id);


 printf("\nThe Current Node Data is:");
 printf("\nNode name is %s", cur_node.name);
 printf("\nNumber of peers is %s", cur_node.num_Neighbor);

 for (i = 0; i < count; i++ ) {
   printf("\nNeighbor %s is at a distance of %s", cur_node.neighbor_List[i].node_Name, cur_node.neighbor_List[i].distance);

 }

// get IP address of the nodes
 get_node_address(total_Nodes);

 //allocate memory for the asl

 asl = (float *)malloc(total_Nodes * sizeof(float));
 tempasl = (float *)malloc(total_Nodes * sizeof(float));

 //initialize the asl to -1 (no neighbors yet)
 for (i = 0; i < total_Nodes; i++ )
	 asl[i] = -1;

  //fill the asl
 asl[self_id[0] - 65] = 0; //self node

 for(i=0;i<total_Nodes;i++) {
        tempasl[i] = asl[i];
    
 }

 peer_num = atoi(cur_node.num_Neighbor);

 //fill distances of neighbor nodes
 for (i = 0; i < peer_num; i++ )
   asl[cur_node.neighbor_List[i].node_Name[0] - 65] = atof(cur_node.neighbor_List[i].distance);

 printf("\n\nIntial ASL for Node %s contains..", self_id);

 for (i = 0; i < total_Nodes; i++ )
  printf("\nNeigbor is %c at a distance of %f", i+65, asl[i]);

//allocate memory for current node's neighbors
   peer_node = (struct Node *) malloc (peer_num * sizeof(struct Node));


 //Mapping list for connection state for avoiding redundant connections
 connectionState = (int *)malloc(total_Nodes * sizeof(int));
 for (i = 0; i < total_Nodes; i++ )
	 connectionState[i] = 0;
// list of connected peers


 connected_list = (int *)malloc(peer_num * sizeof(int));
 for (i = 0; i < peer_num; i++ )
	 connected_list[i] = 0;

 

//set hello status to 0 for potential neighbors
 hello_status= (int *)malloc(total_Nodes * sizeof(int));
 for (i = 0; i < total_Nodes; i++ )
	 hello_status[i] = 0;

//peer sockets
 sockfd = (int *)malloc(peer_num * sizeof(int));
 for (i = 0; i < peer_num; i++ )
	 sockfd[i] = -1;

//sorted neighbor distance
 sortedNDist = (float *)malloc(peer_num * sizeof(float));
 connect_sortedNDist = (float *)malloc(peer_num * sizeof(float));

 for (i = 0; i < peer_num; i++ ) {
	 sortedNDist[i] = 0;
	 connect_sortedNDist[i] = 0;
 }

 send_list= (int *)malloc(peer_num * sizeof(int));

//initialize broadcast_queue
for (i = 0; i < 5; i++)
  broadcast_queue[i].message_Type = 'U'; //un used


//sorted neighbor distance
 sortedNName = (char *)calloc(peer_num , sizeof(char));
 connect_sortedNName = (char *)calloc(peer_num , sizeof(char));

for (i = 0; i < 10; i++ ){

	sq[i].sequencenum=0;
	sq[i].sent = 0;
}


 pthread_mutex_init(&safe_mutex,NULL);

  // Create the thread , passing &value for the argument.
 if(pthread_create(&clientThread_id,NULL,clientFunctionThread,&clientvalue)<0)
 {
	 printf("\nproblem in thread creation \n");
 }

 if(pthread_create(&serverThread_id,NULL,serverFunctionThread,&servervalue)<0)
 {
	 printf("\nproblem in thread creation \n");
 }

// The main program continues while the thread executes.


 
 if(pthread_create(&serverMonitorThread_id,NULL,serverMonitorThread,&servervalue)<0)
 {
	 printf("\nproblem in server thread creation \n");
 }


 pthread_join(clientThread_id,&cexit_status);
 pthread_join(serverThread_id,&sexit_status);

 
 pthread_join(serverMonitorThread_id,&sexit_status);
 
 return 0;

}//end main



void *serverMonitorThread(void *arg) {
	
	int i,k,con_id;
	int mlistenfd;
	struct sockaddr_in mserv_addr;
	struct timeval timeout;
	char opt_val= '1';
	int client_fd;
	int *mconnected_list;  /* Array of connected sockets to know who we are talking to */
	int maxsock;
	fd_set mon_socks;
	int listnum;
	struct sockaddr_in ClientAddress;
	char client_addr[17];
	int sin_size = sizeof(struct sockaddr_in);
    int msocks_read;
	

	
   /* creating a socket */
   if ((mlistenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
     fprintf(stderr, "\nError in creating socket");
	 perror("socket");
     exit(1);
   }

   /*Prevent the address already in use error*/
   if (setsockopt(mlistenfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int)) == 1) {
     fprintf(stderr, "\nError in setting socket options of reuse address.\n");
	 perror("setsockopt");
     exit(1);
   }

   /* configuring server address structure */
   mserv_addr.sin_family = AF_INET;
   mserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   mserv_addr.sin_port = htons(MONITOR_SERVER_PORT);

   memset(&(mserv_addr.sin_zero), '\0', 8);

   /* binding the socket to the service port */
   if (bind(mlistenfd, (struct sockaddr*) &mserv_addr, sizeof(mserv_addr)) < 0) {
     fprintf(stderr, "\nError in binding the socket to the port\n");
	 perror("bind");
     exit(1);
   }


   /* listen on the port*/
    if (listen(mlistenfd, total_Nodes) < 0) {
      fprintf(stderr, "\nError in listening on the port\n");
	  perror("listen");
      exit(1);
	}

   

	while(1) {



			     client_fd = accept(mlistenfd,(struct sockaddr *)&ClientAddress,&sin_size);
		         //extract ip address of peer
				 strcpy(client_addr, inet_ntoa(ClientAddress.sin_addr));

				 //printf("\nPeer IP address on socket %d is %s\n", client_fd, client_addr);

				 
				 if (client_fd < 0) {
				   fprintf(stderr, "\nError in temporarily accepting new client\n");
				   perror("accept");
				   exit(1);
				 }

              
           	    //printf("\nComing into receive!!!!\n");
			    mrecv_data = (char *)malloc(sizeof(struct StatMessage));
				/* recieve a message from the server and display it on the terminal*/
				recv_num = recv(client_fd, mrecv_data, sizeof(struct StatMessage), 0);
                if(recv_num == 0) {
			      
				  perror("recv");
				  continue;
				}
				else if (recv_num > 0) {

//       			 printf("\nData received from peer on socket %d\n", client_fd);
	 			 collectStatistics();
				 print_Statistics();
				}

				free(mrecv_data);
		
		
	} //end while(1)- server loop


}


void *clientFunctionThread(void *arg) {
       int j;
       char in_buf[257];
	   int cpeer_num=peer_num;
	  

	/* child is the CLIENT*/
					sigc.sa_handler = sigchild_handler;
					sigemptyset(&sigc.sa_mask);
					sigc.sa_flags = SA_RESTART;
					 if (sigaction(SIGCHLD, &sigc, NULL) == -1) {
						  fprintf(stderr, "\nError in reaping zombie processes\n");
							perror("sigaction");
							exit(1);
					 }
				//Check for global state for each connections established .
	            //If connection is not established then establish a connection
					printf("\nPausing for while..\n");
                    sleep(10);
					while(numconnects!=cpeer_num){

				      for(j=0;j < cpeer_num;j++){

						//printf("coming in \n");
						//printf("Server port %d ",SERVER_PORT);

						//printf("\nSeek is %s \n",cur_node.neighbor_List[j].node_Name);

				    	minDistIndexFunction();

				    	seek = connect_sortedNName[j]-65;

					//	printf("\nSeek is %d \n",seek);


						//if(IS_CONNECTED(seek)){
							//printf("coming in after state check\n");
							//printf("\n\nIP address of peer is : %s\n",ipaddr_list[seek]);

							client_addr.sin_family = AF_INET;
							client_addr.sin_port = htons((unsigned)SERVER_PORT);
							memset(&(client_addr.sin_zero), '\0', 8);

							if (inet_pton(AF_INET, ipaddr_list[seek], &client_addr.sin_addr) <= 0) {
							   fprintf(stderr, "\nError in configuring server address structure IP address\n");
							   perror("inet_pton");
						       continue;
							}

							if(sockfd[j] == -1) {
							  if ((sockfd[j] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
							   fprintf(stderr, "\nClient:Error in creating socket\n");
							   perror("socket");
							   continue;
							  }
							}

						//	printf("created socket %d ",sockfd[j]);

							/* connecting to the server */
				  		    if (connect(sockfd[j], (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
							  fprintf(stderr, "\nError in connecting to the  server\n");
							  perror("connect");
							 continue;
							}

							//SET_CONNECTED(seek);

							//printf("\nConnected successfully to the server.\n");
							numconnects++;
							//}

					  } //end for


					} //end while
                    
					printf("\n\nBuilding the Hello Packet...");
					sendHello();

					intimeout.tv_sec = 10;
					intimeout.tv_usec = 0;
					maxin_socks = fileno(stdin);

					while (1)
					{


						FD_ZERO(&in_socks);
						FD_SET(fileno(stdin),&in_socks);


						/*call select with 10 sec timeout interval*/


						insocks_read = select(maxin_socks+1, &in_socks, NULL, NULL, &intimeout);

						if (insocks_read < 0) {
						fprintf(stderr, "\nError in reading user input\n");
						perror("select");
						exit(1);
						}

						if (insocks_read == 0) {
							/* print "." to show client  is waiting for user input */
							//printf("C");
							fflush(stdout);
						}
						else if (FD_ISSET(fileno(stdin),&in_socks)) {

							//trigger broad cast
                            if (read(1, in_buf, 256) < 0)
							  printf("\nError in select/reading user input!!");
							printf("\nPressed keyboard" );
							//sands
							if(!(strstr(in_buf,"B")==NULL))
							{
								//Sands came here
							//	prev_seq_num = seqCnt;

                      
                                alarm_alive = 1;
								seqCnt++;
                                sendBroadcast();
								
								//reliability
								
								while ((alarm_alive) && (source_transmit_count < MAX_RETRANSMITS)) {
								
								  /* Establish a handler for SIGALRM signals. */
								  signal (SIGALRM, source_retransmit_handler);
                                  
								  printf("\nStarting retransmit timer. Try %d\n", source_transmit_count+1);
								  
								  /*set an alarm for retransmissions*/
								  alarm(RETRANSMIT_TIMEOUT);

								  sleep(RETRANSMIT_TIMEOUT+1); 
								}
								//reliability

								//broadcast_count++; //keep track of no: of broadcasts including rebroadcasts if any
							}
							else if(!(strstr(in_buf,"D")==NULL))
							{
								
                                
                                
							    printf("\nMonitorting Source is %s", cur_node.name);
							    printf("\nBroadcast count for Source  %s is %d", cur_node.name, broadcast_count);
							    printf("\nReceived count for Source  %s is %d", cur_node.name, recv_count);
							    printf("\nPower consumed by Source  %s is %f", cur_node.name, total_power_consumed);


								printf("\nThe total number of broadcasts : %d \n",neighborsBroadcastCnt+broadcast_count);
								printf("\nThe total number of nodes that did not receive the message : %d\n",orphanNodes);
								printf("\nThe total power consumed  : %f\n",total_power_consumed + neighborsConsumedPwt);
								
								//reseting couters
								recv_count = 0;
								neighborsBroadcastCnt = 0;
								broadcast_count = 0;
								orphanNodes = 0;
								total_power_consumed = 0;
								neighborsConsumedPwt = 0;
								neighborsRecvCnt = 0;
							}

							fflush(stdin);

						}

					}//end while


}

int IS_CONNECTED(seekvalue) {

	pthread_mutex_lock(&safe_mutex);
	if(connectionState[seekvalue]==0)
	{
		pthread_mutex_unlock(&safe_mutex);
		return 1;
	}
	else
	{
		pthread_mutex_unlock(&safe_mutex);
		return 0;
	}


}

void SET_CONNECTED(seekvalue) {

	pthread_mutex_lock(&safe_mutex);
	connectionState[seekvalue]=1;
	pthread_mutex_unlock(&safe_mutex);
}




void *serverFunctionThread(void *arg) {
   int i,k,con_id;
   int speer_num= peer_num;
        /* Parent is the server*/
   /*
   sigs.sa_handler = sigchild_handler;
   sigemptyset(&sigs.sa_mask);
   sigs.sa_flags = SA_RESTART;
   if (sigaction(SIGCHLD, &sigs, NULL) == -1) {
    fprintf(stderr, "\nError in reaping zombie processes\n");
	perror("sigaction");
	exit(1);
   }
   */

   /* creating a socket */
   if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
     fprintf(stderr, "\nError in creating socket");
	 perror("socket");
     exit(1);
   }

   /*Prevent the address already in use error*/
   if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int)) == 1) {
     fprintf(stderr, "\nError in setting socket options of reuse address.\n");
	 perror("setsockopt");
     exit(1);
   }

   /* configuring server address structure */
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(SERVER_PORT);

   memset(&(serv_addr.sin_zero), '\0', 8);

   /* binding the socket to the service port */
   if (bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
     fprintf(stderr, "\nError in binding the socket to the port\n");
	 perror("bind");
     exit(1);
   }


   /* listen on the port*/
    if (listen(listenfd, speer_num) < 0) {
      fprintf(stderr, "\nError in listening on the port\n");
	  perror("listen");
      exit(1);
	}

    maxsock = listenfd;

	while(1) {


	    FD_ZERO(&mon_socks);
	    FD_SET(listenfd,&mon_socks);

        for (listnum = 0; listnum < speer_num; listnum++) {
			if (connected_list[listnum] != 0) {
				FD_SET(connected_list[listnum],&mon_socks);
				if (connected_list[listnum] > maxsock)
					maxsock = connected_list[listnum];
			}
		}

        /*call select with 10 sec timeout interval*/
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		socks_read = select(maxsock+1, &mon_socks, NULL, NULL, &timeout);
       // printf("sock_read is %d\n", socks_read);

		if (socks_read < 0) {
			fprintf(stderr, "\nError in selecting connections\n");
			perror("select");
			exit(1);
		}

		if (socks_read == 0) {
			/* print "." to show server is alive */
			//printf("S");
			fflush(stdout);
		}
		else {
			   if (FD_ISSET(listenfd,&mon_socks)) { /*new connection*/

      	         peer_fd = accept(listenfd,(struct sockaddr *)&PeerAddress,&sin_size);
		         //extract ip address of peer
				 strcpy(peer_addr, inet_ntoa(PeerAddress.sin_addr));

				 //printf("\nPeer IP address on socket %d is %s\n", peer_fd, peer_addr);

				 //find out peer name based on IP address
				 for(i = 0; i < total_Nodes; i++) {
				   if(strcmp(peer_addr, ipaddr_list[i]) == 0) {
                    // printf("\nConnected peer's name is %c\n", i+65);
					 //if(IS_CONNECTED(i)){

					//	SET_CONNECTED(i);

						valid_peer = 1;
					 //}
					 //else
					 //{
					 //	 printf("\nAlready connected with peer %c\n", i+65);
					 //	 close(peer_fd);
					 //	 loop_flag=1;
					 //	 break;
					 //}
				   }
				 }
				 if(loop_flag)
					continue;

				 if(!valid_peer) {
                   printf("\nPeer not recognized!!!Closing the connection!!\n");
				   close(peer_fd);
				   continue;
				 }


				 if (peer_fd < 0) {
				   fprintf(stderr, "\nError in accepting new client\n");
				   perror("accept");
				   exit(1);
				 }

                 for (listnum = 0; listnum < speer_num; listnum++) {
					 if (connected_list[listnum] == 0) {
				        connected_list[listnum] = peer_fd;
						break;
					 }
				 }
                 //	printf("\nConnected to a peer on socket %d\n", connected_list[listnum]);


			   }
			   else {

				   for (listnum = 0; listnum < speer_num; listnum++) {
					  //	printf("\nConnected_list[%d] is %d\n", listnum, connected_list[listnum]);

					 if (connected_list[listnum] != 0) {
				        if (FD_ISSET(connected_list[listnum],&mon_socks)) {
							//check if hello packet or broadcast packet
							//Make sure all hello packets are received and then trigger processing of RNG peers.
                             //check hello_status
							//finish RNG computation wait for user input to initiate broadcast
							//start RBOP alogrithm

						    //check connectionstate and count the number of peers connected


							for( k = 0; k < total_Nodes; k++) {
							 if (con_cnt == speer_num) break;
							 con_id = cur_node.neighbor_List[k].node_Name[0]-65;
							 if(connected_list[con_id] > 0 ) {
									con_cnt++;
							  }
                            }
                           // printf("\nCon_cnt = %d\n", con_cnt);



                            //if all peers are connected
							if(con_cnt == speer_num) {
							   // printf("\nComing into receive!!!!\n");
							    recv_data = (char *)malloc(sizeof(struct Message));
								/* recieve a message from the server and display it on the terminal*/
								recv_num = recv(connected_list[listnum], recv_data, sizeof(struct Message), 0);

                                if(recv_num == 0) {
							      fprintf(stderr, "\nConnection to Peer %c was closed!!\n", listnum+65 );
								  perror("recv");
								  continue;
								}
								else if (recv_num > 0) {
								 //printf("\nData received from peer on socket %d\n", connected_list[listnum]);
                                 printf("\nData received from peer....");
							//	 printf("\nData contains:\n %s", recv_data);
								 //process the recvd data and load the structure
                                 process_data();

								}

								free(recv_data);

								if(recv_message.message_Type == 'H') {
								  hello_status[listnum] = 1;
								  hello_cnt++;
								 // printf("Function %s ,Line %d : hello_cnt is :%d\n",__FUNCTION__,__LINE__,hello_cnt);

								  //call hello processing function
								  hello_packet_processing(hello_cnt-1);

								}
								else if (recv_message.message_Type == 'B') {
								 //call broadcast processing function
									if (!RNG_flag) {
									  printf("\nRNG computation pending for the node!!!.Discarding the message.\n");
								      continue;
									}
									else {
										printf("\nCalling broadcast_packet_processing!!!\n");
										broadcast_packet_processing();

									}
								}
								else {
								  printf("\nMessage Type is Unknown!!!.Discarding the message.\n");
								  continue;
								}



							} //end all peer connect

						} //end if connected_list socket is set

					 } //end if connected list is not empty

				   } //end for - scanning the connected list for active data

			   }//end not a new connection data


		}//end data on sock_read


	} //end while(1)- server loop


  }




void parse_input (char self[]) {

  int i, c, j, k, m, n, found = 0, cur_node_peers;
  FILE *input_file;
  char c_total_nodes[4], c_fprob[6], c_attn[3], c_maxtx_rg[6], c_num_peers[4];
  char node_id[8] = "node ";
  char temp[256];
  

  strcat(node_id, self);

  //printf("\nthe node_id is %s\n",node_id);

  if ((input_file = fopen("input.txt", "r")) == NULL) {
    printf("\nError in opening input file\n");
	exit(0);
  }


  while(1) {

    while ((c = getc(input_file)) != '\n'); //goto end of first line

	j = 0;
	while ((c = getc(input_file)) != '\n') {  //read the total number of nodes
     c_total_nodes [j++] = c;
	}
    c_total_nodes [j] = '\0';

   total_Nodes = atoi(c_total_nodes);

   while ((c = getc(input_file)) != '\n'); //goto end of third line

	j = 0;
	while ((c = getc(input_file)) != '\n') {  //read the failure probability
     c_fprob [j++] = c;
	 // printf("%c ",c);
	}
    c_fprob [j] = '\0';

	//printf("\nc_fprob is %s\n", c_fprob );
	f_Prob = atof(c_fprob);
    //printf("\nfprob is %f\n", f_Prob );


    while ((c = getc(input_file)) != '\n'); //goto end of 5th line

	j = 0;
	while ((c = getc(input_file)) != '\n') {  //read the attenuation
     c_attn [j++] = c;
	 // printf("%c ",c);
	}
    c_attn [j] = '\0';

	//printf("\nc_attn is %s\n", c_attn);
	attenuation = atoi(c_attn);
	//printf("\nattenuation is %d\n", attenuation);

    while ((c = getc(input_file)) != '\n'); //goto end of 7th line

	j = 0;
	while ((c = getc(input_file)) != '\n') {  //read the attenuation
     c_maxtx_rg [j++] = c;
	 //printf("%c ",c);
	}
    c_maxtx_rg [j] = '\0';

	//printf("\nc_maxtx_rg is %s\n", c_maxtx_rg);
	max_Tx_Range = atof(c_maxtx_rg);
    //printf("\nmax tx range is %f\n", max_Tx_Range);


    while ((c = getc(input_file)) != '\n'); //goto end of 9th line
    while ((c = getc(input_file)) != '\n'); //goto end of 10th line

	rewind(input_file);

	while (!feof(input_file)) {
        //printf("\ntemp contains %s", temp);

		if ((fgets(temp, 255, input_file)) == NULL) {
          printf("\nEnd of Input file Reached!!!\n");
		  found = 0;
		}


      if(strncmp(temp, node_id, 6) == 0) {
		found = 1;
        k = 0;
 		while ( k < strlen(temp)) {
          while (temp[k++] != ' ');
		  k++;
		  while (temp[k++] != ' ');
		  m =0;
		  while(temp[k] != '\0')
             c_num_peers[m++] = temp[k++];
          c_num_peers[m-1] = '\0';

		}

	   strcpy(cur_node.num_Neighbor, c_num_peers);

	   //printf("\nc_num_peers is %s\n", c_num_peers);
	   cur_node_peers = atoi(c_num_peers);
	   //printf("\ncur_node_peers is %d\n", cur_node_peers);

	   cur_node.neighbor_List = (struct nodeList *)malloc(cur_node_peers * sizeof(struct nodeList));

	   while (count < cur_node_peers) {
		    i = 0;
          	while ((c = getc(input_file)) != ' ') {  //read the neighbor name
               cur_node.neighbor_List[count].node_Name[i++] = c;
			}
            cur_node.neighbor_List[count].node_Name[i] = '\0';
            //printf("\npeer name is %s", cur_node.neighbor_List[count].node_Name);

			i = 0;
            while ((c = getc(input_file)) != '\n') {  //read the neighbor distance
               cur_node.neighbor_List[count].distance[i++] = c;
			}
			cur_node.neighbor_List[count].distance[i] = '\0';
            //printf("\npeer distance is %s", cur_node.neighbor_List[count].distance);

			count++;

	   }


	  }

	  if(found) break;

	}

	if(found) break;

	if(!found) {
     printf("Not a Valid Node ID!!! Exiting the Program!!!\n");
	 exit(0);

	}


  } //end while(1)

  fclose(input_file);
  

}

void collectStatistics(){

	int i=0,j, len,otemp;
	len = strlen(mrecv_data);
		//char source[3];
		//char broadcastCnt[3];
		//char receivedCnt[10];
		//char consumedPower[10];
	while(i < len){
		
	 //i+=2;
	 j =0;
	 while (mrecv_data[i] != '$')
	   mrecv_message.source[j++] = mrecv_data[i++];
     mrecv_message.source[j] = '\0';
     i++;
     
	 j =0;
	 while (mrecv_data[i] != '$')
	   mrecv_message.broadcastCnt[j++] = mrecv_data[i++];
     mrecv_message.broadcastCnt[j] = '\0';
     i++;

     j =0;
	 while (mrecv_data[i] != '$')
	   mrecv_message.receivedCnt[j++] = mrecv_data[i++];
     mrecv_message.receivedCnt[j] = '\0';
     i++;

     j =0;
	 while (mrecv_data[i] != '$')
	   mrecv_message.consumedPower[j++] = mrecv_data[i++];
     mrecv_message.consumedPower[j] = '\0';

	 break;
	}

   
	neighborsBroadcastCnt += atoi(mrecv_message.broadcastCnt);
	otemp=atoi(mrecv_message.receivedCnt);

	if( otemp == 0 )
	  orphanNodes++;

	neighborsRecvCnt += atoi(mrecv_message.receivedCnt);
    neighborsConsumedPwt += atoi(mrecv_message.consumedPower);
   
	


}

void print_Statistics() {

  printf("\nSource is %s", mrecv_message.source);
  printf("\nBroadcast count for Source  %s is %s", mrecv_message.source, mrecv_message.broadcastCnt);
  printf("\nReceived count for Source  %s is %s", mrecv_message.source, mrecv_message.receivedCnt);
  printf("\nPower consumed by Source  %s is %s", mrecv_message.source, mrecv_message.consumedPower);

}

void process_data() {

   int i = 0, j, len;
   len = strlen(recv_data);

   while (i < len) {

	 recv_message.message_Type = recv_data[i];
	 i+=2;
	 j =0;
	 while (recv_data[i] != '$')
	   recv_message.source[j++] = recv_data[i++];
     recv_message.source[j] = '\0';
     i++;
     
	 j =0;
	 while (recv_data[i] != '$')
	   recv_message.retransmit_source[j++] = recv_data[i++];
     recv_message.retransmit_source[j] = '\0';
     i++;

     j =0;
	 while (recv_data[i] != '$')
	   recv_message.seq_Num[j++] = recv_data[i++];
     recv_message.seq_Num[j] = '\0';
     i++;

     j =0;
	 while (recv_data[i] != '$')
	   recv_message.data[j++] = recv_data[i++];
     recv_message.data[j] = '\0';

	 break;


   }

 // printf("\nRecieved message details:");
 // print_message(recv_message);


  //if msg type is broadcast, copy recvd message to brdcst msg q

  if(recv_message.message_Type == 'B') {
    
	  recv_count++;//keep track total number of broadcast's received


	for(i = 0; i < 5; i++) {
	  if(broadcast_queue[i].message_Type == 'U') {
   
         broadcast_queue[i].message_Type = recv_message.message_Type;
		 strcpy(broadcast_queue[i].source,recv_message.source);
         strcpy(broadcast_queue[i].retransmit_source,recv_message.retransmit_source);
         strcpy(broadcast_queue[i].seq_Num,recv_message.seq_Num);
		 strcpy(broadcast_queue[i].data,recv_message.data);
		 break;
	  }
	}

	/*for(i = 0; i < 5; i++) {
      if(broadcast_queue[i].message_Type != 'U') {  
        printf("\nQ msg type is %c ", broadcast_queue[i].message_Type);
		 printf("\nQ msg source is %s ", broadcast_queue[i].source);
         printf("\nQ msg retrans source is %s ", broadcast_queue[i].retransmit_source);
         printf("\nQ msg seq num is %s ", broadcast_queue[i].seq_Num);
		 printf("\nQ msg data is %s ", broadcast_queue[i].data);


	  }
	}*/

  }




}


void print_message(struct Message msg) {

  if (msg.message_Type == 'H')
    printf("\nMessage Type is Hello");
  else if (msg.message_Type == 'B')
    printf("\nMessage Type is Broadcast");
  else
	printf("\nMessage Type is Unknown");

  printf("\nMessage Source is %s", msg.source);
  printf("\nMessage Retranmsmitting Source is %s", msg.retransmit_source);
  printf("\nMessage Sequence number string is %s integer is %d", msg.seq_Num, atoi(msg.seq_Num));
  printf("\nMessage Data is:\n %s", msg.data);

}


void hello_packet_processing(int hello_num) {

  int id, i, j, k, length, n,aslindex;


  //load the neighbor node structures

   strcpy(peer_node[hello_num].name, recv_message.source);

   printf("\nReceived Hello from Neighbor %c.", peer_node[hello_num].name[0]);

   length = strlen(recv_message.data);

 //  printf("\n Hello message length is %d and it contains:\n", length);
  // printf("%s", recv_message.data);

   i = 0;
   while (i < length) {

	 j =0;
	 while (recv_message.data[i] != '#')
	 {
	 //  printf("Function %s , Line %d : hello_num is %d \n ",__FUNCTION__,__LINE__,hello_num);
	   //printf("Function %s , Line %d : recv_message.data is %d \n",__FUNCTION__,__LINE__,recv_message.data[i++]);
	   peer_node[hello_num].num_Neighbor[j++] = recv_message.data[i++];
	 }
     peer_node[hello_num].num_Neighbor[j] = '\0';
     i++;

     n = atoi(peer_node[hello_num].num_Neighbor);
    // printf("\n n is %d", n);

	 peer_node[hello_num].neighbor_List = (struct nodeList *)malloc(n * sizeof(struct nodeList));

     k = 0;
     while(k < n) {
      j =0;
	  while (recv_message.data[i] != '-')
	   peer_node[hello_num].neighbor_List[k].node_Name[j++] = recv_message.data[i++];
      peer_node[hello_num].neighbor_List[k].node_Name[j] = '\0';

      j = 0;
	  i++;
	  while (recv_message.data[i] != '#')
	   peer_node[hello_num].neighbor_List[k].distance[j++] = recv_message.data[i++];
      peer_node[hello_num].neighbor_List[k].distance[j] = '\0';
	  i++;

//	  printf("\n Peer %d name is %s distance is %s", k, peer_node[hello_num].neighbor_List[k].node_Name, peer_node[hello_num].neighbor_List[k].distance);
      k++;
	 }

	 break;

   }

  // printf("\nHello Message details:");
  // print_node(peer_node[hello_num]);


   //printf("%s : %d : hello_cnt is %d \n",__FUNCTION__,__LINE__,hello_cnt);
   //printf("%s : %d : peer_num is %d \n",__FUNCTION__,__LINE__,peer_num);


   if(hello_cnt == peer_num) {

     /*printf("\n\nHello status for node %s is:", cur_node.name);
     for(i = 0; i < total_Nodes; i++) {
	   if(hello_status[i] == 1)
         printf("\nHello Received from neighbor %c", i+65);

	 }*/

    /* for(i=0;i<peer_num;i++) {
		printf("Function: %s  Line: %d  Peer name is :%s\n",__FUNCTION__,__LINE__, peer_node[i].name);
		printf("Function: %s  Line: %d  No of Neighbors is :%s\n",__FUNCTION__,__LINE__, peer_node[i].num_Neighbor);
     }*/




	 //call RNG algorithm function
     printf("\n\nCalling RNG computation Algorithm....");


	 rngComputation();
     
	 printf("\nAssociated Neighborhood Graph after RNG, for Node %s is:", cur_node.name);

	 /*for (aslindex = 0; aslindex < total_Nodes; aslindex++ ){

	   printf(" ASL: asl[%d] is %f\n", aslindex, asl[aslindex]);
	 }*/

	 for (i = 0; i < total_Nodes; i++ ) {
      if(asl[i] > 0) 
       printf("\nNeigbor is %c at a distance of %f", i+65, asl[i]);
	 }

     printf("\n*****************************************\n");
     printf("*Service open for Broadcast Transmission*\n");
	 printf("*   Enter B to start broadcast    *\n");
	 printf("*   Enter D to display statistics    *\n");
	 printf("*****************************************\n");

   }


}

//Sands came here
void broadcast_packet_processing() {

//use probability to receive the packet

//use seed for RBT before performing the relay

//relay the message using RBOP algorithm

 //strcpy(peer_node[hello_num].name, recv_message.source);

 //printf("\nReceived Hello from Neighbor %c!!", peer_node[hello_num].name[0]);
    int i,j,acceptPacket;
    int IsNotEmpty=0;
    int length;
    int rngSource;
    int alreadyReceived=0;
	int tempseq;
    int *neighbor_Elm_List;
    int m, n;

    float recvd_neighbor_dist = 0;
	
    char source;	
	
    float temp_max_dist = 0;
    float maxdistance, temp;
    int recv_flag = 0, temp_seq = 0, max_old_seq = 0;

    neighbor_Elm_List = (int *)malloc(total_Nodes * sizeof(int));	
    
    for(i = 0 ;i < total_Nodes;i++){
	neighbor_Elm_List[i]=0;
    }

    for(i = 0 ;i < peer_num;i++){
	send_list[i]=0;
    }
     
	//printf("\nCalling calculate_probability!!!\n");
    acceptPacket = calculate_probability(f_Prob);

   if (!acceptPacket){
      printf("\n Probabilistically Dropping the Packet!!!!\n");

          for(j = 0; j < 5; j++) { //scan through broadcast msg queue mark that entry unused
		    tempseq=atoi(broadcast_queue[j].seq_Num);
	        if((broadcast_queue[j].source[0] == recv_message.source[0]) && ( tempseq == atoi(recv_message.seq_Num))) {
		       broadcast_queue[j].message_Type = 'U';
			}

		  } //end for j
	 
      recv_count--; 
      return;
    }
     

   //check if you are receiving you own packet. if so, discard
   if (recv_message.source[0] == cur_node.name[0]) {
    printf("\nReceiving Own Packet!!!Discarding it!!!");
    for (i = 0; i < 5; i++) {
		if(broadcast_queue[i].source[0] == recv_message.source[0]) {
         broadcast_queue[i].message_Type = 'U';
		}

	}
if((recv_message.retransmit_source[0] - 65) == source_max_rng_index) {
        printf("\nReceived original packet from RNG neighbor!!!Canceling retransmission timer!!!");  
        alarm_alive = 0; //flag that will cancel the retransmit alarm
	}

	return;
   }

  

   //re-initialize tempasl to asl if new source or if new packet from old source

   for (i = 0; i < 10; i++) {
     if(sq[i].name == recv_message.source[0])  
        recv_flag = 1;
   }

   if(!recv_flag) {
      for(i=0;i<total_Nodes;i++) {
        tempasl[i]=asl[i];
    }
   }
   else if (recv_flag) {
     for (i = 0; i < 10; i++) {
		if(recv_message.source[0] == sq[i].name) {
          if(max_old_seq > sq[i].sequencenum)
			   max_old_seq = sq[i].sequencenum;
		}
	 }
     temp_seq = atoi(recv_message.seq_Num);
	 if(temp_seq > max_old_seq) {
	   for(i=0;i<total_Nodes;i++) {
        tempasl[i]=asl[i];
	   }
	 }
 
   }

  
   /*if(recv_message.source[0] == recv_message.retransmit_source[0]) 
    source = recv_message.source[0];
   else*/
    source = recv_message.retransmit_source[0];


   
   maxdistance=atof(recv_message.data);
   
  // printf("\nCalling find_common_neighbors!!!\n");

   find_common_neighbors(maxdistance, neighbor_Elm_List);   

    for (i = 0; i < total_Nodes; i++) {  //eliminate common neighbors from rng
      if(neighbor_Elm_List[i] > 0) {
       if(tempasl[i] > 0)
	    tempasl[i] = -1;	
      	}
    }
    
	printf("\nReceived broadcast packet from %c", source);
    tempasl[source-65] = -1; //eliminating source
    
    //printf("\nEliminated source is %c  and tempasl index is %i value is %f\n",source, source-65, tempasl[source-65] );
    printf("\nEliminated source is %c.\n",source);

    for (i = 0; i < total_Nodes; i++) { //calcuate rebroadcast_dist
       if(tempasl[i] > temp_max_dist) {
		   temp_max_dist = tempasl[i];
       }
    }

    rebroadcast_dist = temp_max_dist;
 //////////////////reliablity
    //use max of rng distance and received dist to do the rebroadcast
	recvd_neighbor_dist = asl[source-65];
	printf("\nDistance to the retransmitting source is: %f\n", recvd_neighbor_dist);
    if(rebroadcast_dist < recvd_neighbor_dist)
        rebroadcast_dist = recvd_neighbor_dist;
	//reliablity

    for(i = 0 ;i < peer_num;i++) { //calculate the actual neighbors(both rng and non-rng), who are going to receive the broadcast
       //temp = atof(cur_node.neighbor_List[i].distance);
		temp = connect_sortedNDist[i];
       if((temp <= rebroadcast_dist) && (rebroadcast_dist != 0))
	     send_list[i] = 1;
    }
 

	/*for (i = 0; i < total_Nodes; i++) { 
       printf("Temp asl[%d] is %f \n",i,tempasl[i]);
    }

    for(i = 0 ;i < peer_num;i++) { 
	  printf("Send List is %d\n",send_list[i]);
    }*/

	printf("\nDistance to be used for rebroadcast is %f\n",rebroadcast_dist);
	
	/*for (i = 0; i < 10; i++ ){

	printf("sq[%d].sequencenum is %d \n",i,sq[i].sequencenum);
	printf("sq[%d].sent is %d \n",i,sq[i].sent);
	}*/


    for(i=0;i<10;i++) {  //find if new broadcast from the source or if old packet
    
	   //if ((sq[i].name==source) && (sq[i].sequencenum == atoi(recv_message.seq_Num))){
       if ((sq[i].name==recv_message.source[0]) && (sq[i].sequencenum == atoi(recv_message.seq_Num))) {
           printf("\nRecevied an already received packet!!!!");
           alreadyReceived = 1; 

		   for(m = 4; m > 0; m--) {
             if(broadcast_queue[m].message_Type == 'U') continue;
             for(n = 0; n < 5; n++) {
               if(broadcast_queue[n].message_Type == 'U') continue;
			   tempseq = atoi(broadcast_queue[n].seq_Num);
			   if((broadcast_queue[n].source[0] == broadcast_queue[m].source[0]) && (tempseq == atoi(broadcast_queue[m].seq_Num))) {
                  broadcast_queue[m].message_Type = 'U';
			   }

			 }
		   }

       }
    
    }

    
    if (asl[source-65] > 0) { //rng neighbor
    	rngSource = 1;
    	
    }
    else if (asl[source-65] == -1) { //not rng neighbor
    	 rngSource = 0;
    }
    else if (asl[source-65] == 0) 
	  printf("\nThis cannot happen. This is me!!");
	
	
    if ((acceptPacket) && (!alreadyReceived)) {  //new broadcast msg
 
        for(i=0;i<10;i++) {    //store source and seq num for new msg 
             if(sq[i].sequencenum==0){
     	       sq[i].name=recv_message.source[0];
               sq[i].sequencenum=atoi(recv_message.seq_Num);
			   sequence_index = i;
		       break;
	      }
        }
            			

	    if(rngSource) {
		   rebroadcast (1,  temp_max_dist); //type = 1, RNG

		} else if(!rngSource) { 
       
           rebroadcast(0, temp_max_dist); //type = 0, non-RNG
		}
		
    }
    else if ((acceptPacket) && (alreadyReceived)) { //old broadcast msg

      for(i=0;i<10;i++) {    
            if ((sq[i].name==recv_message.source[0]) && (sq[i].sequencenum == atoi(recv_message.seq_Num))&&(sq[i].sent==1)){
     	    printf("\nReceived already sent packet!!!Discarding it!!");  
  
			
			//if(asl[recv_message.retransmit_source[0] - 65] > 0) { //if u hear your retransmit msg from an rng neighbor, cancel timer 
                printf("\nReceived retransmitted packet from RNG neighbor %s!!!Canceling retransmission timer!!!", recv_message.retransmit_source);  
                relay_alarm_alive = 0;
			//}

			//need to mark Q unused
			for(j = 0; j < 5; j++) { //scan through broadcast msg queue mark that entry unused
	         tempseq=atoi(broadcast_queue[j].seq_Num);
             if((broadcast_queue[j].source[0] == sq[i].name) && ( tempseq == sq[i].sequencenum)) {
				// printf("\nResetting broadcast queue for already received message\n");
				 //printf("\nRebroadcast source is %c \n ",broadcast_queue[j].retransmit_source[0]);
				 broadcast_queue[j].message_Type = 'U';
			 }

			} //end for j

		    return;
		   }
	  } //end for i

	  for(i=0;i<total_Nodes;i++){
	    if(tempasl[i]>0)
		IsNotEmpty=1;
		break;
	  }
        
	  if(!IsNotEmpty) {
         printf("\nDiscarding packet as neighbor list is empty!!!!");
		 for(i=0;i<10;i++) {    
          for(j = 0; j < 5; j++) { //scan through broadcast msg queue mark that entry unused
		    tempseq=atoi(broadcast_queue[j].seq_Num);
	        if((broadcast_queue[j].source[0] == sq[i].name) && ( tempseq == sq[i].sequencenum)) {
		       broadcast_queue[j].message_Type = 'U';
			}

		  } //end for j
	 
		 } //end for i
	     return;
	  }

      if(rngSource) {
		rebroadcast (1,  temp_max_dist); //type = 1, RNG

	  } else if(!rngSource) { 
       
               rebroadcast(0,  temp_max_dist); //type = 0, non-RNG
	  }
	
   }

  
  
}

int calculate_probability(float probability) {
    float myrand;
	myrand=(double)rand()/RAND_MAX;
	if(myrand > probability){
		return 1;
	}
	else
		return 0;
}
//Sands came here


void print_node(struct Node N) {

  int m, i;

  m = atoi(N.num_Neighbor);
 // N.neighbor_List = (struct nodeList *)malloc(m * sizeof(struct nodeList));

  printf("\nNode name is %c", N.name[0]);
  printf("\nNode %c has %d neighbors", N.name[0], m);
  printf("\nNode %c 's neighbor details are:", N.name[0]);
  for(i = 0; i < m; i++)
    printf("\nNeighbor %c is at a distance of %s", N.neighbor_List[i].node_Name[0], N.neighbor_List[i].distance);
  printf("\n");

}



void sendHello() {

 int i = 0, j, k ;
 char helloMsg[300] = {'\0'};

 //Build hello packet
 helloMsg[i++] = 'H';
 helloMsg[i++] = '$';

 helloMsg[i++] = cur_node.name[0];

 helloMsg[i++] = '$';
 helloMsg[i++] = '0';
 helloMsg[i++] = '$';
 helloMsg[i++] = '0';
 helloMsg[i++] = '$';

 j = 0;
 while (j < strlen(cur_node.num_Neighbor) - 1)
   helloMsg[i++] = cur_node.num_Neighbor[j++];

 helloMsg[i++] = '#';

 for (k = 0;k < peer_num; k++) {

   helloMsg[i++] = cur_node.neighbor_List[k].node_Name[0];

   helloMsg[i++] = '-';

   j = 0;

   while (j < strlen(cur_node.neighbor_List[k].distance) - 1)
     helloMsg[i++] = cur_node.neighbor_List[k].distance[j++];

   helloMsg[i++] = '#';


 }

 helloMsg[i++] = '$';
 helloMsg[i] = '\0';

 //printf("\n\nHelloMsg Structure is \n%s size of %d", helloMsg, strlen(helloMsg));

 printf("\nCurrent node data is:");
 print_node(cur_node);




 for(j=0;j<peer_num;j++) {
   if(sockfd[j] != 0) {
	if (send(sockfd[j], (const void*) helloMsg, strlen(helloMsg), 0) < 0) {
	  fprintf(stderr, "\nError in service request\n");
      perror("send");

	}

	//printf("\nSent hello to Peer : %c on Socket : %d \n",connect_sortedNName[j],sockfd[j]);
	printf("\nSent hello to Peer : %c ",connect_sortedNName[j]);
   }


 }



}


//sands came here
void sendBroadcast() {

 int i = 0, j, k ;
 char broadcastMsg[300] = {'\0'};
 float maxAslDist = 0, temp;
 char seq;
 char tempseq[10];
 char broadcast_data[10];
 int temp_seqCnt = seqCnt;
 //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);
 itoa(temp_seqCnt,tempseq);
 //printf("seqCnter value is %d \n",temp_seqCnt);
 //printf("tempseq value is %s \n",tempseq);

 //Build broadcast packet
 broadcastMsg[i++] = 'B';
 broadcastMsg[i++] = '$';

 broadcastMsg[i++] = cur_node.name[0];

 broadcastMsg[i++] = '$';
 broadcastMsg[i++] = cur_node.name[0]; //retransmit source
 broadcastMsg[i++] = '$';
 strcat(broadcastMsg , tempseq);
 strcat(broadcastMsg , "$");
 
 

 
 //asl has rng nodes index and distance
 //getting max distance from the asl
 for (i = 0; i < total_Nodes; i++ ){
   if(asl[i] > maxAslDist){
  
	maxAslDist=asl[i];
    source_max_rng_index = i;
   }
	
  }


  
  gcvt (maxAslDist,5,broadcast_data);
 
  strcat(broadcastMsg, broadcast_data);
  strcat(broadcastMsg,"$");
  strcat(broadcastMsg,"\0");
 
  printf("\nBroadcast Message Structure is: \n%s", broadcastMsg);
  //printf("FUNCTION %s : maxAslDist is %f ",__FUNCTION__,maxAslDist);
 



 //peer num iteration gives the information for FD

 for(j=0;j<peer_num;j++) {
  //temp = atof(cur_node.neighbor_List[j].distance);
	 temp = connect_sortedNDist[j];
  if( temp <= maxAslDist){
   if(sockfd[j] != 0) {
	if (send(sockfd[j], (const void*) broadcastMsg, strlen(broadcastMsg), 0) < 0) {
	  fprintf(stderr, "\nError in broadcast service request\n");
      perror("send broadcast");

	}
	printf("\nSent Broadcast  to Peer : %c on Socket : %d \n",connect_sortedNName[j],sockfd[j]);
   }//if sockfd
  }//if maxDist check
  }//end for
  recv_count++;
  broadcast_count++; //increment broadcast count

  compute_consumed_power(maxAslDist); //calculate consumed power

  //printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
}//end sendBroadcast



void rngComputation() {

    int i,j;
	float uvDist, uwDist, vwDist;
	char vName;


	maxDistIndexFunction();


	for(i=0;i<peer_num;i++) { //v

      uvDist=sortedNDist[i];
	  vName=sortedNName[i];

	  //printf("\nvName is %c, uvDist is %f", vName, uvDist);

	  for(j=0;j<peer_num;j++) { //w

	    if(cur_node.neighbor_List[j].node_Name[0] == vName)
	      continue;

		//printf("\n w is %c", cur_node.neighbor_List[j].node_Name[0]);

	    if(commonNeighborCheck(vName, cur_node.neighbor_List[j].node_Name[0]) == 1) {
		   uwDist = atof(cur_node.neighbor_List[j].distance);
           vwDist = find_v_w_dist(vName,cur_node.neighbor_List[j].node_Name[0]);

	       if(vwDist == 0)
	         printf("\nError in gettting vw distance");

		   //printf("\nuwDist is %f uvDist is %f vwDist is %f uvDist is %f", uwDist,uvDist,vwDist,uvDist);

           if((uwDist<=uvDist)&&(vwDist<=uvDist)){

			 //Eliminating the node V if there exist w based on the above condition
		      //printf("\nFunction %s - Line %d : Eliminating vName %c \n",__FUNCTION__,__LINE__,vName);
              printf("\nEliminating Node %c from ASL",vName);
			  asl[vName-65] = -1;

		   }


		}


	  } //end j
	RNG_flag=1;

	} //end i

	printf("\n\nRNG computation completed!!!\n");
}


float find_v_w_dist(char vnm,char w) {

  int i, j;
  int numofNeigh;
  //printf("FUNCTION: %s vnm passed is %c, w passed is  %c \n",__FUNCTION__,vnm,w);


  for (i = 0; i < peer_num; i++) {
    if(peer_node[i].name[0] != vnm)
		continue;
	numofNeigh  = atoi(peer_node[i].num_Neighbor);
    for (j = 0; j < numofNeigh; j++) {
      if(peer_node[i].neighbor_List[j].node_Name[0] == w)
	  {
		 // printf("FUNCTION: %s vw PERRNODE NAME is %c \n",__FUNCTION__,peer_node[i].neighbor_List[j].node_Name[0]);
		 // printf("FUNCTION: %s vw distance is %f \n",__FUNCTION__,atof(peer_node[i].neighbor_List[j].distance));
		  return atof(peer_node[i].neighbor_List[j].distance);
	  }
	}
  }

  //printf("FUNCTION: %s vw PERRNODE NAME is %c \n",__FUNCTION__,peer_node[i].neighbor_List[j].node_Name[0]);

  return 0;
}


void maxDistIndexFunction() {

  int i, j = 0, k = 0;
  float max = 0, temp;

  struct nodeList temp_node[peer_num];


  for (i = 0; i < peer_num; i++) {
   strcpy(temp_node[i].distance, cur_node.neighbor_List[i].distance);
   strcpy(temp_node[i].node_Name, cur_node.neighbor_List[i].node_Name);
  }

  //sort the temp_node of nieghbors and fill the sortedNDist and sortedNName
  while (j < peer_num) {
   for (i = 0; i < peer_num; i++) {
	temp = atof(temp_node[i].distance);
	if(temp > max) {
      max = temp;
	  k = i;
	}

   }
   sortedNDist[j] = max;
   sortedNName[j] = cur_node.neighbor_List[k].node_Name[0];
   strcpy (temp_node[k].distance, "0.0");
  // printf("\nSortedNDist %f, SortedNName is %c k is %d", sortedNDist[j], sortedNName[j], k);
   max  = 0;
   j++;

  }

}

void minDistIndexFunction() {

  int i, j = 0, k = 0;
  float min = 9999, temp;

  struct nodeList temp_node[peer_num];


  for (i = 0; i < peer_num; i++) {
   strcpy(temp_node[i].distance, cur_node.neighbor_List[i].distance);
   strcpy(temp_node[i].node_Name, cur_node.neighbor_List[i].node_Name);
  }

  //sort the temp_node of nieghbors and fill the sortedNDist and sortedNName
  while (j < peer_num) {
   for (i = 0; i < peer_num; i++) {
	temp = atof(temp_node[i].distance);
	if(temp < min) {
      min = temp;
	  k = i;
	}

   }
   connect_sortedNDist[j] = min;
   connect_sortedNName[j] = cur_node.neighbor_List[k].node_Name[0];
   strcpy (temp_node[k].distance, "9999");
   //printf("\nConnect_sortedNDist %f, Connect_sortedNName is %c k is %d", connect_sortedNDist[j], connect_sortedNName[j], k);
   min  = 9999;
   j++;

  }

}

int commonNeighborCheck(char vTempName, char wTempName) {

  int i,j, k;
  int numNeighbor;

  for(i = 0; i < peer_num;i++) {

	if(peer_node[i].name[0] == vTempName) {

     numNeighbor= atoi(peer_node[i].num_Neighbor);

     for (j = 0; j < numNeighbor ; j++) {
      if(peer_node[i].neighbor_List[j].node_Name[0] == wTempName)
	    return 1;
	 }

	}


  }



  return 0;

}



void get_node_address(int n) {

  FILE *fp1;
  char temp[20];
  int i;

  //n = 25;

  ipaddr_list = (char **)calloc(n, sizeof(char *));
  for(i = 0; i < n; i++)
    ipaddr_list[i] = (char *)calloc(16, sizeof(char));


  if (n > 25) {
	printf("\n Only 25 IP addresses defined!!");
	exit(0);
  }
  if ((fp1 = fopen("ipaddrs.txt", "r")) == NULL) {
    printf("\nError in opening ip address file\n");
	exit(0);
  }

  i = 0;

 // printf("\nno: of nodes = %d", n);

  while (i < n) {

       if ((fgets(temp, 16, fp1)) == NULL) {
          printf("\nError reading IP addresses!!!\n");
	   }

       strncpy(ipaddr_list[i], temp, 13);
	  // printf("\nIP address is %s", ipaddr_list[i]);
	   i++;
  }


  fclose(fp1);

}

void itoa(int n, char str[]) { 
  

  int i, j, sign; 
  char *temp; 

  //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);
  if ((sign = n) < 0)  
     n = -n; 
  i = 0; 
  do {  // generate string digits in reverse order 
   str[i++] = n % 10 + '0'; 
 
  }while((n /= 10) > 0); 
  if (sign < 0) 
    str[i++] = '-'; 
  str[i] = '\0'; 
 
  //reverse the string 
 
  temp = (char *)malloc (strlen(str) * sizeof(char)); 
 
  for (j = strlen(str) - 1, i = 0;j >= 0; j--, i++)  
	  temp[i] = str[j]; 
  temp[i] = '\0'; 
  strcpy(str,temp); 
   
  free(temp); 
  //printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
   
} 


void find_common_neighbors(float dist, int n_list[]) {

  int i, j, ind, ngh_num;

  //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);
   for (i = 0; i < peer_num; i++)  { //extract peer node number

	//if(recv_message.source[0] == peer_node[i].name[0]) {
    if(recv_message.retransmit_source[0] == peer_node[i].name[0]) {
	   ind = i;
	   break;
	}
   }

  ngh_num =  atoi(peer_node[ind].num_Neighbor);
  
  for (i = 0; i < peer_num; i++) { //extract common physical neighbors
   for (j = 0; j < ngh_num; j++)	{
       if(cur_node.neighbor_List[i].node_Name[0] ==  peer_node[ind].neighbor_List[j].node_Name[0] ) {
             n_list[cur_node.neighbor_List[i].node_Name[0] - 65] = 1;
      }

   }

   for (i = 0; i < total_Nodes; i++) {
     if (n_list[i] == 1) {
	 //if(asl[i] <= dist) 
       if((asl[i] <= dist) && (asl[i] > 0))
	 	n_list[i] = 0;
     }
	
   }
   
   
	
  }

 /* for (i = 0; i < total_Nodes; i++) {
  	 	printf("\nn_list[%d] is %d", i, n_list[i]);
  
   }*/
  
  //printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
	
}

void rebroadcast (int type, float trans_dist) { //type = 1, RNG, type = 0, nonRNG
  
 
   
   double RBT, rand_value;
   int i;
   float temp;
   
   //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);

   //Compute MaxNB and CNB, for RBT computation
   MaxNB = 0;
   CNB = 0;

   for (i = 0; i < peer_num; i++) {
	   temp = atof(cur_node.neighbor_List[i].distance);
     if( (temp <= max_Tx_Range) && (temp != 0))
		MaxNB++;
   }

   for (i = 0; i < total_Nodes; i++) {
	   temp = atof(cur_node.neighbor_List[i].distance);
	 if( (temp <= trans_dist) && (temp != 0))
       CNB++;
	 
   }
     
   
   //calculate RBT
   srand(seed);
   rand_value = (double)rand()/RAND_MAX;
   printf("\n Random_value is %lf", rand_value);

   RBT = ceil((CNB/MaxNB) * max_Delay * rand_value);
   
   printf("\nCNB is %d MaxNB is %d", CNB, MaxNB);
   printf("\n RBT value is %lf", RBT);

   if(type == 1){
   
	/* Establish a handler for SIGALRM signals. */
    signal (SIGALRM, rng_alarm_handler);

	/*set an alarm to wait for Client B to respond*/
	alarm(RBT+3);
      // send_rebroadcast();
	
   }
   else if(type == 0) {
 
   /* Establish a handler for SIGALRM signals. */
	signal (SIGALRM, nonrng_alarm_handler);

	/*set an alarm to wait for Client B to respond*/
	alarm(NON_RNG_TIMEOUT);
    //send_rebroadcast();
 
   }
   //printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
}
 

void nonrng_alarm_handler(int sig) {

  //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);
  send_rebroadcast();
  
  //printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
     
}

void rng_alarm_handler(int sig) {

  //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);
  send_rebroadcast();
  //printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
  
      
     
}
void source_retransmit_handler(int sig) {

  //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);

  if(source_transmit_count+1 >= MAX_RETRANSMITS) {
     alarm_alive = 0;
     source_transmit_count=0;
	 printf("\nMaximum Retransmit limit reached!!! Giving up!!");
	 return;
  }
  
  if(alarm_alive) {
	printf("\nRetransmit timer expired!! Transmitting count is %d\n", source_transmit_count+1);
    sendBroadcast();
    source_transmit_count++;
  }
  
  //printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
     
}

void send_rebroadcast() {
   
  int i, j, k =0;
  int sent_flag = 0;
  char rebroadcast_data[300] = {'\0'};
  float maxRebDist;
   char broadcast_data[20];
  int count = 0;
  int only_neighbor = 0;

  pthread_t retransmitThread_id;
 
 
  void *rcexit_status;
  int retransmitvalue;

  retransmitvalue = 39;

  //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);

  maxRebDist=rebroadcast_dist;
  
  relay_alarm_alive = 1; //resetting for new rebroadcast
  for(i = 0; i < 5; i++) { //scan through broadcast msg queue and send 
	  k = 0;
	  printf("broadcast_queue[%d].message_Type is %c \n",i,broadcast_queue[i].message_Type );
	  if(broadcast_queue[i].message_Type != 'U') {
		  rebroadcast_data[k++] = broadcast_queue[i].message_Type;
		  strcat(rebroadcast_data, "$");
		  strcat(rebroadcast_data, broadcast_queue[i].source);
		  strcat(rebroadcast_data, "$");
		  strcat(rebroadcast_data, cur_node.name); //copying our (relay node) name
		  strcat(rebroadcast_data, "$");
		  strcat(rebroadcast_data, broadcast_queue[i].seq_Num);
		  strcat(rebroadcast_data, "$");

		  gcvt (maxRebDist,5,broadcast_data);
 
		  strcat(rebroadcast_data, broadcast_data);
		  strcat(rebroadcast_data,"$");
		  strcat(rebroadcast_data,"\0");
		 
		  printf("\n Rebroadcast Message contains: %s\n", rebroadcast_data); 

		  for(j=0;j<peer_num;j++) {
		
			  if(send_list[j] == 1) {
		       		        sent_flag = 1;

			

				if (send(sockfd[j], (const void*) rebroadcast_data, strlen(rebroadcast_data), 0) < 0) {
				  fprintf(stderr, "\nError in re-broadcast service request\n");
				  perror("send rebroadcast");

				}
				printf("\nSent re-broadcast  to Peer : %c on Socket : %d \n",connect_sortedNName[j],sockfd[j]);
		
			}//if maxDist check
		  }//end for
		  	

		  //set the sequence_number data to 'sent'
		  if(sent_flag) {
			 sq[sequence_index].sent = 1;

			 broadcast_count++;//keep track total number of rebroadcast's sent
             compute_consumed_power(maxRebDist); //calculate consumed power

            //storing a copy of the sent msg for retransissions
			 retransmit_message.message_Type = 'B';
		     strcpy(retransmit_message.source, broadcast_queue[i].source);
			 strcpy(retransmit_message.retransmit_source, broadcast_queue[i].retransmit_source); 
			 strcpy(retransmit_message.seq_Num, broadcast_queue[i].seq_Num);
			 strcpy(retransmit_message.data, broadcast_queue[i].data);
			 
			 //printf("\nCopied message:\n");
			 //print_message(retransmit_message);

 		  }

		  //mark que entry as un used. 
		  broadcast_queue[i].message_Type = 'U';
		  strcpy(broadcast_queue[i].data, "\0");
          strcpy(broadcast_queue[i].retransmit_source, "\0"); 
		  strcpy(broadcast_queue[i].seq_Num, "\0");
		  strcpy(broadcast_queue[i].source, "\0");
          strcpy (rebroadcast_data, "\0"); 
		  
	  }
 
  }

  /*printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
  for (i = 0; i < 10; i++ ){

	printf("sq[%d].sequencenum is %d \n",i,sq[i].sequencenum);
	printf("sq[%d].sent is %d \n",i,sq[i].sent);
	}*/
	
  for (i = 0; i < total_Nodes; i++) {
	  if(asl[i] > 0) count++;
	  if(asl[recv_message.retransmit_source[0] - 65] > 0) only_neighbor = 1;
  }
   
  if(only_neighbor && (count == 1)) return;  //if you are the leaf node, dont start retransmit timer


   //creating timer thread for retransmission

   if(pthread_create(&retransmitThread_id,NULL,retransmitThread,&retransmitvalue) < 0) {
		printf("\nProblem in thread creation \n");
   }


   pthread_join(retransmitThread_id,&rcexit_status);

}

void *retransmitThread(void *arg) {

//reliability
								
		while ((relay_alarm_alive) && (retransmit_count < MAX_RETRANSMITS)) {
					
		/* Establish a handler for SIGALRM signals. */
		signal (SIGALRM, retransmit_broadcast_handler);
                              
		printf("\nStarting Relay retransmit timer. Try %d\n", retransmit_count+1);
		/*set an alarm for retransmissions*/
		alarm(RETRANSMIT_TIMEOUT);
	    sleep(RETRANSMIT_TIMEOUT+1); 
		
		}

		//reliability

}

void retransmit_broadcast_handler(int sig) {

  //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);

  if(retransmit_count+1 >= MAX_RETRANSMITS) {
     relay_alarm_alive = 0;
	 retransmit_count=0;
	 printf("\nMaximum relay Retransmit limit reached!!! Giving up!!");
	 return;
  }
  
  if(relay_alarm_alive) {
	printf("\nRelay Retransmit timer expired!! Transmitting count is %d\n", retransmit_count+1);
    retransmit_broadcast();
    retransmit_count++;
  }
  
 // printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
     
}

void retransmit_broadcast() {
   
  int j, k;
  char rebroadcast_data[300] = {'\0'};
  float maxRebDist;
  char broadcast_data[20];
  
      //printf("\nENTERED FUNCTION TRACED : %s \n",__FUNCTION__);

      maxRebDist=rebroadcast_dist;
  
 	  k = 0;
	  
	  printf("\nRetransmit Message contains:");
      print_message(retransmit_message);

	  rebroadcast_data[k++] = retransmit_message.message_Type;
	  strcat(rebroadcast_data, "$");
	  strcat(rebroadcast_data, retransmit_message.source);
	  strcat(rebroadcast_data, "$");
	  strcat(rebroadcast_data, cur_node.name); //copying our (relay node) name
	  strcat(rebroadcast_data, "$");
	  strcat(rebroadcast_data, retransmit_message.seq_Num);
	  strcat(rebroadcast_data, "$");

	  gcvt (maxRebDist,5,broadcast_data);
 
	  strcat(rebroadcast_data, broadcast_data);
	  strcat(rebroadcast_data,"$");
	  strcat(rebroadcast_data,"\0");
	
	  //printf("\nRetransmission Rebroadcast Message contains: %s\n", rebroadcast_data); 

	  for(j=0;j<peer_num;j++) {
	  
		  if(send_list[j] == 1) {
		     
		    
			  if (send(sockfd[j], (const void*) rebroadcast_data, strlen(rebroadcast_data), 0) < 0) {
				  fprintf(stderr, "\nError in re-broadcast service request\n");
				  perror("send retransmit rebroadcast");

			  }
				printf("\nSent Rebroadcast Retransmission  to Peer : %c on Socket : %d \n",connect_sortedNName[j],sockfd[j]);
		
			}//if maxDist check
	  }//end for
		  	
  
      broadcast_count++;//keep track total number of rebroadcast's sent
      compute_consumed_power(maxRebDist); //calculate consumed power
	
     // printf("\nLEFT FUNCTION TRACED : %s \n",__FUNCTION__);
      
	 

}
void compute_consumed_power(float tx_dist) {


	float power = 0;

	power = pow(tx_dist, attenuation);

	total_power_consumed += power;


}

