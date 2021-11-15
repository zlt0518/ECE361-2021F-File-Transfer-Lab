#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include "packet.h"

#define MAXBUFLEN 100 
#define FRAGLEN 1000



int intLength(int num);

int main(int argc, char *argv[]){
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	struct timespec begin, end;
	
	if(argc != 3) {
		printf("usage:  deliver server_addr port_num\n");
		exit(1);
	}
	
	
	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM; // Beej Guide's format
	
	int res = getaddrinfo(argv[1], argv[2], &hints, &servinfo);
	if(res != 0){
		printf("getaddrinfo error\n");
		return 1;
	}
	
	
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if((sockfd = socket(p->ai_family, p->ai_socktype,
		p->ai_protocol)) == -1) {
			perror("deliver: socket");
			continue;
		}

		
		break;
		
	} // beej's loop to bind to the first available port
	
	if(p == NULL) {
		printf("deliver: failed to bind socket\n");
		return 2;
	}	
	
	// prompt user to enter a file name
	char cmd[128]; 
	char fileName[128];
	printf("please input: \"ftp <filename>\"  \n");
	scanf("%s%s", cmd, fileName);
	
	// check if file exists
	FILE* file_ptr;
	file_ptr = fopen((const char*)fileName,"rb");
	
	bool f_exist;
	if(file_ptr == NULL) f_exist = false;
	else{
		f_exist = true;
	}
		
	// if no such file, exit. if exists, send ftp to server
	if(!f_exist){
		printf("file does not exist\n");
		exit(1);
	}else{
        printf("File exists\n");
    }
	
	
	// send "ftp" to server
	char* msg = "ftp";
	timespec_get(&begin, TIME_UTC);
	int bytesNum = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen);
	if(bytesNum == -1){
		perror("deliver: sendto");
		exit(1);
	}
	
	// listen back from server
	bytesNum = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *) &their_addr, &addr_len);
	if(bytesNum == -1){
		printf("deliver: recvfrom");
		exit(1);
	}
	timespec_get(&end, TIME_UTC);

    //print the time for the timeout of the round trip time
	long RTT_micro = (long)((end.tv_sec - begin.tv_sec)*1000000 +(end.tv_nsec - begin.tv_nsec)/1000);
	printf("RTT in microsec is: %d\n", RTT_micro);
      
    //set up time for time out with 8 RTT
    struct timeval time_out;
    time_out.tv_sec = 0;
    time_out.tv_usec = RTT_micro* 8;
    int settimer = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &time_out, sizeof(time_out));
    if(settimer<0){
        printf("Error in configuring the timer");
        return 0;
    }
	



	if(strncmp(buf, "yes",3) == 0) printf("A file transfer can start");

	else{
		printf("server replied no\n");
		exit(1);
	}


    //get the length of the file that opened and print out
    fseek(file_ptr,0,SEEK_END);
    int fileLength = ftell(file_ptr);
    fseek(file_ptr, 0, SEEK_SET);


    printf("The total length of the file is:%d \n",fileLength);

    int fileNameLength = strlen(fileName);
     
    //store the file into the buffer with dynamic array
    unsigned char* buffer = (unsigned char*) malloc(sizeof(unsigned char) * (fileLength+1));
    if (!buffer) {
        printf("Memory error!\n");
        exit(0);
    }

	fread(buffer, fileLength, 1, file_ptr);
    printf("file length is %d\n",fileLength);
    printf("buffer is %s\n",buffer);
    fclose(file_ptr);

    //calculate the total number of fragememnt needed
    int totalFragementNum = fileLength/FRAGLEN;
    int remainFragementLength = fileLength%FRAGLEN;

    //check if need to add one more packet and print out
    if(remainFragementLength!=0) totalFragementNum+=1;
    printf("Total number of packets: %d\n", totalFragementNum);


    //create a list of packets and fill in with data
    struct packet* packetList = malloc(sizeof(struct packet) * totalFragementNum);
    
    for(int i = 0; i<totalFragementNum;i++){


        packetList[i].total_frag = totalFragementNum;
        packetList[i].frag_no = i + 1;

        //determine the size of the packet
        if((i == totalFragementNum -1 )&&(remainFragementLength!=0)){
            packetList[i].size = remainFragementLength;

        }else{
            packetList[i].size = FRAGLEN;
        }


        packetList[i].filename = (char*)fileName;
        


        //transfer the file data to packet determine if it is the last packet
        if((i == totalFragementNum -1 )&&(remainFragementLength!=0)){
            for(int j = 0;j<remainFragementLength;j++){
                packetList[i].filedata[j] = buffer[j+i*FRAGLEN];
            }
        }else{

            for(int j = 0;j<1000;j++){
                packetList[i].filedata[j] = buffer[j+i*FRAGLEN];
            }

        } 
       
    }

    //transform the packet list to a char string list and send

    for(int i = 0; i < totalFragementNum; i++){
        int packetStringLength = 4;
        int total_frag_length = intLength(packetList[i].total_frag);
        int frag_no_length = intLength(packetList[i].frag_no);
        int size_length = intLength(packetList[i].size);
        int filename_length = strlen(packetList[i].filename);

        //compute the total length of the packet string length
        packetStringLength+= total_frag_length;
        packetStringLength+= frag_no_length;
        packetStringLength+= size_length;
        packetStringLength+= filename_length;
        packetStringLength+= packetList[i].size;

        char* packetString = (char*)malloc(sizeof(char) * packetStringLength);
 
 
        // add total frag length
        char totalFragString[total_frag_length];
        sprintf(totalFragString, "%d", packetList[i].total_frag);


        int j = 0;
        int semiColumn = j + total_frag_length;

        while(j<semiColumn){
            packetString[j] = totalFragString[j];
            j++;
        }
        


        packetString[j] = ':';
        j++;



        // add total no length
        char fragNoString[frag_no_length];
        sprintf(fragNoString, "%d", packetList[i].frag_no);


        semiColumn = j+frag_no_length;
        while(j<semiColumn){
            packetString[j] = fragNoString[j-1-total_frag_length];
            j++;
        }


        packetString[j] = ':';
        j++;



        // add frag size length
        char fragSizeString[size_length];
        sprintf(fragSizeString, "%d", packetList[i].size);
        
        semiColumn = j+size_length;
        while(j<semiColumn){
            packetString[j] = fragSizeString[j-2-frag_no_length-total_frag_length];
            j++;
        }
        


        packetString[j] = ':';
        j++;

        //add file name
        semiColumn = j+filename_length;
        while(j<semiColumn){

            packetString[j] = packetList[i].filename[j-3-frag_no_length-total_frag_length-size_length];
            j++;

        }
 

        //add the file data
        packetString[j] = ':';
        j++;

        semiColumn = j+packetList[i].size;
        while(j<semiColumn){

            packetString[j] = packetList[i].filedata[j-4-filename_length-frag_no_length-total_frag_length-size_length];
            j++;
        }
        
        printf("the packet string is %s\n",packetString);


        //send the packet string to the server
        int fileNum = sendto(sockfd, packetString, packetStringLength, 0, p->ai_addr, p->ai_addrlen);

	    if(fileNum == -1){
		    perror("Error:fail to deliever the data");
		    exit(1);
	    }

        // wait for acknowledgement
        char buf[MAXBUFLEN];
	    bytesNum = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *) &their_addr, &addr_len);

	    if(bytesNum <0){
            printf("Error in receiving the ACK package\n");
            int resendbytes = sendto(sockfd, packetString, packetStringLength, 0, p->ai_addr, p->ai_addrlen);
            if(resendbytes<0){
		        printf("Error:fail to deliever the data");
		        exit(1);
	        }
            printf("Resending the package\n");

            int resendcount = 1;
            //check if resending succeeded or do resending for at most 5 times 
            while(recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *) &their_addr, &addr_len)<0){
                printf("Error in receiving the ACK package\n");
                printf("1\n");
                resendbytes = sendto(sockfd, packetString, packetStringLength, 0, p->ai_addr, p->ai_addrlen);
                if(resendbytes<0){
                    printf("Error:fail to deliever the data\n");
		            exit(1);

                }
                resendcount++;
                printf("Resending the package %d times\n",resendcount);
                if(resendcount>5){
                    printf("Error: Lost connection, Terminating...\n");
                    return 0;
                }
                printf("2\n");

            }

            printf("Resend succeeded!\n");

		    
	    }
        
	    if(strncmp(buf, "ACK", 3) == 0) printf("Successfully sent the packet\n");
        


        free(packetString);

    }

    free(buffer);
    free(packetList);
	close(sockfd);
	
	return 0;
	
}



//function transform integer to the number of digit
int intLength(int num){
    int i = 0;
    while(num!=0){
        num = num/10;
        i=i+1;
    }

    return i;
}

