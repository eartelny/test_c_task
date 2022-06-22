// server program for udp connection

//define below is required for resolving sigaction - not a functional requirement
#define _POSIX_SOURCE

//TODO - align alphabetically
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
#include <stdint.h>
//#include <time.h> // for usleep

//helper fnction just to handle fcntl results
int guard(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }


#define PORT 31337
#define MAXLINE 1000
#define MAX_VALUE_LEN 4
#define MAX_SAFE_UDP_PAYLOAD 508
#define MAX_POSSIBLE_UDP_PAYLOAD 67008


//sigterm handling block 1 of 2
volatile sig_atomic_t done = 0;

void term(int signum)
{
    done = 1;
    fflush(stdout);
}
//end of sigterm handling block 1 of 2

//TLV-related stuff - TODO - move to separate module
enum TlvType {Amplifier = 0x0, TransieverMode = 0x1, TlvTypeUnknown} tlv_type;


//TODO assume we have it properly aligned and packed without gaps
struct TypeLen{
    uint16_t type;
    uint16_t len;
};

//a couple of methods for parsing/making TypeLen structure with type and len
void TLstructToBuf(struct TypeLen * tl, char* buffer){
    char * current_buffer_pointer = buffer;
    uint16_t type = htons(tl->type);
    uint16_t len = htons(tl->len);
    memcpy(current_buffer_pointer,(char*)&type, sizeof(tl->type));
    current_buffer_pointer += sizeof(tl->type);
    memcpy(current_buffer_pointer,(char*)&len, sizeof(tl->len));
};

struct TypeLen bufToTLstruct(char* buffer){
    struct TypeLen tl;
    uint16_t curr_value;
    char* offsetted_buffer = buffer;
    memcpy(&curr_value,offsetted_buffer, sizeof(tl.type));
    tl.type = ntohs(curr_value);
    offsetted_buffer+=sizeof(tl.type);
    memcpy(&curr_value,offsetted_buffer, sizeof(tl.len));
    tl.len = ntohs(curr_value);
    return tl;
};

// a couple of methods to work with bool value
uint8_t bufToUint8(char* buffer)
{
    uint8_t result;
    memcpy(&result,buffer,sizeof(uint8_t));//TODO - might need to pass buffer len, but - we know the type and len - actually no need to pass
}

void uint8Tobuf(uint8_t value, char* buffer)
{
    memcpy(buffer,(char*)&value, sizeof(value));
}


//actual processing of parsed messages could be done here. now just printing to stderr
void processParsedTLV(struct TypeLen * tl, char * value, char* original_buffer)
{
    fprintf(stderr, "Type: %d Len: %d ", tl->type, tl->len);
    switch(tl->type){
        case Amplifier:
            fprintf(stderr, "Value: %d\n",bufToUint8(value));
            break;
        case TransieverMode:
            //&value[tl->len] = '\0';//TODO - no special processing for transiever modes yet - just print as is
            fprintf(stderr, "Value: %s\n",value);
            break;
        default:
            fprintf(stderr, "Value: SKIPPED\n");
            break;

    }
    if (tl->type != TlvTypeUnknown)
    {
       uint16_t target_offset = sizeof(uint16_t)*2 +tl->len;
       char buffer_for_sending[target_offset];
       memcpy(buffer_for_sending,original_buffer,target_offset);
       puts(buffer_for_sending);//TODO - send to destination
    }

}

//read one TLV fragment from the buffer, return read bytes
//value must be allocated to max possible len for "value" field
uint16_t readNextTLVfragment(char * buffer , uint16_t buffer_len, struct TypeLen * tl, char* value)
{
    uint16_t parsedTLVmessageType = TlvTypeUnknown;
    uint16_t sizeOfTypeLen = sizeof(tl->type) + sizeof(tl->len);
    if (buffer_len < sizeOfTypeLen  )//TODO use packed TypeLen to just take its sizeof 
    {
        return buffer_len;//TODO - also need to handle as error
    }
    struct TypeLen tl_from_buffer = bufToTLstruct(buffer);//read typelen into struct
    tl->type = tl_from_buffer.type;
    tl->len = tl_from_buffer.len;
    uint16_t target_offset = sizeOfTypeLen + tl->len;
    if (target_offset > buffer_len)
    {
       return buffer_len;//TODO - need to handle as and error case - TLV len is set more than buffer_len
    }
    memcpy(value,buffer + sizeOfTypeLen, tl->len);
    switch(tl->type){
        case Amplifier:
            parsedTLVmessageType = Amplifier;
            //readAmplifierParam(buffer,tl->len, value); - in case we have some complex - it cound be done in separate functions, rather than memcpy into *value
            break;
        case TransieverMode:
            parsedTLVmessageType = TransieverMode;
            //readTrancieverParam(buffer,tl->len, value);
            break;
        default:
            //TODO - bad practice to assign here, maybe better to make this function return some error code here instead.
            tl->type = TlvTypeUnknown;
            break;
    }

    processParsedTLV(tl,value, buffer);
    uint16_t read_bytes = sizeOfTypeLen + tl->len;
    if (read_bytes < buffer_len){
        return read_bytes;
    }else{
        return buffer_len;//have read whole buffer, nothing to read anymore;
    }
}




void start_udp_server()
{
    char buffer[MAX_SAFE_UDP_PAYLOAD];
    
    int listenfd, len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));
  
    // Create a UDP Socket
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    //TODO: bad practice - better to use poll/select with blocking socket - TODO - implement with using POLL
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

        struct TypeLen tl; 
        char value[MAX_VALUE_LEN];
        uint16_t remaining_bytes = n;
        uint16_t buffer_offset = 0;//TODO - too complicated logic with offsets
        while (buffer_offset < n){
          memset(&value[0], 0, sizeof(value));
          uint16_t read_bytes = readNextTLVfragment(&buffer[buffer_offset], remaining_bytes, &tl, &value[0]);
          buffer_offset = buffer_offset + read_bytes;
          remaining_bytes = remaining_bytes - read_bytes;
        }
      
       
        //buffer[n] = '\0';
            //puts(buffer);
            //fprintf(stderr, "%s\n", buffer);
            // send the response
        //sendto(listenfd, &buffer, n, 0,
        //        (struct sockaddr*)&cliaddr, sizeof(cliaddr));

    }
}


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


    start_udp_server();
}
