#include "ludo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define CLOSESOCK closesocket
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
typedef int socket_t;
#define CLOSESOCK close
#endif

static LudoRoom rooms[LUDO_MAX_ROOMS];
static time_t seen[LUDO_MAX_ROOMS][LUDO_PLAYERS];
static time_t queued[LUDO_MAX_ROOMS];
static char buf[32768];
static const char *colors[]={"Red","Green","Yellow","Blue"};
static int safe_copy(char *out,size_t cap,const char *s) {
    size_t n=0;if(!s)s="";while(*s&&n+1<cap){unsigned char c=(unsigned char)*s++;if(c=='"'||c=='\\'||c<32)continue;out[n++]=(char)c;}out[n]=0;return (int)n;
}
static void json_string(const char *src,char *dst,size_t cap){safe_copy(dst,cap,src);}
static void getstr(const char *json,const char *key,char *out,size_t cap){
    char k[80];snprintf(k,sizeof(k),"\"%s\"",key);const char*p=strstr(json,k);if(!p){out[0]=0;return;}p=strchr(p,':');if(!p){out[0]=0;return;}p++;while(*p==' '||*p=='\t')p++;if(*p!='"'){out[0]=0;return;}p++;size_t n=0;while(*p&&*p!='"'&&n+1<cap){if(*p=='\\'&&p[1])p++;out[n++]=*p++;}out[n]=0;
}
static void parse_line(const char*line,char*method,size_t mcap,char*path,size_t pcap){size_t i=0;while(*line==' ')line++;while(*line&&*line!=' '&&i+1<mcap)method[i++]=*line++;method[i]=0;while(*line==' ')line++;i=0;while(*line&&*line!=' '&&i+1<pcap)path[i++]=*line++;path[i]=0;}
static int getint(const char *json,const char *key,int def){char k[80];snprintf(k,sizeof(k),"\"%s\"",key);const char*p=strstr(json,k);if(!p||(p=strchr(p,':'))==NULL)return def;return atoi(p+1);}
static int roomidx(const char *code){for(int i=0;i<LUDO_MAX_ROOMS;i++)if(rooms[i].code[0]&&!strcmp(rooms[i].code,code))return i;return -1;}
static int playeridx(LudoRoom*r,const char*owner){for(int i=0;i<r->count;i++)if(!strcmp(r->player[i].owner,owner))return i;return -1;}
static void sendall(socket_t s,const char*p,size_t n){while(n){int k=send(s,p,(int)n,0);if(k<=0)return;p+=k;n-=(size_t)k;}}
static void reply(socket_t s,int status,const char *type,const char *body){char h[512];int n=snprintf(h,sizeof(h),"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",status,status==200?"OK":"Bad Request",type,strlen(body));sendall(s,h,(size_t)n);sendall(s,body,strlen(body));}
static bool static_file(socket_t s,const char *url){const char*file=NULL,*type="text/plain";if(!strcmp(url,"/")||!strcmp(url,"/index.html")){file="../frontend/index.html";type="text/html; charset=utf-8";}else if(!strcmp(url,"/style.css")){file="../frontend/style.css";type="text/css; charset=utf-8";}else if(!strcmp(url,"/app.js")){file="../frontend/app.js";type="application/javascript; charset=utf-8";}else if(!strcmp(url,"/health")){reply(s,200,"application/json","{\"ok\":true}");return true;}else return false;FILE*f=fopen(file,"rb");if(!f){reply(s,404,"text/plain","Not found");return true;}char data[65536];size_t n=fread(data,1,sizeof(data)-1,f);fclose(f);data[n]=0;reply(s,200,type,data);return true;}
static void state_json(LudoRoom*r,const char*viewer,char*out,size_t cap){
    size_t n=(size_t)snprintf(out,cap,"{\"code\":\"%s\",\"mode\":\"%s\",\"status\":\"%s\",\"current\":%d,\"roll\":",r->code,r->public_room?"public":r->private_room?"private":"bots",r->over?"finished":r->started?"playing":"waiting",r->current);
    n+=(size_t)snprintf(out+n,cap-n,r->rolled?"%d":"null",r->die);
    n+=(size_t)snprintf(out+n,cap-n,",\"valid\":[");
    if(r->rolled){int first=1;for(int t=0;t<2;t++)if(ludo_valid_move(r,r->current,t,r->die)){n+=(size_t)snprintf(out+n,cap-n,"%s\"token-%d\"",first?"":",",t);first=0;}}
    const char*winner="";for(int i=0;i<r->count;i++)if(r->player[i].rank==1)winner=r->player[i].name;
    n+=(size_t)snprintf(out+n,cap-n,"],\"message\":\"%s\",\"winner\":\"%s\",\"players\":[",r->over?winner:r->started?r->player[r->current].name:"Waiting for players",r->over?winner:"");
    for(int i=0;i<r->count;i++){LudoPlayer*p=&r->player[i];char name[LUDO_NAME],owner[LUDO_NAME+8];json_string(p->name,name,sizeof(name));json_string(p->owner,owner,sizeof(owner));
        n+=(size_t)snprintf(out+n,cap-n,"%s{\"id\":\"p%d\",\"ownerId\":\"%s\",\"name\":\"%s\",\"seat\":%d,\"bot\":%s,\"connected\":%s,\"finished\":%s,\"finishPosition\":%d,\"tokens\":[",i?",":"",i,!p->bot&&viewer&&strcmp(p->owner,viewer)==0?owner:"",name,i,p->bot?"true":"false",p->connected?"true":"false",p->done?"true":"false",p->rank);
        for(int t=0;t<2;t++)n+=(size_t)snprintf(out+n,cap-n,"%s{\"id\":\"token-%d\",\"state\":\"%s\",\"step\":%d}",t?",":"",t,p->token[t].state==TOKEN_BASE?"home":p->token[t].state==TOKEN_FINISHED?"finish":"track",p->token[t].step);
        n+=(size_t)snprintf(out+n,cap-n,"]}");
    }
    n+=(size_t)snprintf(out+n,cap-n,"],\"rankings\":[");int first=1;for(int place=1;place<=r->ranks;place++)for(int i=0;i<r->count;i++)if(r->player[i].rank==place){n+=(size_t)snprintf(out+n,cap-n,"%s{\"id\":\"p%d\",\"name\":\"%s\",\"seat\":%d,\"position\":%d}",first?"":",",i,r->player[i].name,i,place);first=0;}
    snprintf(out+n,cap-n,"]}");
}
static void run_bots(int ri){LudoRoom*r=&rooms[ri];for(int loops=0;loops<20&&r->started&&!r->over&&r->player[r->current].bot;loops++){int d=ludo_roll(r,0);if(d<0)break;if(r->rolled){int ti=ludo_bot_move(r,r->current,d);if(ti>=0)ludo_move(r,ti);}}}
static void handle(socket_t s){
    int got=0;while(got<(int)sizeof(buf)-1){int k=recv(s,buf+got,(int)sizeof(buf)-1-got,0);if(k<=0)break;got+=k;buf[got]=0;char*end=strstr(buf,"\r\n\r\n");if(end){int bodylen=0;char*cl=strstr(buf,"Content-Length:");if(cl)bodylen=atoi(cl+15);int have=got-(int)(end+4-buf);if(have>=bodylen)break;}}
    if(got<=0)return;buf[got]=0;char*sep=strstr(buf,"\r\n\r\n");if(!sep){reply(s,400,"application/json","{\"ok\":false}");return;}char*body=sep+4;char method[12]={0},path[256]={0};parse_line(buf,method,sizeof(method),path,sizeof(path));if(!strcmp(method,"OPTIONS")){reply(s,200,"text/plain","");return;}if(!strcmp(method,"GET")){char staticPath[256];snprintf(staticPath,sizeof(staticPath),"%s",path);char*q=strchr(staticPath,'?');if(q)*q=0;if(static_file(s,staticPath))return;}
    char name[LUDO_NAME],owner[LUDO_NAME+8],code[16];getstr(body,"name",name,sizeof(name));getstr(body,"clientId",owner,sizeof(owner));getstr(body,"code",code,sizeof(code));char cleaned[LUDO_NAME+8];safe_copy(cleaned,sizeof(cleaned),name);memcpy(name,cleaned,strlen(cleaned)+1);safe_copy(cleaned,sizeof(cleaned),owner);memcpy(owner,cleaned,strlen(cleaned)+1);int ri=-1,pi=-1;
    if(!strcmp(path,"/api/create")||!strcmp(path,"/api/bots")){int human=!strcmp(path,"/api/bots")?getint(body,"humanCount",1):1;if(human<1||human>4)human=1;ri=ludo_room_create(rooms,name,!strcmp(path,"/api/create"),false);if(ri>=0){LudoRoom*r=&rooms[ri];for(int j=0;j<human;j++){int seat=j?ludo_room_join(r,"Player"):0;if(seat>=0){snprintf(r->player[seat].owner,sizeof(r->player[seat].owner),"%s",owner);seen[ri][seat]=time(NULL);}}if(!strcmp(path,"/api/bots"))ludo_room_start(r);pi=0;}}
    else if(!strcmp(path,"/api/join")){ri=roomidx(code);if(ri>=0&&rooms[ri].private_room)pi=ludo_room_join(&rooms[ri],name);if(pi>=0){snprintf(rooms[ri].player[pi].owner,sizeof(rooms[ri].player[pi].owner),"%s",owner);seen[ri][pi]=time(NULL);}}
    else if(!strcmp(path,"/api/match")){bool created=false;for(int i=0;i<LUDO_MAX_ROOMS;i++)if(rooms[i].code[0]&&rooms[i].public_room&&!rooms[i].started&&rooms[i].count<4){ri=i;break;}if(ri<0){ri=ludo_room_create(rooms,name,false,true);created=ri>=0;if(ri>=0)queued[ri]=time(NULL);}if(ri>=0){pi=created?0:ludo_room_join(&rooms[ri],name);if(pi>=0){if(created)safe_copy(rooms[ri].player[pi].name,LUDO_NAME,name);snprintf(rooms[ri].player[pi].owner,sizeof(rooms[ri].player[pi].owner),"%s",owner);seen[ri][pi]=time(NULL);}if(rooms[ri].count==4)ludo_room_start(&rooms[ri]);}}
    else if(!strcmp(path,"/api/state")){ri=roomidx(code);if(ri>=0){pi=playeridx(&rooms[ri],owner);if(pi>=0)seen[ri][pi]=time(NULL);}}
    else {if(path[0]=='/')memmove(path,path+1,strlen(path));char*q=strchr(path,'?');if(q)*q=0;ri=roomidx(code);if(ri>=0){LudoRoom*r=&rooms[ri];pi=playeridx(r,owner);if(pi>=0)seen[ri][pi]=time(NULL);
        if(!strcmp(path,"api/start")&&pi==0&&r->private_room)ludo_room_start(r);
        else if(!strcmp(path,"api/roll")&&pi==r->current){ludo_roll(r,0);}
        else if(!strcmp(path,"api/move")&&pi==r->current){int ti=getint(body,"token",-1);ludo_move(r,ti);}
        else if(!strcmp(path,"api/leave")&&pi>=0){ludo_disconnect(r,pi);}
    }}
    char out[20000];if(ri<0){reply(s,200,"application/json","{\"ok\":false,\"error\":\"Room unavailable or full\"}");return;}LudoRoom*r=&rooms[ri];if(!r->started&&!r->private_room&&!r->public_room&&pi==0)ludo_room_start(r);
    if(r->public_room&&!r->started&&queued[ri]&&time(NULL)-queued[ri]>=15)ludo_room_start(r);
    for(int i=0;i<r->count;i++)if(!r->player[i].bot&&r->player[i].connected&&r->started&&time(NULL)-seen[ri][i]>20&&seen[ri][i])ludo_disconnect(r,i);
    run_bots(ri);state_json(r,owner,out,sizeof(out));if(!strcmp(path,"/api/create")||!strcmp(path,"/api/join")||!strcmp(path,"/api/match")||!strcmp(path,"/api/bots")){char state[20000];snprintf(state,sizeof(state),"%s",out);snprintf(out,sizeof(out),"{\"ok\":true,\"code\":\"%s\",\"player\":%d,\"state\":%s}",r->code,pi,state);}reply(s,200,"application/json",out);
}
int main(void){
#ifdef _WIN32
    WSADATA wd;if(WSAStartup(MAKEWORD(2,2),&wd)!=0){fprintf(stderr,"Winsock init failed\n");return 1;}
#endif
    srand((unsigned)time(NULL));int port=3001;const char*env=getenv("PORT");if(env)port=atoi(env);socket_t server=socket(AF_INET,SOCK_STREAM,0);if(server==(socket_t)-1){perror("socket");return 1;}int yes=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,(const char*)&yes,sizeof(yes));struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons((unsigned short)port);if(bind(server,(struct sockaddr*)&a,sizeof(a))<0||listen(server,32)<0){perror("bind/listen");return 1;}printf("C Ludo server listening on http://localhost:%d\n",port);fflush(stdout);
    for(;;){socket_t client=accept(server,NULL,NULL);if(client==(socket_t)-1)continue;handle(client);CLOSESOCK(client);}
}
