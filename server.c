
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_SOCKETS 10000
#define STDOUT 1
#define STDERR 2
#define DELAY 10

struct addrinfo* serveraddr;
int ngroups;
int got_reply=1;
int groupports[MAX_SOCKETS];
char *groupnames[MAX_SOCKETS];


char* itoa(int val)
{
    static char buf[32] = {0};
    int i=30;
    for(; val && i ; --i, val /= 10)
        buf[i] = "0123456789abcdef"[val % 10];
    return &buf[i+1];
}

/*void signalhandler(int signum)
{
    int broadcastrecvfd,i,flg;
    struct sockaddr_in bcrecvaddr;
    bcrecvaddr.sin_family = AF_INET;
    bcrecvaddr.sin_addr.s_addr  = htonl(INADDR_ANY);
    for(i=1;i<=ngroups;i++)
        if( groupnames[i]!=NULL && strlen(groupnames[i])>0 )
        {
        	if( signum==SIGURG )
        	{
                broadcastrecvfd = socket(AF_INET, SOCK_DGRAM, 0);

                flg=1;
                if(setsockopt(broadcastrecvfd, SOL_SOCKET, SO_REUSEADDR, &flg, sizeof(flg)) <0)
            		write(STDERR,"OPT REUSEADDR ERROR\n",20);
                flg=1;
                if(setsockopt(broadcastrecvfd, SOL_SOCKET, SO_REUSEPORT, &flg, sizeof(flg)) <0)
            		write(STDERR,"OPT REUSEPORT ERROR\n",20);
                bcrecvaddr.sin_port = htons(groupports[i]);

                char c;
        		recvfrom(broadcastrecvfd, &c, sizeof(c), MSG_OOB, NULL, 0);
        		got_reply = ( c == '~' );
        		close(broadcastrecvfd);
        	}
        	else if ( signum == SIGALRM )
            {
        		if ( got_reply )
        		{
        			alarm(DELAY);
        			got_reply = 0;
        		}
        		else
                    groupnames[i]=NULL;
            }
        }
}*/


int start_listen(char* port)
{
    struct addrinfo hints;
    int serverfd,tmp;
    int yes=1;

    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype=SOCK_STREAM;
    if( getaddrinfo(NULL,port,&hints,&serveraddr)!=0 )
        return -1;
    serverfd=socket(AF_INET,SOCK_STREAM,0);
    if( serverfd < 0 )
        return -1;
    yes=1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if( bind(serverfd,serveraddr->ai_addr,serveraddr->ai_addrlen) < 0 )
        return -1;
    if( listen(serverfd,10)== -1 )
        return -1;
    return serverfd;
}

void handle_private_chat_request(char* buf,int client_fd,int max_fd,char* request,char* *username,int *chatcontact, fd_set* users)
{
    int destfd,i,flg;
    if(strlen(request)==1)
        destfd=0;
    else
        destfd=atoi(strtok(request,"@"));
    if(destfd<=0 || destfd>max_fd)
    {
        strcat(buf,"Users List:(Type @ And ID To Start Private Chat)\n");
        for(i=4;i<=max_fd;i++)
            if(i!=client_fd && FD_ISSET(i,users))
            {
                strcat(buf,"@");strcat(buf,itoa(i));strcat(buf,": ");
                strcat(buf,username[i]);strcat(buf,"\n");
            }
    }
    else
    {
        if(strlen(username[destfd])==0 || destfd==client_fd || !FD_ISSET(destfd,users))
            strcpy(buf,"User Not Found!\n");
        else if(chatcontact[destfd]==0)
            strcpy(buf,"User Is In A Group Chat,Try Again Later\n");
        else if(chatcontact[destfd]>0)
            strcpy(buf,"User Is In A Private Chat,Try Again Later\n");
        else
        {
            write(STDOUT,"users on sockets ",17);
            write(STDOUT, itoa(client_fd), strlen(itoa(client_fd)));
            write(STDOUT," & ",3);
            write(STDOUT, itoa(destfd), strlen(itoa(destfd)));
            write(STDOUT, "are in a private chat now.\n",27);
            chatcontact[client_fd]=destfd;
            chatcontact[destfd]=client_fd;
            strcpy(buf,"You Are In Private Chat With '");
            strcat(buf,username[client_fd]);
            strcat(buf,"'(Type $exit_chat$ To Quit.)\n");
            send(destfd,buf,strlen(buf),0);
            strcpy(buf,"You Are In Private Chat With '");
            strcat(buf,username[destfd]);
            strcat(buf,"'(Type $exit_chat$ To Quit.)\n");
        }
    }
}

void handle_group_chat_request(char* buf,int client_fd,char* request,int *chatcontact)
{
    int groupid,i,flg,groupport;

    if(strlen(request)==1)
        groupid=0;
    else
        groupid=atoi(strtok(request,"#"));
    if( strlen(request)==1 || groupid>ngroups)
    {
        strcpy(buf,"Groups List:(Type # And ID To Join Group)\n");
        for(i=1;i<=ngroups;i++)
            if( strlen(groupnames[i])>0 )
            {
                strcat(buf,"#");strcat(buf,itoa(i));strcat(buf,": ");
                strcat(buf,groupnames[i]);strcat(buf,"\n");
            }
    }
    else
    {
        if(groupid<=0)//create
        {
            write(STDOUT,"user on socket ",15);
            write(STDOUT, itoa(client_fd), strlen(itoa(client_fd)));
            write(STDOUT, " created a new group\n",21);
            ngroups++;
            groupport=30000+ngroups;
            groupports[ngroups]=groupport;
            chatcontact[client_fd]=0;
            strcpy(buf,"#newgroupport=");
            strcat(buf,itoa(groupport));
            strcat(buf,"\n");
            groupnames[ngroups]=malloc(64);
            strcpy(groupnames[ngroups],strtok(request,"#"));
        }
        else if(groupid>0 && strlen(groupnames[groupid])==0 )
            strcpy(buf,"Group Not Found!\n");
        else//join
        {
            write(STDOUT,"user on socket ",15);
            write(STDOUT, itoa(client_fd), strlen(itoa(client_fd)));
            write(STDOUT, " joined a new group\n",20);
            chatcontact[client_fd]=0;
            strcpy(buf,"#groupport=");
            strcat(buf,itoa(groupports[groupid]));
            strcat(buf,"\n");
        }
    }
}

int main(int argc, char* argv[])
{
    int serverfd,max_fd,i,flg,pvflg;

    int client_fd;
    struct sockaddr_in* clientaddr,broadcastaddr;
    char clientip[INET_ADDRSTRLEN];
    socklen_t clientaddrlen;

    fd_set allfds,ready_fds,users,groups;

    char request[256];
    int nbytes;

    char *username[MAX_SOCKETS];
    int chatcontact[MAX_SOCKETS];

    int groupsignal[MAX_SOCKETS];
    memset(groupsignal, 0, sizeof(groupsignal));

    char* tmp;
    char buf[512],sbuf[8],bufmsg[256];
    char firstpage[256]="Type # To See Available Groups\nType #GROUPNAME To Create A New Group\nType @ To See Available Users\nType exit To Quit\n";

    serverfd=start_listen(argv[1]);
    flg=1;
    flg=fcntl(serverfd,F_GETFL);
    flg=(flg|O_NONBLOCK);
    fcntl(serverfd,F_SETFL,flg);

    FD_ZERO(&users);
    FD_ZERO(&ready_fds);
    FD_SET(serverfd,&users);
    max_fd=serverfd;

    tmp=malloc(64);
    for(i=0;i<=max_fd;i++)
    {
        username[i]=malloc(64);
        chatcontact[i]=-1;
    }

/*    struct sigaction groupsmonitor;
	memset (&groupsmonitor, '\0', sizeof(groupsmonitor));
    groupsmonitor.sa_handler = signalhandler;
	groupsmonitor.sa_flags = SA_RESTART;
	sigaction(SIGURG, &groupsmonitor, 0);
	sigaction(SIGALRM, &groupsmonitor, 0);*/

    while(1)
    {
        ready_fds=users;
        if( select(max_fd+1,&ready_fds,NULL,NULL,NULL) <= 0 )
            break;
        for(i=0;i<=max_fd;i++)
        {
            if(i==serverfd && FD_ISSET(i,&ready_fds)) // new conncetion request
            {
                clientaddrlen=sizeof(clientaddr);
                client_fd=accept(serverfd, &clientaddr, &clientaddrlen);
                write(STDOUT,"new connection on socket",24);
                write(STDOUT, itoa(client_fd), strlen(itoa(client_fd)));
                write(STDOUT, ".\n",2);
                flg=1;
                if(setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &flg, sizeof(flg)) <0)
            		write(STDERR,"OPT REUSEADDR ERROR\n",20);
                flg=1;
                if(setsockopt(client_fd, SOL_SOCKET, SO_REUSEPORT, &flg, sizeof(flg)) <0)
            		write(STDERR,"OPT REUSEPORT ERROR\n",20);

                if(client_fd<0)
                    write(STDERR,"ACCEPT ERROR\n",13);
                FD_SET(client_fd,&users);
                if(client_fd>max_fd)
                {
                    username[client_fd]=malloc(64);
                    chatcontact[client_fd]=-1;
                    max_fd=client_fd;
                }

                send(client_fd,"Please Type Your Username:",26,0);
            }
            else if( FD_ISSET(i,&ready_fds) )//handling user commands and private chats
            {
                rehandle:
                client_fd=i;
                memset(request,'\0',128);
                nbytes=recv(client_fd,request,sizeof(request),0);
                if( nbytes<=0 ) // user leaving
                {
                    if(nbytes==0)
                    {
                        write(STDOUT,"connection on socket ",21);
                        write(STDOUT, itoa(client_fd), strlen(itoa(client_fd)));
                        write(STDOUT, " closed.\n",9);
                        username[client_fd]=NULL;
                        username[client_fd]=malloc(64);
                        chatcontact[client_fd]=-1;
                        close(client_fd);
                        FD_CLR(client_fd,&users);
                    }
                //    else
                //        write(STDERR,"USER MSG ERROR\n",15);
                }
                else // user new request
                {
                    request[strlen(request)-1]=0;
                    memset(buf,'\0',128);
                    if(strlen(username[client_fd])==0) // setting username
                    {
                        strcpy(username[client_fd],request);
                        strcpy(buf,"Welcome ");
                        strcat(buf,username[client_fd]);strcat(buf,"\n");
                        strcat(buf,firstpage);
                    }
                    else if(chatcontact[client_fd]<=-1)
                    {
                        if(request[0]=='#')
                            handle_group_chat_request(buf,client_fd,request,chatcontact);
                        else if(request[0]=='@')
                            handle_private_chat_request(buf,client_fd,max_fd,request,username,chatcontact,&users);
                    }
                    else
                    {
                        if(chatcontact[client_fd]>0 && strcmp(request,"$exit_chat$")==0)
                        {
                            write(STDOUT,"users on sockets ",17);
                            write(STDOUT, itoa(client_fd), strlen(itoa(client_fd)));
                            write(STDOUT," & ",3);
                            write(STDOUT, itoa(chatcontact[client_fd]), strlen(itoa(chatcontact[client_fd])));
                            write(STDOUT, "finished their private chat.\n",29);
                            strcpy(bufmsg,username[client_fd]);
                            strcat(bufmsg," LEFT THE CHAT\n");
                            strcat(bufmsg,firstpage);
                            strcpy(buf,"YOU LEFT THE CHAT\n");
                            strcat(buf,firstpage);
                            send(chatcontact[client_fd],bufmsg,strlen(bufmsg),0);
                            chatcontact[ chatcontact[client_fd] ]=-1;
                            chatcontact[client_fd]=-1;
                        }
                        else if(chatcontact[client_fd]>0)
                        {
                            strcpy(bufmsg,username[client_fd]);
                            strcat(bufmsg,":");
                            strcat(bufmsg,request);strcat(bufmsg,"\n");
                            send(chatcontact[client_fd],bufmsg,strlen(bufmsg),0);
                        }
                        else if(chatcontact[client_fd]==0)
                        {
                            write(STDOUT,"user on socket ",15);
                            write(STDOUT, itoa(client_fd), strlen(itoa(client_fd)));
                            write(STDOUT, " left the group\n",16);
                            chatcontact[client_fd]=-1;
                            goto rehandle;
                        }

                    }
                    if(strlen(buf)>0)
                        send(client_fd,buf,strlen(buf),0);
                }
            }

        }
    }
}
