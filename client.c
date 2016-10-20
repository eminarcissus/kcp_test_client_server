/*
 * File : client.c
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
#define BUFFERT 1350

/* Commande pou génerer un fichier de test
 * dd if=/dev/urandom of=fichier count=8
 */

/* Declaration des fonctions*/
int duration (struct timeval *start,struct timeval *stop, struct timeval *delta);
int create_client_socket (int port, char* ipaddr);

struct sockaddr_in sock_serv;
struct kcp_context {
	struct sockaddr *addr;
	int socklen;
	int fd;
};

int kcp_op(const char *buf, int len, ikcpcb *kcp,void *user)  {
    struct kcp_context *ctx = (struct kcp_context *)user;
    //printf("Send %d bytes\n",len);
		return sendto(ctx -> fd,buf,len,0,ctx->addr,ctx -> socklen);
}

int main (int argc, char**argv){
	struct timeval start, stop, delta;
    int sfd,fd;
    char buf[BUFFERT];
    char buf2[BUFFERT];
    off_t count=0, m,sz;//long
	long int n;
  int x;
    int l=sizeof(struct sockaddr_in);
	struct stat buffer;
  ikcpcb *kcpobj;
    
	if (argc != 4){
		printf("Error usage : %s <ip_serv> <port_serv> <filename>\n",argv[0]);
		return EXIT_FAILURE;
	}
    
    sfd=create_client_socket(atoi(argv[2]), argv[1]);
  struct kcp_context *ctx = malloc(sizeof(struct kcp_context));
  ctx -> addr = &sock_serv;
  ctx -> socklen = l;
  ctx -> fd = sfd;
  kcpobj = ikcp_create(123,ctx);
  ikcp_wndsize(kcpobj,1024,1024);
  ikcp_nodelay(kcpobj,1,20,2,1);
  ikcp_setoutput(kcpobj,kcp_op);
  ikcp_update(kcpobj,iclock());
  //int n = 1024 * 1024;
  //  if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
  //    // deal with failure, or ignore if you can live with the default size
  //}
	if ((fd = open(argv[3],O_RDONLY))==-1){
		perror("open fail");
		return EXIT_FAILURE;
	}
    
	//taille du fichier
	if (stat(argv[3],&buffer)==-1){
		perror("stat fail");
		return EXIT_FAILURE;
	}
	else
		sz=buffer.st_size;
    
	//preparation de l'envoie
	bzero(&buf,BUFFERT);
    
	gettimeofday(&start,NULL);
    n=read(fd,buf,BUFFERT);
    if(n==-1){
      perror("read fails");
      return EXIT_FAILURE;
    }
	while(n){
		//m=sendto(sfd,buf,n,0,(struct sockaddr*)&sock_serv,l);
    ikcp_update(kcpobj,iclock());
    x=recvfrom(sfd,&buf2,BUFFERT,MSG_DONTWAIT,(struct sockaddr *)&sock_serv,&l);
    //printf("Receving now %d\n",x);
    if(x > 0)
    {
        ikcp_input(kcpobj,buf2,x);
    }
    if(ikcp_waitsnd(kcpobj) < kcpobj->snd_wnd)
    {
        ikcp_send(kcpobj,buf,n);
        usleep(20);
    }
    else
    {
        usleep(20);
        continue;
    }
    //printf("Now window size %d\n",ikcp_waitsnd(kcpobj));
		count+=m;
		//fprintf(stdout,"----\n%s\n----\n",buf);
		bzero(buf,BUFFERT);
    n=read(fd,buf,BUFFERT);
	}
	//read vient de retourner 0 : fin de fichier
	
	//pour debloquer le serv
	//m=sendto(sfd,buf,0,0,(struct sockaddr*)&sock_serv,l);
  //m = ikcp_send(kcpobj,buf,n);
  char endbuf[13]={0x22,0x00,0x0d,0xf4,0x35,0x31,0x02,0x71,0xa7,0x31,0x88,0x80,0x00};
  uint32_t number = htonl(0xDEADBEAF);
  ikcp_send(kcpobj,&endbuf,sizeof(endbuf));
  usleep(20);
  ikcp_update(kcpobj,iclock());
  ikcp_flush(kcpobj);
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
  gettimeofday(&stop,NULL);
	duration(&start,&stop,&delta);
    
	printf("Nombre d'octets transférés : %lld\n",count);
	printf("Sur une taille total de : %lld \n",sz);
	printf("Pour une durée total de : %ld.%d \n",delta.tv_sec,delta.tv_usec);
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

/* Fonction permettant la creation d'un socket
 * Renvoie un descripteur de fichier
 */
int create_client_socket (int port, char* ipaddr){
    int l;
	int sfd;
    
	sfd = socket(AF_INET,SOCK_DGRAM,0);
  int n = 4096 * 1024;
  if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n)) == -1) {
      printf("Error setting buffer size");
  }
  if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
      printf("Error setting buffer size");
  }

	if (sfd == -1){
        perror("socket fail");
        return EXIT_FAILURE;
	}
    
    //preparation de l'adresse de la socket destination
	l=sizeof(struct sockaddr_in);
	bzero(&sock_serv,l);
	
	sock_serv.sin_family=AF_INET;
	sock_serv.sin_port=htons(port);
    if (inet_pton(AF_INET,ipaddr,&sock_serv.sin_addr)==0){
		printf("Invalid IP adress\n");
		return EXIT_FAILURE;
	}
    
    return sfd;
}
