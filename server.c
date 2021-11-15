#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <random>
#include "packet.h"

#define buffer_size 1050

//part of the code is cited from https://beej.us/guide/bgnet/

void decode_packet(char* buffer, struct packet* pac);

int main(int argc, char const *argv[]){

    //get the port number
    if (argc !=2) {
        printf("Invalid number of arguments!");
        return(1);
    }

    // socket()
    struct addrinfo hints;
    struct addrinfo* res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    int rv = getaddrinfo(NULL, argv[1], &hints, &res);

    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    //check if the socket is legel
    if (s < 0){
        printf("Error Socket");
        return (1);
    }

    // bind()
    int bindinfo = bind(s, res->ai_addr, res->ai_addrlen);


    //check if the binding is success else return the failure message
    if (bindinfo == -1) {
        printf("Binding failure");
        exit (1);
    }

    //do the job of receiving the message from udp
    //define the address of sinder and szie of address
    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof client_address; // length of client info
    char buffer[buffer_size];

    // do the receiving
    int num_bytes = recvfrom(s, buffer, buffer_size, 0, (struct sockaddr *)&client_address, &client_address_len);

    if (num_bytes < 0){

        printf("Error receiving the message!");
        return 1;
    }

    //check if it is the ftp requirement
    if (strncmp(buffer, "ftp", 3) == 0){
        //do the reply for correct message
        int reply_info = sendto(s, "yes", strlen("yes"), 0, (struct sockaddr *) &client_address, client_address_len);

        //check if the reply success
        if (reply_info < 0){
            printf("Error in replying the ftp message!");
            return 1;
        }else{
            //print out the message of successful connection
            printf("Connection succeeded!\n");
        }

    }
	else{
        
        // send info back to the client
        int reply_info = sendto(s, "no", strlen("no"), 0, (struct sockaddr *) &client_address, client_address_len);

        //check if the reply success
        if (reply_info < 0){
            printf("Error in replying 'no' back to the client!");
            return 1;

        }else{
            printf("Success in replying 'no' to the client!");
			return 0;
        }

    }
	
	// process packet transfers
	bool t_end = false;
	
	FILE* t_file = NULL;
	
	while(!t_end)
	{
		struct packet rcv_packet;

		int num_bytes = recvfrom(s, buffer, buffer_size, 0, (struct sockaddr *)&client_address, &client_address_len);
		
		if(num_bytes < 0)
		{
			perror("Receiving packet failed...");
		}
		
		buffer[num_bytes] = '\0';
		
		decode_packet(buffer,&rcv_packet);
		
		// if file not created, create one and pass to file ptr
		if(t_file == NULL)
		{
			char* f_n = (char*) malloc(sizeof(char)*strlen(rcv_packet.filename)+2);
			f_n[0] = 't';
			f_n[1] = '_';
			strcat(f_n, rcv_packet.filename);
			
			t_file = fopen(f_n,"wb");
		}
		
		//write packet data to file
		fwrite(rcv_packet.filedata,1,rcv_packet.size,t_file);
		
		// send ACK to client
		int reply_info = sendto(s, "ACK", 3, 0, (struct sockaddr *) &client_address, client_address_len);
		
		if(rcv_packet.total_frag == rcv_packet.frag_no)
		{
			t_end = true;
			printf("Transfer complete");
		}	
	}
	
	fclose(t_file);
    
    return 0;
}

void decode_packet(char* buffer, struct packet* pac)
{
	printf("buffer content: \n %s\n",buffer);
	int colon_one, colon_two, colon_three, colon_four;
	
	char* total_frag_string;
	char* frag_no_string;
	char* size_string;
	char* file_name;
	
	unsigned int total_frag, frag_no, size;
	
	// use for loop to obtain total frag string
	for(int i = 0; i < buffer_size; i++)
	{
		if(buffer[i] == ':')
		{
			colon_one = i;
			break;
		}
	}
	
	total_frag_string = (char*) malloc(sizeof(char) * (colon_one));
	
	for(int i = 0; i < colon_one; i++)
	{
		total_frag_string[i] = buffer[i];
	}
	
	total_frag = atoi(total_frag_string);
	
	// loop to find the frag no string
	for(int i = colon_one + 1; i < buffer_size ; i++)
	{
		if(buffer[i] == ':')
		{
			colon_two = i;
			break;
		}
	}
	
	frag_no_string = (char*) malloc(sizeof(char) * (colon_two - colon_one - 1));
	
	for(int i = 0; i < colon_two - colon_one - 1; i++ )
	{
		frag_no_string[i] = buffer[i+colon_one+1];
	}
	
	frag_no = atoi(frag_no_string);
	
	// loop to find the size string
	for(int i = colon_two + 1; i < buffer_size ; i++)
	{
		if(buffer[i] == ':')
		{
			colon_three = i;
			break;
		}
	}
	
	size_string = (char*) malloc(sizeof(char) * (colon_three - colon_two - 1));
	
	for(int i = 0; i < colon_three - colon_two - 1; i++ )
	{
		size_string[i] = buffer[i+colon_two+1];
	}
	
	size = atoi(size_string);
	
	// loop to find filename string
	for(int i = colon_three + 1; i < buffer_size ; i++)
	{
		if(buffer[i] == ':')
		{
			colon_four = i;
			break;
		}
	}
	
	file_name = (char*) malloc(sizeof(char) * (colon_four - colon_three - 1));
	
	for(int i = 0; i < colon_four - colon_three - 1; i++ )
	{
		file_name[i] = buffer[i+colon_three+1];
	}
	
	//copy the parameter and data into the struct packet pac
	
	pac->total_frag = total_frag;
	pac->frag_no = frag_no;
	pac->size = size;
	
	pac->filename = (char*) malloc(sizeof(char) * (colon_four - colon_three - 1));
	
	for(int i = 0; i < colon_four - colon_three - 1; i++)
	{
		pac->filename[i] = file_name[i];
	}
	
	for(int i = 0; i < size; i++)
	{
		pac->filedata[i] = buffer[i+colon_four+1];

	}

	printf("filedata content: \n %s\n", pac->filedata);
	
}
