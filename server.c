/*
 * File : server.c
 * Author : Amine Amanzou
 *
 * Created : 4th January 2013
 *
 * Under GNU Licence
 */

#include <stdio.h>
#include <stdlib.h>

// Time function, sockets, htons... file stat
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

// File function and bzero
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include "ikcp.h"

/* Taille du buffer utilise pour envoyer le fichier
 * en plusieurs blocs
 */
#define BUFFERT 1400

/* Commande pou génerer un fichier de test
 * dd if=/dev/urandom of=fichier count=8
 */

/* Declaration des fonctions*/
int duration (struct timeval *start,struct timeval *stop, struct timeval *delta);
int create_server_socket (int port);

struct sockaddr_in sock_serv,clt;
struct kcp_context {
	struct sockaddr *addr;
	int socklen;
	int fd;
};
int kcp_op(const char *buf, int len, ikcpcb *kcp,void *user)  {
    struct kcp_context *ctx = (struct kcp_context *)user;
		return sendto(ctx -> fd,buf,len,0,ctx->addr,ctx -> socklen);
}

int main (int argc, char**argv){
    //Descripteur
	int fd, sfd;
    
	char buf[BUFFERT];
	off_t count=0, n,m; // long type
	char filename[256];
  unsigned int l=sizeof(struct sockaddr_in);
	
    // Variable pour la date
	time_t intps;
	time_t st,et;
	struct tm* tmi;
    
	if (argc != 2){
		printf("Error usage : %s <port_serv>\n",argv[0]);
		return EXIT_FAILURE;
	}
    
  sfd = create_server_socket(atoi(argv[1]));
  int s = 4096 * 1024;
  if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &s, sizeof(s)) == -1) {
      printf("Error setting buffer size");
  }
  if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &s, sizeof(s)) == -1) {
      printf("Error setting buffer size");
  }


	intps = time(NULL);
  tmi = localtime(&intps);
	bzero(filename,256);
	sprintf(filename,"clt.%d.%d.%d.%d.%d.%d",tmi->tm_mday,tmi->tm_mon+1,1900+tmi->tm_year,tmi->tm_hour,tmi->tm_min,tmi->tm_sec);
	printf("Creating the output file : %s\n",filename);
    
	//ouverture du fichier
	if((fd=open(filename,O_CREAT|O_WRONLY|O_TRUNC,0600))==-1){
		perror("open fail");
		return EXIT_FAILURE;
	}
    
	//preparation de l'envoie
	bzero(&buf,BUFFERT);
  n=recvfrom(sfd,&buf,BUFFERT,0,(struct sockaddr *)&clt,&l);
  struct kcp_context *ctx = malloc(sizeof(struct kcp_context));
  int finished = 0;
  char *t;
  char t2 = {0x22,0x00,0x0d,0xf4,0x35,0x31,0x02,0x71,0xa7,0x31,0x88,0x80,0x00};
  ctx -> addr = (struct sockaddr *)&clt;
  ctx -> socklen = l;
  ctx -> fd = sfd;
  ikcpcb *kcpobj = ikcp_create(123,ctx);
  ikcp_setoutput(kcpobj,kcp_op);
  ikcp_nodelay(kcpobj,1,20,2,1);
  ikcp_wndsize(kcpobj,32,1024);
  ikcp_update(kcpobj,iclock());
  ikcp_input(kcpobj,buf,n);
	while(n){
    ikcp_update(kcpobj,iclock());
    m = ikcp_recv(kcpobj,buf,BUFFERT);
    while(m > 0)
    {
        //printf("%lld of data received \n",m);
        if(m==-1){
            perror("read fails");
            return EXIT_FAILURE;
        }
        else if(m == 13)
        {
            t = (char *)buf;
            if(strcmp(t,&t2))
            {
                finished = 1;
            }
        }
        if(count == 0 &&  m > 0)
        {
            st = time(NULL);
        }
        count+=m;
        write(fd,buf,m);
        m = ikcp_recv(kcpobj,buf,BUFFERT);
    }
    bzero(buf,BUFFERT);
    n=recvfrom(sfd,&buf,BUFFERT,MSG_DONTWAIT,(struct sockaddr *)&clt,&l);
    if(n > 0)
    {
        ikcp_input(kcpobj,buf,n);
    }
    if(finished)
    {
        break;
    }
    usleep(20);
	}
  int ocount = 0;
  int last = 0;
  last = ikcp_waitsnd(kcpobj);
  while(1)
  {
      int now = ikcp_waitsnd(kcpobj);
      if( last != now || ocount < 50)
      {
          ikcp_update(kcpobj,iclock());
          n=recvfrom(sfd,&buf,BUFFERT,MSG_DONTWAIT,(struct sockaddr *)&sock_serv,&l);
          //printf("Window %d,last %d\n",now,last);
          if(n > 0)
          {
              ikcp_input(kcpobj,buf,n);
          }
          if(last == now) 
              ocount+=1; 
          else
          {
              last = now;
              count = 0;
          }
          usleep(20);
      }
      else {
          break;
      }
  }

  et = time(NULL);
	printf("Nombre d'octets transférés : %lld ,speed is %lld K /s \n",count,count / 1024 / (et - st));
  ikcp_release(kcpobj);
    
  close(sfd);
  close(fd);
	return EXIT_SUCCESS;
}

/* Fonction permettant le calcul de la durée de l'envoie */
int duration (struct timeval *start,struct timeval *stop,struct timeval *delta)
{
    suseconds_t microstart, microstop, microdelta;
    
    microstart = (suseconds_t) (100000*(start->tv_sec))+ start->tv_usec;
    microstop = (suseconds_t) (100000*(stop->tv_sec))+ stop->tv_usec;
    microdelta = microstop - microstart;
    
    delta->tv_usec = microdelta%100000;
    delta->tv_sec = (time_t)(microdelta/100000);
    
    if((*delta).tv_sec < 0 || (*delta).tv_usec < 0)
        return -1;
    else
        return 0;
}

/* Fonction permettant la creation d'un socket et son attachement au systeme
 * Renvoie un descripteur de fichier dans la table de descripteur du processus
 * bind permet sa definition aupres du systeme
 */
int create_server_socket (int port){
    int l;
	int sfd;
    
	sfd = socket(AF_INET,SOCK_DGRAM,0);
	if (sfd == -1){
        perror("socket fail");
        return EXIT_FAILURE;
	}
    
    //preparation de l'adresse de la socket destination
	l=sizeof(struct sockaddr_in);
	bzero(&sock_serv,l);
	
	sock_serv.sin_family=AF_INET;
	sock_serv.sin_port=htons(port);
	sock_serv.sin_addr.s_addr=htonl(INADDR_ANY);
    
	//Affecter une identité au socket
	if(bind(sfd,(struct sockaddr*)&sock_serv,l)==-1){
		perror("bind fail");
		return EXIT_FAILURE;
	}
    
    
    return sfd;
}
