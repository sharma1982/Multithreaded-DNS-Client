#include "stdio.h"
#include "string.h"
#include "malloc.h"

#include "pthread.h"
#include "unistd.h"
#include "sys/stat.h"
#include "sys/mman.h"
#include "fcntl.h"

#include "netinet/in.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "DNSClient.h"

unsigned int eachThreadShare=0;
pthread_t  allThread[101]={0};
unsigned char dnsServer[5][50], dnsFileName[MAX_FILE_NAME];
pthread_mutex_t dns_mutex;

/*
Description: 	This function responsible to initialize the dns srrvers.
Input: 		None
OutPut: 	None
*/
void dns_init_server()
{
        int count=0;
	char *dnsIp = "10.31.2.10";

        strcpy(dnsServer[0], dnsIp);
        strcpy(dnsServer[1], dnsIp);
        strcpy(dnsServer[2], dnsIp);
        strcpy(dnsServer[3], dnsIp);
        strcpy(dnsServer[4], dnsIp);

}

/*
Description: 	Main entry of the program. Calculate each thread's share. 
		Spawn thread and in end, consolidate all the file in one out put file.
Input: 		Standard inputs;
OutPut: 	0 for error, 1 for success.
*/
int main(int argc, char* argv[])
{
	/*local variables*/
        unsigned int numThread=0;
        FILE *fOutFile = NULL, *fThreadFile = NULL;
        struct stat stStat={0};
        int fqdnFileSize=0;
        int threadCount=0, rval=0;
	char data[65535] = {0}, threadFile[MAX_FILE_NAME]={0};
        

	//check if we have got all the required parameters.
	if (argc != 4) {
		//User is playing with us, lets talk to him :)
                printf("Help: ./DNSClient.o \"number-of-thread\" \"input-FQDN-filename\" \"output-filename\"\n");
                goto error;
        }

	//get the number of thread, and check if it is under limit.
        numThread = (int) atoi(argv[1]);
        if (numThread > 100) {
                printf("Warning: Maximum 100 threads are allowed. Number reduced to 100\n");
		numThread = 100;
        }
	//get the input fqdn file name.
	if (strlen(argv[2]) > MAX_FILE_NAME) {
		printf("Error: File name legth exceeded the maximum allowed limit %d:%d\n", strlen(argv[2]), MAX_FILE_NAME);
		goto error;
	}
	//get the output file name.
	if (strlen(argv[3]) > MAX_FILE_NAME) {
                printf("Error: File name legth exceeded the maximum allowed limit %d:%d\n", strlen(argv[3]), MAX_FILE_NAME);
                goto error;
        }


	sprintf(dnsFileName, "%s", argv[2]);
	//get the stat of the input file, to get the size.
        stat(argv[2], &stStat);
        fqdnFileSize = stStat.st_size;
        if (fqdnFileSize == 0) {
                printf("Error: Empty file\n");
                goto error;
        }

	//if number of thread are more than number of bytes available in input FQDN file.
	//then reinit the number of threads to one less than the size. so that each thread will
	//get atleast 1 byte to read.
	if (numThread >= fqdnFileSize)
		numThread = fqdnFileSize-1;

	//calculate each thread's segment.
        eachThreadShare = fqdnFileSize/numThread;
	//add one more thread of the remainder of the file.
        numThread += (fqdnFileSize%numThread)? 1:0;

	if (DNS_CLIENT_DEBUG) {
		printf("DNS_DEBUG: number of threads:%d, thread share %d bytes\n", numThread, eachThreadShare); 
	}
	//init base to fire the rocket.
        dns_init_server();
	dns_cache_init();
	pthread_mutex_init(&dns_mutex, NULL);

	//fire the threads.
        for(threadCount=0; threadCount<numThread; threadCount++) {
                rval = pthread_create(&allThread[threadCount], NULL, (void *) &DNSClientThreadFucntion, (void*)(threadCount+1));
                if (rval) {
                        printf("Error: Could not spawn all the thread(s). Return value %d\n", rval);
                        goto error;
                }
        }
	//join, wait till all the thead finishes.
        for(threadCount=0; threadCount<numThread; threadCount++) {
                rval = pthread_join(allThread[threadCount], NULL);
                if (rval) {
                        printf("Error: Could not join all the thread(s). Return value %d\n", rval);
                        goto error;
                }

        }
        printf("\nDone.\n");
	dns_cache_dump();
	//create output file.
	fOutFile = fopen(argv[3], "w+");
	if (NULL==fOutFile) {
		printf("Error:Not able to open %s\n", argv[3]);
		goto error;
	}
	
	//lets go through all the thread's file and read them write in one single output file.
	for(threadCount=1; threadCount<=numThread; threadCount++) {	
		//create thread's file name and open it, 
		sprintf(threadFile, "thread%d", threadCount);
		stat(threadFile, &stStat);
		fThreadFile = fopen(threadFile, "r");
		if (DNS_CLIENT_DEBUG) {
                	printf("DNS_DEBUG: Reading %s\n", threadFile);
        	}
		//start reading and witing chenk by chenk.
		do{
			//first read big chenks of 65000 bytes
			if(stStat.st_size > 65000) {
				fread(data, 65000, 1 , fThreadFile);
				fwrite(data,65000, 1,  fOutFile);
				stStat.st_size -= 65000;
			} else {
				//read the last leftover chunk which is less than 65000
				fread(data, stStat.st_size, 1 , fThreadFile);
                                fwrite(data,stStat.st_size, 1,  fOutFile);
                                stStat.st_size -= stStat.st_size;
			}
		}while(stStat.st_size);

		//flush all the file pointer and close it.
		fflush(fThreadFile);
		fclose(fThreadFile);		
		//unlink the inode of thread file, so that it would be deleted form file system.
		unlink(threadFile);
	}	
	
	//This function has only one return point, i.e. this section.
	error:
	pthread_mutex_destroy(&dns_mutex);
	dns_cache_destroy();
	if (fOutFile){
		fflush(fOutFile);
		fclose(fOutFile);
	}
	return 1;
}


/*
Description: 	This function is main entry function for each thread. 
		This function is responsible for reading each thread's section.
		Send out the DSN Query, process DNS Response and write in each thread's local file.
Input: 		Thread ID;
OutPut: 	None
*/
void DNSClientThreadFucntion(void *threadID)
{
        char buffer[65535] = {0}, threadFile[MAX_FILE_NAME]={0};
        int rVal = 0, fdSocket = 0, tmpCount=0, dnsMsgLen = 0;
	int  rcvLen=0, readLen=0, threadId = (int)threadID;
        struct sockaddr_in destSocket = {0};
	char *tmpPtr = NULL, fqdnName[100] = {0};	
	FILE *fThreadFile=NULL, *fpPtr = NULL;
	unsigned short ttl=0;
	struct in_addr ip_addr = {0};
	struct timeval tv = {0};

	// Marking the territory

	//Each thread will read from their own territory or section of the file 
	//so that we will not need to acquire lock for each read. 
	//If we have too many thread than we might slightly interfere each other territory.
	//but that will be taken care by cache.
	
	//go to the start memory address of this thread.
	//first thread will start from 0th URL, i.e. from the very start.
	fpPtr = fopen(dnsFileName, "r+");
	fseek(fpPtr, ((threadId-1)*eachThreadShare), SEEK_SET);
	//for rest of threads, we will set the file pointer accordingly.
	//i.e. each thread will start next logical url and read till last logical/complete url
	if ((threadId-1)) {
		//if our section starts firn nid of URL then lets move to next URL
		// because last thread would read this complete URL
		while (  (rVal != EOF) && (rVal != '\n')) {
			rVal = fgetc(fpPtr);
		}
		if (rVal == EOF) 
		{
			//if we have reached till last of file, it means 
			//I am a poor guy and dont have anything to read.
			//lets say good bye to the world.
			goto error;
		}
	}
	
	//create local thread file.
	sprintf(threadFile, "thread%d", threadId);
	fThreadFile = fopen(threadFile, "a+");
	if (fThreadFile == NULL) {
		printf("Error: Thread %d not able to create its own file\n", threadId-1);
		return ;
	}
	//end

        fdSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	//set the timeout value of 3 second to each receiving socket.
	if (setsockopt(fdSocket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		printf("Warning: Could not set the tiemout for receiving socket\n");
	}

	//Lets take a walk in our territory, and send out the DNS Req for each FQDN
	while(readLen < eachThreadShare) {
		//setting a UDP socket to DNS port / 53.
                destSocket.sin_family=AF_INET;
                destSocket.sin_port=htons(DNS_PORT);
                destSocket.sin_addr.s_addr=inet_addr(dnsServer[tmpCount%MAX_DNS_SERVER]);

		//Lets read the FQDN, it should be less than 100 bytes
		tmpCount =0;
		while(tmpCount<100) {
			rVal = (char)fgetc(fpPtr);
			if ( (rVal == '\n') || (rVal == EOF)) {
				break;
			}
			fqdnName[tmpCount] = (char)rVal;
			readLen++;
			tmpCount++;
		}
		fqdnName[tmpCount] = '\0';
		
		// if we could not read any FQDN
		if (!tmpCount) goto error;

		if (DNS_CLIENT_DEBUG) {
                        printf("DNS_DEBUG(%d): Trying to send the DNS reqeust for %s\n", threadId, fqdnName);
                }
		
		// Check the cache, if anyother thread had encountered the same FQDN earlier
		//if yes, cache will refresh the node else cache will create a new entry for the node.
		pthread_mutex_lock(&dns_mutex);
		rVal = dns_cache_check(fqdnName, strlen(fqdnName));
		pthread_mutex_unlock(&dns_mutex);
		//if rVal is 1, then it was a cache hit.
		if (rVal > 0) continue;
	
		//in case of cache miss, send out the DNS Request

		//construct the DNS request
                dnsMsgLen = dns_construct_query(buffer, fqdnName, (int )(threadID));

		//send the DNS request 
                rVal = sendto(fdSocket, (void*)buffer, dnsMsgLen, 0,
                                (struct sockaddr *)&destSocket, sizeof(destSocket));
		if (DNS_CLIENT_DEBUG) {
			printf("DNS_DEBUG(%d):sending DNS req for %s\n", threadId, fqdnName);

		}
                if (rVal < 0) {
			//oops :(, lets try with another URL
                        printf("Error: While sending the DNS Request to %d:%s\n",tmpCount%MAX_DNS_SERVER,  dnsServer[tmpCount%MAX_DNS_SERVER]);
			memset(&destSocket, 0, sizeof(struct sockaddr_in));
                        continue;
                }
                rcvLen = sizeof (struct sockaddr);
		rVal = recvfrom(fdSocket, (void*)buffer, 65535, 0, (struct sockaddr *)&destSocket, &rcvLen);
		if(rVal <0) {
			//oops :(, lets try with another URL
			memset(&destSocket, 0, sizeof(struct sockaddr_in));
			continue;
		}
		rVal = dns_parse_response(buffer, rVal, &(ip_addr.s_addr), &ttl);	
                if (rVal<0) {
			//we fail to get the answer from DNS Server
			if (DNS_CLIENT_DEBUG) {
				printf("DNS_DEBUG(%d): Fail to get the answer from DNS Server for %s\n", threadId, fqdnName);
			}
			fprintf(fThreadFile, "%s \t\t\t Not available\n",fqdnName); 
                }else {
			//wo got the reply.
			fprintf(fThreadFile, "%s \t\t\t %s\n",fqdnName, inet_ntoa(ip_addr));
		}
		//lets bug another DNS server.
                memset(&destSocket, 0, sizeof(struct sockaddr_in));
        }
	error:
	//sanitize everything, before going back to almighty "main"
	if (fThreadFile) {
		fflush(fThreadFile);
		fclose(fThreadFile);
	}
	if (fpPtr){
		fflush(fpPtr);
		fclose(fpPtr);
	}
	close(fdSocket);
	//aamen :)
}

