#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(const char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {

 	 int sockfdc, sockfds, newsockfd, portno, buffersize = 256, n;
     socklen_t clientlength;
     char buffer[buffersize];
     struct sockaddr_in server_address, server_address_2, client_address;
     struct hostent *server;

   
   //========================================================================================//
   //                  opens socket and accepts a client then reads message                  //
   //========================================================================================//
   
            
     printf("Reading port number... \n");
     portno = atoi(argv[1]);                                   // read port number from terminal
     
     printf("Reading server ip address... \n");
     server = gethostbyname(argv[2]);                          // read server ip from terminal
     if (server == NULL) {                                     // check if server exists
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
     } else {
     	printf("Server exists!\n");
     }
          
     printf("Opening socket for client to connect to... \n");
     sockfds = socket(AF_INET, SOCK_STREAM, 0);                // open new socket
     if (sockfds < 0) { error("ERROR opening socket"); }       // check if open
 
 	 bzero((char *) &server_address, sizeof(server_address));  // fill server address var with zeros
     server_address.sin_family = AF_INET;
     server_address.sin_addr.s_addr = INADDR_ANY;                   // server stuff
     server_address.sin_port = htons(portno);
     
     printf("Binding socket...\n");                                             // bind socket to address
     if (bind(sockfds, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
         error("ERROR on binding");
     }
     
     printf("Listening...\n");
     listen(sockfds,5);                                                                  // listen for client
     clientlength = sizeof(client_address);                                    // save client address length
     printf("Accepting client...\n");
     newsockfd = accept(sockfds, (struct sockaddr *) &client_address, &clientlength);      // accept client
     if (newsockfd < 0) { error("ERROR on accept"); }                                     // check if accepted
     
     
     bzero(buffer,buffersize);                                                 // fill msg buffer with 0's
     printf("Reading client message...\n");
     n = read(newsockfd,buffer,buffersize-1);                                  // read client msg into buffer
     if (n < 0) { error("ERROR reading from socket"); }                        // check read
     
     printf("Message received from client: %s",buffer);                     // print msg
     n = write(newsockfd,"The proxy recieved your message",31);                // send confirmation to client
     if (n < 0) error("ERROR writing to socket");
     
   //  close(newsockfd);
   //  close(sockfds);

     
     // ================================================================================================= //
     //                          opens socket and forwards message to server                              //
     // ==================================================================================================//
     
     
     printf("Opening socket for communication to server... \n");
     sockfdc = socket(AF_INET, SOCK_STREAM, 0);                             // open socket to com with server
     if (sockfdc < 0) { error("ERROR opening socket"); }                    // check if open
     
     bzero((char *) &server_address_2, sizeof(server_address_2));          // fill server addr with 0's
     server_address_2.sin_family = AF_INET;
     bcopy((char *)server->h_addr, (char *)&server_address_2.sin_addr.s_addr, server->h_length);
     server_address_2.sin_port = htons(portno+1);
     
     if(connect(sockfdc,(struct sockaddr *) &server_address, sizeof(server_address)) < 0) { // connect to server
        error("ERROR connecting");
     }
    
     printf("Sending: %s",buffer);                     // print msg to send
     n = write(sockfdc,buffer,strlen(buffer));
     if (n < 0) { error("ERROR writing to socket"); }    
    // close(sockfdc);
     
	 return 0;
}
