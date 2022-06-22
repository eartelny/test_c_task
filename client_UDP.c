// udp client driver program
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include <string.h>
  
#define PORT 31337
#define MAXLINE 1000
  
//TLV-related stuff - TODO - move to separate module
///TLV-related stuff - TBD - move to separate module
enum TlvType {Amplifier = 0x0, TransieverMode = 0x1, TlvTypeUnknown} tlv_type;


//assume we have it properly aligned and packed without gaps
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

void buildMEssageTypeAmplifier(char* buffer, uint8_t amplifief_value){
    struct TypeLen tl;
    tl.type = Amplifier;
    tl.len = sizeof(uint8_t);
    TLstructToBuf(&tl, buffer);
    char * value_place = buffer + 4;
    uint8Tobuf(amplifief_value,value_place);
}

void buildMessageTypeInvalid(char* buffer, uint16_t len, uint16_t type){
    struct TypeLen tl;
    tl.type = type;
    tl.len = len; 
    TLstructToBuf(&tl, buffer);
    if (len > 0){
      char * value_place = buffer + 4;
      char value[len];
      memcpy(value_place,value,sizeof(value));
    }
}

void buildMessageTypeMode(char* buffer, uint8_t transeiver_mode){
    struct TypeLen tl;
    tl.type = TransieverMode;
    tl.len=2;
    char * value_place = buffer + 4;
    char * value = "rx";
    switch (transeiver_mode){
        case 0:
            memcpy(value_place,value,2);
            break;
        case 1:
            value = "tx";
            memcpy(value_place,value,2);
            break;
        case 2:
            tl.len=4;
            value = "rxtx";
            memcpy(value_place,value,4);
            break;            
    }
    TLstructToBuf(&tl, buffer);
}


int main()
{   
    char buffer[100];
    char message[30];
    memset(&message[0], 0, sizeof(message));
    buildMEssageTypeAmplifier(message,1);
    char * next_message = &message[5];
    buildMEssageTypeAmplifier(next_message,0);
    next_message = &message[10];
    buildMessageTypeMode(next_message,0);
    next_message = &message[16];
    buildMessageTypeMode(next_message,1);
    next_message = &message[22];
    buildMessageTypeMode(next_message,2);;
    //puts(message);

    int sockfd, n;
    struct sockaddr_in servaddr;
      
    // clear servaddr
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;
      
    // create datagram socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
      
    // connect to server
    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(0);
    }
  
    // request to send datagram
    // no need to specify server address in sendto
    // connect stores the peers IP and port
    sendto(sockfd, message, sizeof(message), 0, (struct sockaddr*)NULL, sizeof(servaddr));
      
    // waiting for response
    recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)NULL, NULL);
    puts(buffer);
  
    // close the descriptor
    close(sockfd);
}
