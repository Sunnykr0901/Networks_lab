#include <stdio.h>  
#include <string.h>   //strlen  
#include <stdlib.h> 
#include<ctype.h>
#include <errno.h> 
#include <unistd.h> //close 
#include <arpa/inet.h> //close 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 
	
#define TRUE 1 
#define FALSE 0 
#define MAXLINE 1024
int PORT=0; 
int UDP_PORT_INITIAL=8000;
	
int main(int argc , char *argv[]) 
{ 
	//Checking Command line argument
	if(argc<2)
	{
		perror("PORT MISSING\n");
		return 0;
	}
	//Reading PORT
	for(int i=0;argv[1][i]!='\0';i++)
	{
		if(!isdigit(argv[1][i]))	//Invalid PORT
		{
			perror("INVALID PORT\n");
			return 0;
		}
		PORT*=10;
		PORT+=(argv[1][i]-48);
	}

	
	//UDP Parameters
	int udp_client_socket[30];
	int sockfd;
	struct sockaddr_in servaddr, cliaddr; 

	//TCP Parameters
	int opt = TRUE; 
	int master_socket , addrlen , new_socket , client_socket[30] , 
		max_clients = 30 , activity, i , valread , sd; 
	int max_sd; 
	struct sockaddr_in address; 
		
	//Data buffer of 1K 
	char buffer[1025]; 
		
	//set of socket descriptors 
	fd_set readfds; 
	
	
	//initialise all client_socket[] and udp_client_socket[] to 0 so not checked 
	for (i = 0; i < max_clients; i++) 
	{ 
		client_socket[i] = 0; 
		udp_client_socket[i]=0;
	} 
		
	//create a master socket TCP
	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
	{ 
		perror("master socket creation failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	//set master socket to allow multiple connections
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) 
	{ 
		perror("setsockopt"); 
		exit(EXIT_FAILURE); 
	} 
	
	//type of socket created 
	address.sin_family = AF_INET; 	//ipv4
	address.sin_addr.s_addr = INADDR_ANY; 	//Accept incoming connection on all ips assigned to the machine
	address.sin_port = htons( PORT ); 		//Custom Port
		
	//bind the socket to localhost port provided by user
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
	{ 
		perror("master socket bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	printf("Master Socket listening on port %d \n", PORT); 
	printf("Maximum concurrent connections supported: %d\n", max_clients); 
		
	//try to specify maximum of 3 pending connections for the master socket 
	if (listen(master_socket, 3) < 0) 
	{ 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	} 
		
	//accept the incoming connection 
	addrlen = sizeof(address); 
	puts("Waiting for connections ..."); 
		
	while(TRUE) 
	{ 
		//clear the socket set 
		FD_ZERO(&readfds); 
	
		//add master socket to set 
		FD_SET(master_socket, &readfds); 
		max_sd = master_socket; 
			
		//add child sockets to set 
		for ( i = 0 ; i < max_clients ; i++) 
		{ 
			//socket descriptor 
			sd = client_socket[i]; 
				
			//if valid socket descriptor then add to read list 
			if(sd > 0) {
				FD_SET( sd , &readfds);
				//printf("Adding TCP: %d:%d\n",i,sd);
			}

			//highest file descriptor number, need it for the select function 
			if(sd > max_sd) 
				max_sd = sd;  

			//socket descriptor 
			sd = udp_client_socket[i]; 

			//if valid UDP socket descriptor then add to read list 
			if(sd > 0){
				FD_SET( sd , &readfds);
				//printf("Adding UDP: %d:%d\n",i,sd);
			}
				
			//highest file descriptor number, need it for the select function 
			if(sd > max_sd) 
				max_sd = sd; 
		} 
	
		//wait for an activity on one of the sockets , timeout is NULL , 
		//so wait indefinitely 
		activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL); 
	
		if ((activity < 0) && (errno!=EINTR)) 
		{ 
			printf("select error \n");
			if(errno==EBADF)
				printf("1\n"); 
			else if(errno==EINVAL)
				printf("2\n");
			else if(errno==ENOMEM)
				printf("3\n");
		} 
			
		//If something happened on the master socket , 
		//then its an incoming connection 
		if (FD_ISSET(master_socket, &readfds)) 
		{ 
			if ((new_socket = accept(master_socket, 
					(struct sockaddr *)&address, (socklen_t*)&addrlen))<0) 
			{ 
				perror("accept"); 
				exit(EXIT_FAILURE); 
			} 
			
			//inform user of socket number - used in send and receive commands 
			printf("New connection , socket fd is %d | ip is : %s | port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs 
				(address.sin_port)); 
				
			//add new socket to array of sockets 
			for (i = 0; i < max_clients; i++) 
			{ 
				//if position is empty 
				if( client_socket[i] == 0 ) 
				{ 
					client_socket[i] = new_socket; 
					printf("Adding to list of sockets as %d\n" , i); 
						
					break; 
				} 
			} 
		} 
			
		//else its some IO operation on some other socket 
		for (i = 0; i < max_clients; i++) 
		{ 
			sd = client_socket[i]; 
				
			if (FD_ISSET( sd , &readfds)) 
			{ 
				//Check if it was for closing , and also read the 
				//incoming message 
				if ((valread = read( sd , buffer, 1024)) == 0) 
				{ 
					//Somebody disconnected , get his details and print 
					getpeername(sd , (struct sockaddr*)&address , \ 
						(socklen_t*)&addrlen); 
					printf("Host disconnected IP: %s | PORT: %d \n" , 
						inet_ntoa(address.sin_addr) , ntohs(address.sin_port)); 
						
					//Close the socket and mark as 0 in list for reuse 
					close( sd ); 
					client_socket[i] = 0; 
				} 
					
				//Some message has arrived
				else
				{ 
					//set the string terminating NULL byte on the end 
					//of the data read 
					buffer[valread] = '\0';
					//UDP PORT Request 
					if(valread>0&&buffer[0]=='1')	
					{getpeername(sd , (struct sockaddr*)&address , \ 
						(socklen_t*)&addrlen); 
					printf("HOST: IP: %s | PORT: %d asks UDP PORT\n" ,inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

						// Creating socket file descriptor for UDP
					    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
					        perror("UDP socket creation failed"); 
					        exit(EXIT_FAILURE); 
					    } 

					    memset(&servaddr, 0, sizeof(servaddr)); 
					    memset(&cliaddr, 0, sizeof(cliaddr)); 
					      
					    // Filling server information 
					    servaddr.sin_family    = AF_INET; // IPv4 
					    servaddr.sin_addr.s_addr = INADDR_ANY; 
					    int port;
					    for(int i=0;i<max_clients;i++)
					    	if(udp_client_socket[i]==0)
					    	{
					    		port=UDP_PORT_INITIAL+i;
					    		udp_client_socket[i]=sockfd;
					    		printf("PORT PICKED: %d\n",port);
					    		break;
					    	}
					    servaddr.sin_port = htons(port);

					    // Bind the socket with the server address 
					    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
					            sizeof(servaddr)) < 0 ) 
					    { 
					        perror("bind failed"); 
					        exit(EXIT_FAILURE); 
					    } 
					    buffer[0]='2';
					    buffer[1]='4';
					    buffer[2]='$';
					    int x=port%10;
					    port/=10;
					    buffer[6]=x+48;
					    x=port%10;
					    port/=10;
					    buffer[5]=x+48;
					    x=port%10;
					    port/=10;
					    buffer[4]=x+48;
					    x=port%10;
					    port/=10;
					    buffer[3]=x+48;
					    buffer[7]='\0';
					    send(sd , buffer , strlen(buffer) , 0 ); 
					}
					
				} 
			} 
		} 
	
		//else its some IO operation on some UDP Socket
		for (int i2 = 0; i2 < max_clients; i2++) 
		{ 
			sd = udp_client_socket[i2]; 
			
			if (FD_ISSET( sd , &readfds)) 
			{ 
				//incoming message 
				int len,n;
				len = sizeof(cliaddr);
				n = recvfrom(sd, (char *)buffer, MAXLINE,  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
                &len);
				
					buffer[n] = '\0'; 
					if(n>0)
					{
						
					printf("Host: IP: %s | PORT: %d | sends message: %s\n" ,inet_ntoa(cliaddr.sin_addr) , ntohs(cliaddr.sin_port),buffer);
					if(buffer[0]=='3')
					{
						int x=1;
						int len=0;
						while(buffer[x]!='$')
						{
							len*=10;
							len+=(buffer[x]-48);
							x++;
						}
						char msg[1024];
						int len2=len;
						int temp=x+1;
						int i=0;
						while(len2>0&&buffer[temp]!='\0')
						{
							msg[i++]=buffer[temp++];
							len2--;
						}
						msg[i]='\0';
						printf("TYPE-3 | MESSAGE LENGTH:%d | MESSAGE:%s\n",len,msg);
						printf("Sending TYPE-4 message\n");
						if(!sendto(sd, (const char *)"4\0", 2, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, sizeof(cliaddr))){
            				perror("type 4 message failed"); 
					        exit(EXIT_FAILURE); 
						}
					//Close the socket and mark as 0 in list for reuse 
					printf("Closing UDP Socket\n");
					close( sd ); 
					udp_client_socket[i2] = 0; 
					}
						
					}
					
				

			} 
		} 
	} 
		
	return 0; 
} 
