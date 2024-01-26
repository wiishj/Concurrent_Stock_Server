/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct{
	int maxfd;
	fd_set read_set;
	fd_set ready_set;
	int nready;
	int maxi;
	int clientfd[FD_SETSIZE];
	rio_t clientrio[FD_SETSIZE];
}pool;

int client_num;
void init_pool(int listenfd, pool *p){
	p->maxi=-1;
	for(int i=0; i<FD_SETSIZE; i++) p->clientfd[i]=-1;
	p->maxfd=listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);

	client_num=0;
}

typedef struct _item{
	int ID; 
	int left_stock;
	int price;
	
	struct _item *left;
	struct _item *right;
}item;

item* stock;
char str[MAXLINE];

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

void make_show(item* stock){
	char tmp[MAXLINE];
	
	if(stock==NULL) return;

	sprintf(tmp, "%d %d %d\n", stock->ID, stock->left_stock, stock->price);
	strcat(str, tmp);
	make_show(stock->left); make_show(stock->right);
	
	return;
}

void update_stock(FILE *fp, item* node){
	if(node==NULL) return;
	fprintf(fp, "%d %d %d\n", node->ID, node->left_stock, node->price);
	update_stock(fp, node->left);
	update_stock(fp, node->right);
}
void echo(int connfd);

void add_client(int connfd, pool *p){
	int i;
	p->nready--;
	for(i=0; i<FD_SETSIZE; i++){
		if(p->clientfd[i]<0){
			client_num++;
			p->clientfd[i]=connfd;
			Rio_readinitb(&p->clientrio[i], connfd);
			
			FD_SET(connfd, &p->read_set);
			
			if(connfd > p->maxfd) p->maxfd=connfd;
			if(i> p->maxi) p->maxi=i;
			
			break;
		}
	}
	if(i==FD_SETSIZE) app_error("add_client error : too many clients");
}

void check_client(pool *p){
	int connfd, n;
	char buf[MAXLINE];
	rio_t rio;

	for(int i=0; (i<=p->maxi) && (p->nready >0); i++){
		connfd=p->clientfd[i];
		rio=p->clientrio[i];

		if((connfd>0) && (FD_ISSET(connfd, &p->ready_set))){
			p->nready--;
			if((n=Rio_readlineb(&rio, buf, MAXLINE))!=0){
				printf("Server received %d bytes on fd %d\n", n, connfd);
				//Rio_writen(connfd, buf, MAXLINE);
				
				if(!strcmp(buf, "show\n")){
					memset(str, 0, sizeof(str));
					make_show(stock);
					Rio_writen(connfd, str, MAXLINE);
				}
				else if(!strcmp(buf, "exit\n")){
					client_num--;
					/*FILE *fp=fopen("stock.txt", "w");
					item *node=stock;
					update_stock(fp, node);
					fclose(fp);
					*/
					Rio_writen(connfd, "exit\n", MAXLINE);
					return;
				}
				else if(!strncmp(buf, "buy", 3)){
					char cmd[10]; int b_id, b_num;
					sscanf(buf, "%s %d %d", cmd, &b_id, &b_num);
					item *node=stock;
					int flag=0;

					while(node){
						if(node==NULL) break;
						if(b_id==node->ID){
							if(node->left_stock >= b_num) flag=1;
							break;
						}
						else if(b_id < node->ID) node=node->left;
						else node=node->right;
					}

					if(!flag) strcpy(buf, "Not enough left stock\n\0");
					else{
						node->left_stock -= b_num;
						strcpy(buf, "[buy] success\n\0");
					}
					Rio_writen(connfd, buf, MAXLINE);
				}
				else if(!strncmp(buf, "sell", 4)){
					char cmd[10]; int s_id, s_num;
					sscanf(buf, "%s %d %d", cmd, &s_id, &s_num);
					item *node = stock;

					while(node){
						if(node==NULL) break;
						if(s_id==node->ID) break;
						else if(s_id < node->ID) node=node->left;
						else node=node->right;
					}
					
					node->left_stock += s_num;
					strcpy(buf, "[sell] success\n\0");
					Rio_writen(connfd, buf, MAXLINE);
				}
				//Rio_writen(connfd, buf, MAXLINE);
			}
			else{
				Close(connfd);
				FD_CLR(connfd, &p->read_set);
				p->clientfd[i]=-1;
				client_num--;
			}
		}
	}
}
int main(int argc, char **argv) 
{
	int listenfd, connfd;
    socklen_t clientlen;
    int stck_id, left_stck, stck_price;
	struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	static pool pool;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

	FILE *fp=fopen("stock.txt", "r");
	if(fp==NULL){
		printf("ERROR : file doesn't exist\n");
		return 0;
	}
	while(EOF!=fscanf(fp, "%d %d %d\n", &stck_id, &left_stck, &stck_price)){
		stock=make_stock(stock, stck_id, left_stck, stck_price);
	}
	fclose(fp);

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

	while (1) {
		pool.ready_set=pool.read_set;
		pool.nready=Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
		if(FD_ISSET(listenfd, &pool.ready_set)){
			clientlen = sizeof(struct sockaddr_storage); 
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        	Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        	printf("Connected to (%s, %s)\n", client_hostname, client_port);
			//echo(connfd);
			add_client(connfd, &pool);
		}
		check_client(&pool);

		if(!client_num){
			FILE* fp=fopen("stock.txt", "w");
			item* cur=stock;
			update_stock(fp, cur);
			fclose(fp);
		}
	}
	//Close(connfd);
    exit(0);
}
/* $end echoserverimain */
