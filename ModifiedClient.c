/******************************************************************************************************************************************

        -Code for cyclic redundancy check taken from 
        https://stackoverflow.com/questions/23478417/cyclic-redundancy-check-calculation-c-to-c-sharp-code-conversion-not-working
        
        -Code for queue structure, now64() etc was taken from Tim Seeley's unreliable relay server and modified for use within this code.

 		-Utilizes "Go back N" method 
        	-Sender buffers unacknowledged frames.
        	-If ACK timeout occurs, sender retransmits everything beginning with the non-ACKed frame
        	-Receiver rejects frames with a missing predecessor.
        
        Although it works fine on most of the tested cases, the following code might be missing a few features related to:
        
             - Making timeout variable	
        		- including timestamp in header to calc RTT
        		- TIMEOUT = RTT + X
        	 - Fixing timeout logic, make Go back N more efficient
        	 - Allowing user to quit by joining threads in main()
        	 
        The authors acknowledge this and are working on a way to fix those issues. Due to time constraints we were not able to implement a 
        perfect solution.
        
        IMPORTANT: 
        
        	-This program has a high RAM demand and may freeze your PC if allowed to continue running for extended periods of time. 
        	Monitor RAM usage when handling large files. 
        
        	-Testing was done on multiple PC's and the results varied depending on the task and the machine. The testing revealed that 
        	on some machines, the code did not function at all. We were unable to determine why this was and as such, the remainder of the 
        	testing was done on a machine that had no issues (i.e the code functioned properly). 
         		
 *****************************************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <poll.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>

/************************************************************ GLOBAL VARIABLES ***********************************************************/

int BUFFERSIZE = 256, SEQ = 1000, HEADERSIZE = 10, TIMEOUT = 200, Qsize = 0, VERBOSE = 0;
long long timer;

struct ThreadVars {
    int sockfd;
    struct mq* msq;
};

struct mqn {
    char *msg;
    int SEQ; 
    struct mqn *next;
    struct mqn *prev;
};

struct mq {
    struct mqn *head;
    struct mqn *tail;
};

/************************************************************ FUNCTION HEADERS ***********************************************************/

void *ReceiveSTDIN(void *Vars);                      // Function for thread to read STDIN and send MSG's
void *ReceiveSOCKET(void *Vars);                     // Function for thread to read from SOCKET and perform required tasks 
unsigned short CalculateCRC(char* buffer, int size); // This function calculates the cyclic redundancy checksum.
long long now64();                                   // This function gets the current system time as a 64-bit integer in microseconds 
struct mq *make_queue();                             // This function allocates memory for the queue data structure 
void enqueue(struct mq *q, char *msg, int SEQ);      // This function inserts new messages into the queue ordered by SEQ
char *dequeue(struct mq *q);                         // This function removes and returns the head of the queue
void dump_queue(struct mq *q);                       // This function prints the contents of the queue
void GoBackN(void* Vars);                            // This function resends all MSGS in the MSG queue from last non ACK'd MSG
int isDigit(char i);                                 // This function determines if the char provided is a digit
int itoa(int num);                                   // This function converts an int to ASCII
void error(const char *msg);

/**************************************************************** MAIN ********************************************************************/

int main(int argc, char *argv[]) {
    int sockfd, portno, temp = 0;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    struct pollfd poll_array[2];
    pthread_t id1, id2;

    if(argc < 3) {
        fprintf(stderr, "usage %s hostname port, v can be used for verbose mode\n", argv[0]);
        exit(0);
    }
    
    if(argv[3]) {
		if(argv[3][0] == 'v') VERBOSE = 1;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if(sockfd < 0) error("ERROR opening socket"); 
    
    server = gethostbyname(argv[1]);
    if(server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr)); 
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    /* Connect to server socket */
    
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) { 
        error("ERROR connecting");
    }

    /* Initilize variables to pass to threads */
    
    struct ThreadVars vars;
    vars.sockfd = sockfd;
    vars.msq = make_queue();

    /* Start worker threads */
    
    pthread_create(&id1, NULL, ReceiveSTDIN, &vars);
    pthread_create(&id2, NULL, ReceiveSOCKET, &vars);

    // TODO: create logic to join threads and end program if user wants to quit
    
    while(1) {} // pause main thread here 

    close(sockfd);
    return 0;
}

/*******************************************************FUNCTION DEFINITIONS**************************************************************/

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void *ReceiveSTDIN(void *Vars) {
    char buffer[BUFFERSIZE];
    unsigned short crc;
    struct ThreadVars *var = Vars;
    int n, i, j, k, l, m;

    while(1) {
    
    	/* Read MSG from STDIN */
    	       
        bzero(buffer, BUFFERSIZE);
        fgets(buffer, BUFFERSIZE - 1, stdin); 

        /* Create header (4 Digit SEQ + 'M' or 'A' + 5 Digit CRC) */
        
        char *header = (char*) malloc(BUFFERSIZE + HEADERSIZE); 
        bzero(header, BUFFERSIZE + HEADERSIZE);

        i = SEQ % 10;                          
        j = ((SEQ % 100) - i) / 10;             
        k = ((SEQ % 1000) - j - i) / 100;     
        l = ((SEQ % 10000) - k - j - i) / 1000; 
        header[0] = itoa(l);                     
        header[1] = itoa(k);
        header[2] = itoa(j);
        header[3] = itoa(i);
        header[4] = 'M';

		crc = CalculateCRC(buffer, strlen(buffer));
        i = crc % 10;                               
        j = ((crc % 100) - i) / 10;                   
        k = ((crc % 1000) - j - i) / 100;            
        l = ((crc % 10000) - k - j - i) / 1000;       
        m = ((crc % 100000) - l - k - j - i) / 10000; 
        header[5] = itoa(m);
        header[6] = itoa(l);
        header[7] = itoa(k);
        header[8] = itoa(j);
        header[9] = itoa(i);

        /* Copy MSG into new array with the header */

        for(i = HEADERSIZE; i < strlen(buffer) + HEADERSIZE; i++) {
            header[i] = buffer[i - HEADERSIZE];
        }
		
		if(crc) {
		
			/* Store MSG with attached header in queue */
		
        	enqueue(var->msq, header, SEQ);
        	if(VERBOSE) {
        		dump_queue(var->msq); 
        		fprintf(stderr, "Sending %s \n", header); 
        	}   		 
        
        	/* Send MSG with attached header */

        	n = write(var->sockfd, header, strlen(header));
        	if(n < 0) error("ERROR writing to socket"); 
        	timer = now64(); // Store time MSG was sent 
        	SEQ++;
        }       
    }
}

void *ReceiveSOCKET(void *Vars) {
    struct ThreadVars *var = Vars;
    struct pollfd pollvar;
    pollvar.fd = var->sockfd;
    pollvar.events = POLLIN;
    int n, i, j, k, l, crc_num, crc_rcvd, rcvd_SEQ_num;
    int expected_SEQ = 1000, ACK_SEQ = 0, ret = 0;
    char buffer[BUFFERSIZE], *msg;
    char crc[5], rcvd_SEQ[4], ACK[6];

    while(1) {
        bzero(buffer, BUFFERSIZE);
        bzero(ACK, 5);

        ret = poll(&pollvar, 1, TIMEOUT); 

        if(ret > 0) {
            n = read(var->sockfd, buffer, BUFFERSIZE - 1);
            if(n < 0) error("ERROR reading from socket"); 
            if(buffer[4] == 'A') { 
            
                /* Remove ACK'd MSG from send queue */  
                
                if(isDigit(buffer[0]) && isDigit(buffer[1]) && isDigit(buffer[2]) && isDigit(buffer[3])) { // If ACK is not corrupt
		            struct mqn *m = var->msq->head; 
		            ACK_SEQ = (buffer[0] - '0')*1000 + (buffer[1] - '0')*100 + (buffer[2] - '0')*10 + (buffer[3] - '0');

					/* Delete all MSG's from queue with ACK'd SEQ */

		            while(m) { 
	  	            	if(m->SEQ == ACK_SEQ) { // Shouldnt need to check as ACK'd msg should be at front of the queue
	  	            		if(VERBOSE) fprintf(stderr, "Removing MSG from queue %s", m->msg); 
	  	            		dequeue(var->msq); 
	  	            	} 
	  	            	m = m->next;           	
		            }
                }
            } else if(buffer[4] == 'M') { 
            
            	/* Get SEQ from header */
            
                for(i = 0; i < 5; i++) rcvd_SEQ[i] = buffer[i];                
                rcvd_SEQ_num = atoi(rcvd_SEQ);                       
                
                /* Get CRC from header */
                              
                for(i = 5; i < 10; i++) crc[i - 5] = buffer[i];                
                crc_num = atoi(crc);
                
                /* Store received MSG and calculate CRC */
                
                msg = (char*) malloc(sizeof(buffer) - 10);
                for(i = 10; i < sizeof(buffer); i++) msg[i - 10] = buffer[i];       
                crc_rcvd = CalculateCRC(msg, strlen(msg));                 
                if(crc_num == crc_rcvd) {              // If MSG is not corrupted              
                    if(rcvd_SEQ_num <= expected_SEQ) { // If SEQ is the one we're expecting (received in order) or an older msg (ACK didnt make it back)
                    
                        /* Send ACK for received MSG */

                        i = rcvd_SEQ_num % 10;                            
                        j = ((rcvd_SEQ_num % 100) - i) / 10;            
                        k = ((rcvd_SEQ_num % 1000) - j - i) / 100;        
                        l = ((rcvd_SEQ_num % 10000) - k - j - i) / 1000;  
                        ACK[0] = itoa(l);                        
                        ACK[1] = itoa(k);
                        ACK[2] = itoa(j);
                        ACK[3] = itoa(i);
                        ACK[4] = 'A';
                        ACK[5] = '\n'; 

						if(VERBOSE) fprintf(stderr,"Sending ACK %s\n", ACK); 
                        n = write(var->sockfd, ACK, strlen(ACK));
                        if(n < 0) error("ERROR writing to socket"); 

                        /* Display received MSG */
                        
						if(rcvd_SEQ_num == expected_SEQ) { // If the MSG received was received in order.
                        	fprintf(stdout, "%s", msg);
                        	fflush(stdout);
                        	free(msg);
                        	expected_SEQ++;
                        }                      
                    } else {
                        if(VERBOSE) { 
                        	fprintf(stderr, "SEQ not correct: rcvd_SEQ_num = %d expected_SEQ = %d \n",rcvd_SEQ_num ,expected_SEQ);  
                        }
                    }
                } else {
                    if(VERBOSE) { 
                    	fprintf(stderr, "Checksum not correct: CRC from header = %d Calculated CRC from received MSG = %d \n", crc_num, crc_rcvd); 
                    }
                }
            } else {
                if(VERBOSE) { 
               		fprintf(stderr, "Message header was corrupted: Found MSG descriptor = %c \n", buffer[4]);
               	}
            }
        } else if(!ret) { // logic may not be correct
        	if(VERBOSE) fprintf(stderr, "Timeout\n"); 
        	GoBackN(var);            
        }
    }
}

int isDigit(char i) {
	return i - '0' < 0 || i - '0' > 9 ? 0 : 1;
}

int itoa(int num) {
	return num + '0';
}

// TODO: Make "Go back N" method more efficient, currenty using stop and wait equivilent...
void GoBackN(void *Vars) {
    struct ThreadVars *var = Vars;
    struct mqn *m;
    int n, done = 1;
    
    if(Qsize > 0) {          
    	m = var->msq->head; 
    	done = 0;
    	if(VERBOSE) fprintf(stderr, "Resending %s \n", m->msg); 
    	n = write(var->sockfd, m->msg, strlen(m->msg));
    }
    
	// The code below is the go back n logic, currently commented out for testing

 //   while(!done) {                                                              
 //       n = write(var->sockfd, m->msg, strlen(m->msg));
 //       if (n < 0) error("ERROR writing to socket"); 
 //       if(m->next) m = m->next;  
 //       else done = 1; 
 //   }    
}

unsigned short CalculateCRC(char* buffer, int size) {
    unsigned short cword = 0, ch;
    int i, j;

    for(i = 0; i < size; i++) {
        ch = buffer[i] << 8;
        for(j = 0; j < 8; j++) {
            if((ch & 0x8000)^(cword & 0x8000)) cword = (cword <<= 1)^4129;
            else cword <<= 1;
            ch <<= 1;
        }
    }
    return cword;
}

struct mq *make_queue() {
    struct mq *q = (struct mq*) malloc(sizeof(struct mq));
    if(!q) error("ERROR: malloc() failed in make_queue()\n"); 
    bzero(q, sizeof(struct mq));
    return q;
}

void enqueue(struct mq *q, char *msg, int SEQ) {
    struct mqn *m = (struct mqn*) malloc(sizeof(struct mqn));
    if(!m) error("ERROR: malloc() failed in function enqueue()\n"); 
    if(VERBOSE) fprintf(stderr, "Adding to queue %s", msg);  
    bzero(m, sizeof(struct mqn));
    Qsize++;
    m->msg = msg;
    m->next = 0;
    m->SEQ = SEQ;

    if(!q->tail) { // insert into empty queue 
        assert(!q->head);
        q->head = m;
        q->tail = m;
    } else {
        assert(q->tail);
        assert(q->head); // insert into queue sorted by SEQ value (smallest first) 
        if(m->SEQ >= q->tail->SEQ) { // append after tail of queue - should be the common case 
            q->tail->next = m;
            m->prev = q->tail;
            q->tail = m;
        } else if(m->SEQ < q->head->SEQ) { // insert before head of queue 
            m->next = q->head;
            q->head->prev = m;
            q->head = m;
        } else { // walk the list to find insert point
            struct mqn *insert, *previous;
            assert(q->head != q->tail); // at least 2 items in queue
            insert = q->head;
            while(insert->SEQ <= m->SEQ) {
                previous = insert;
                insert = insert->next;
            }

            assert(insert && previous); // insert here, between 'previous' and 'insert' 
            m->next = insert;
            insert->prev = m;
            previous->next = m;
            m->prev = previous;
        }
    }
}

char *dequeue(struct mq *q) {
    if(!q->head) { // queue is empty 
        assert(!q->tail);
        return 0;
    } else {
        assert(q->tail); // queue is non-empty 
        struct mqn *m = q->head; // the head message is ready to send, so let's remove it from the queue 
        q->head = q->head->next;
        if(q->head) q->head->prev = 0;
        else  q->tail = 0;  // we just removed last item, so queue is now empty 
        char *msg = m->msg;
        free(m);
        Qsize--;
        if(VERBOSE) fprintf(stderr, "Queue size = %d\n", Qsize); 
        return msg;
    }
}

void dump_queue(struct mq *q) {
    struct mqn *n = q->head;
    int i;
    char* buf;
	
	fprintf(stderr, "-------Queue contents--------\n\n");
    while(n) {
        buf = n->msg;
        for(i = 0; i < strlen(n->msg); i++) fputc(buf[i], stderr);
        n = n->next;
    }
    fprintf(stderr, "\n----End of queue contents----\n");
}

long long now64() {
    struct timeval now;
    bzero(&now, sizeof(struct timeval));
    if(gettimeofday(&now, 0)) error("ERROR: gettimeofday() failed\n"); 
    return (long long) (now.tv_sec)*1000000 + now.tv_usec;
}
