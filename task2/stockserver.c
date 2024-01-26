/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct _item{
	int ID; 
	int left_stock;
	int price;

	struct _item *left;
	struct _item *right;
}item;

item *stock;
char str[MAXLINE];
int read_cnt, byte_cnt;
int trd_num;
sem_t mutex, w;

void make_show(item* stock){
	char tmp[MAXLINE];
	
	if(stock==NULL) return;
	
	P(&mutex);
	read_cnt++;
	if(read_cnt==1) P(&w); //write block 
	V(&mutex);

	sprintf(tmp, "%d %d %d\n", stock->ID, stock->left_stock, stock->price);
	strcat(str, tmp);
	
	make_show(stock->left); make_show(stock->right);

	P(&mutex);
	read_cnt--;
	if(read_cnt==0) V(&w);
	V(&mutex);

	return;
}

void update_stock(FILE* fp, item* node){
	if(node==NULL) return;
	fprintf(fp, "%d %d %d\n", node->ID, node->left_stock, node->price);
	update_stock(fp, node->left);
	update_stock(fp, node->right);
}

void exe_cmd(int connfd){
	int n; 
	char buf[MAXLINE];
	rio_t rio;
	
	Rio_readinitb(&rio, connfd);
	while((n=Rio_readlineb(&rio, buf, MAXLINE))!=0){
		printf("Server received %d bytes on fd %d\n", n, connfd);

		if(!strcmp(buf, "show\n")){
			memset(str, 0, sizeof(str));
			make_show(stock);
			Rio_writen(connfd, str, MAXLINE);
			//printf("%s", str);
		}
		else if(!strcmp(buf, "exit\n")){
			FILE *fp=fopen("stock.txt", "w");
			item *node=stock;
			update_stock(fp, node);
			fclose(fp);
			
			Rio_writen(connfd, "exit\n", MAXLINE);
			return;
		}
		else if(!strncmp(buf, "buy", 3)){
			char cmd[10]; int b_id, b_num;
			sscanf(buf, "%s %d %d", cmd, &b_id, &b_num);
			item *node = stock;
			int flag=0;

			while(node){
				if(node==NULL) break;
				if(b_id==node->ID){
					P(&mutex);
					read_cnt++;
					if(read_cnt==1) P(&w);
					V(&mutex);

					if(node->left_stock >= b_num) flag=1;
					break;
				}
				else if(b_id < node->ID) node=node->left;
				else node=node->right;
			}

			P(&mutex);
			read_cnt--;
			if(read_cnt==0) V(&w);
			V(&mutex);
			
			P(&w);
			if(!flag) strcpy(buf, "Not enough left stock\n\0");
			else{
				//printf("%d %d\n", node->left_stock, b_num);
				node->left_stock -= b_num;
				strcpy(buf, "[buy] success\n\0");
			}
			V(&w);
			Rio_writen(connfd, buf, MAXLINE);
		}
		else if(!strncmp(buf, "sell", 4)){
			char cmd[10]; int s_id, s_num;
			sscanf(buf, "%s %d %d", cmd, &s_id, &s_num);
			item* node=stock;
			while(node){
				if(node==NULL) break;
				if(s_id==node->ID){
					P(&mutex);
					read_cnt++;
					if(read_cnt==1) P(&w);
					V(&mutex);
					break;
				}
				else if(s_id < node->ID) node=node->left;
				else node=node->right;
			}
			
			P(&mutex);
			read_cnt--;
			if(read_cnt==0) V(&w);
			V(&mutex);
			
			P(&w);
			node->left_stock+=s_num;
			strcpy(buf, "[sell] success\n\0");
			V(&w);
			Rio_writen(connfd, buf, MAXLINE);
		}
	}
	P(&w);
	trd_num--;
	V(&w);
}

void *thread(void *vargp){
	int connfd=*((int*)vargp);
	Pthread_detach(pthread_self());
	Free(vargp);

	
	exe_cmd(connfd);
	Close(connfd);
	
	//printf("trd cnt %d\n", trd_num);
	if(trd_num==0){
		FILE* fp=fopen("stock.txt", "w");
		item* cur=stock;
		update_stock(fp, cur);
		fclose(fp);
	}
}

item* make_stock(item* stock, int ID, int left_stock, int price){
	if(stock==NULL){
		stock=(item*)malloc(sizeof(item));
		stock->right = stock->left = NULL;
		stock->ID = ID;
		stock->left_stock = left_stock;
		stock->price = price;
	}
	else{
		if(ID <stock->ID) stock->left = make_stock(stock->left, ID, left_stock, price);
		else stock->right = make_stock(stock->right, ID, left_stock, price);
	}
	return stock;
}
void echo(int connfd);

int main(int argc, char **argv) 
{
    int listenfd, *connfd;
	int stck_id, left_stck, stck_price;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid;
	
    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }
	
	FILE *fp=fopen("stock.txt", "r");
	if(fp==NULL){
		printf("file doesn't exist\n");
		exit(0);
	}
	while(EOF!=fscanf(fp, "%d %d %d\n", &stck_id, &left_stck, &stck_price)){
		stock=make_stock(stock, stck_id, left_stck, stck_price);
	}
	fclose(fp);
    
	listenfd = Open_listenfd(argv[1]);
	
	//init semaphore
	sem_init(&mutex, 0, 1);
	sem_init(&w, 0, 1);
	
	while (1) {
		clientlen = sizeof(struct sockaddr_storage); 
		connfd = Malloc(sizeof(int));
		*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA*) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		trd_num++;
		Pthread_create(&tid, NULL, thread, connfd);
	}
    exit(0);
}
/* $end echoserverimain */
