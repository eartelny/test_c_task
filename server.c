// server program for udp connection

//define below is required for resolving sigaction - not a functional requirement
#define _POSIX_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h> //for exit()
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include <fcntl.h> //non-blocking IO
#include <errno.h>
//#include <time.h> // for usleep

//just not handle fcntl results
int guard(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }


#define PORT 5000
#define MAXLINE 1000


//sigterm handling block 1 of 2
volatile sig_atomic_t done = 0;

void term(int signum)
{
    done = 1;
    fflush(stdout);
}
//end of sigterm handling block 1 of 2

int main()
{   
    puts("press ctrl+C to exit");
    //sigterm handling block 2 of 2
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
    //end of sigterm handling block 2 of 2
    
    //set non-blocking stderr and stdout - TODO - seems not working now due to wrong usage
    int flags = guard(fcntl(STDOUT_FILENO, F_GETFL),"unable to get stdout flags");
    guard(fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK),"unable to set stdout non-blocking");
    
    int err_flags = guard(fcntl(STDERR_FILENO, F_GETFL),"unable to get stderr flags");
    guard(fcntl(STDERR_FILENO, F_SETFL, err_flags | O_NONBLOCK),"unable to set stderr flags");
    //end of set non-blocking stderr and stdout

    char buffer[100];
    char *message = "Hello Client";
    int listenfd, len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));
  
    // Create a UDP Socket
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    //int socket_flags = guard(fcntl(listenfd, F_GETFL), "could not get flags on TCP listening socket");
    //guard(fcntl(listenfd, F_SETFL, flags | O_NONBLOCK), "could not set UDP listening socket to be non-blocking");
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET; 
   
    // bind server address to socket descriptor
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
       
    //receive the datagram
    len = sizeof(cliaddr);
    while(!done)
    {
        int n = recvfrom(listenfd, buffer, sizeof(buffer),
            0, (struct sockaddr*)&cliaddr,&len); //receive message from server
        if (n !=0)
        {
             buffer[n] = '\0';
            //puts(buffer);
            fprintf(stderr, "%s\n", buffer);
            // send the response
            sendto(listenfd, &buffer, n-3, 0,
                (struct sockaddr*)&cliaddr, sizeof(cliaddr));
        }
    }
}