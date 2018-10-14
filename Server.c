#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

// %p = char**

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, buffersize = 256, n;
     socklen_t clilen;
     char buffer[buffersize];
     struct sockaddr_in serv_addr, cli_addr;
       	 	 
  	// printf("This is argc %d\n", argc);
  	// printf("This is argv %s\n", *argv);
  	// printf("This is the port number %s\n", argv[1]); 
  	 
	 if (argc < 2) {
	     fprintf(stderr,"ERROR, no port provided\n");
	     exit(1);
	 }
    
	 sockfd = socket(AF_INET, SOCK_STREAM, 0);                               // open new socket
	 if (sockfd < 0) { error("ERROR opening socket"); }                      // check if socket is open
	 bzero((char *) &serv_addr, sizeof(serv_addr));                          // place zeros in server address
		 
	 portno = atoi(argv[1]);                                                 // read in port number from terminal
	                         
		 
	 serv_addr.sin_family = AF_INET;
	 serv_addr.sin_addr.s_addr = INADDR_ANY;
	 serv_addr.sin_port = htons(portno);         // converts the unsigned short integer from host byte order to network byte order.
		 
	 if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
	 	error("ERROR on binding");
	 }
		
	 while(1) { 
		 listen(sockfd,5);                                                                     // listen for client           
		 clilen = sizeof(cli_addr);                                                            // save client address length
		 newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);                    // accept client 
		 if (newsockfd < 0) { error("ERROR on accept"); }
		 bzero(buffer,buffersize);                                                             // fill buffer with 0's
		 n = read(newsockfd,buffer,buffersize-1);                                              // read message into buffer
		 if (n < 0) { error("ERROR reading from socket"); }
		 printf("Here is the message: %s\n",buffer);                                           // print received message from buffer
		 n = write(newsockfd,"I got your message",18);                                         // send ack message
		 if (n < 0) { error("ERROR writing to socket"); }
		 //close(newsockfd);                                                                     // close socket
		 //close(sockfd);
     }
     return 0; 
}
