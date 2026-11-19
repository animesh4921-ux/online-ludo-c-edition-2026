#include "ludo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int START[LUDO_PLAYERS] = {39,0,13,26};
static const int SAFE[] = {0,8,13,21,26,34,39,47};
static const char *COLORS[LUDO_PLAYERS] = {"Red","Green","Yellow","Blue"};
static void set_name(char dst[LUDO_NAME], const char *src) {
    if (!src || !*src) src="Guest";
    snprintf(dst,LUDO_NAME,"%s",src);
}
void ludo_init(LudoRoom *g, const char *code) {
    memset(g,0,sizeof(*g)); if(code) snprintf(g->code,sizeof(g->code),"%.6s",code);
    g->die=0;
}
int ludo_track_square(int player, int step) {
    if(player<0||player>=LUDO_PLAYERS||step<0||step>50) return -1;
    return (START[player]+step)%LUDO_TRACK;
}
bool ludo_safe_square(int square) {
    for(size_t i=0;i<sizeof(SAFE)/sizeof(SAFE[0]);i++) if(SAFE[i]==square) return true;
    return false;
}
bool ludo_valid_move(const LudoRoom *g,int p,int ti,int die) {
    if(!g||p<0||p>=g->count||ti<0||ti>=LUDO_TOKENS||die<1||die>6||g->player[p].done) return false;
    const LudoToken *t=&g->player[p].token[ti];
    return t->state!=TOKEN_FINISHED && (t->state==TOKEN_BASE || t->step+die<=56);
}
static void advance(LudoRoom *g) {
    if(g->over||g->count==0) return;
    for(int n=0;n<g->count;n++) { g->current=(g->current+1)%g->count; if(!g->player[g->current].done) return; }
    g->over=true;
}
int ludo_roll(LudoRoom *g,int forced_die) {
    if(!g||!g->started||g->over||g->rolled||g->player[g->current].bot==false&&!g->player[g->current].connected) return -1;
    int d=forced_die?forced_die:rand()%6+1; if(d<1||d>6) return -1;
    g->die=d; g->rolled=true;
    bool any=false; for(int t=0;t<LUDO_TOKENS;t++) any|=ludo_valid_move(g,g->current,t,d);
    if(!any) { g->rolled=false; g->die=0; if(d!=6) advance(g); }
    return d;
}
bool ludo_move(LudoRoom *g,int ti) {
    if(!g||!g->started||g->over||!g->rolled||!ludo_valid_move(g,g->current,ti,g->die)) return false;
    int p=g->current,d=g->die; LudoToken *t=&g->player[p].token[ti];
    if(t->state==TOKEN_BASE) { t->state=TOKEN_TRACK; t->step=d-1; } else t->step+=d;
    if(t->step==56) t->state=TOKEN_FINISHED;
    bool captured=false;
    if(t->state==TOKEN_TRACK) {
        int square=ludo_track_square(p,t->step);
        /* Home-lane steps are not on the shared track; track_square returns -1. */
        if(square>=0&&!ludo_safe_square(square)) for(int q=0;q<g->count;q++) if(q!=p)
            for(int k=0;k<LUDO_TOKENS;k++) { LudoToken *o=&g->player[q].token[k];
                if(o->state==TOKEN_TRACK&&ludo_track_square(q,o->step)==square) { o->state=TOKEN_BASE;o->step=0;captured=true; }
            }
    }
    bool won=true;for(int k=0;k<LUDO_TOKENS;k++) won&=g->player[p].token[k].state==TOKEN_FINISHED;
    g->rolled=false;g->die=0;
    if(won&&!g->player[p].done) { g->player[p].done=true;g->player[p].rank=++g->ranks; }
    if(g->ranks==g->count) g->over=true;
    else if(won||(!captured&&d!=6)) advance(g);
    return true;
}
int ludo_room_create(LudoRoom *rooms,const char *name,bool priv,bool pub) {
    if(!rooms)return -1;
    for(int i=0;i<LUDO_MAX_ROOMS;i++) if(!rooms[i].code[0]) {
        char code[7];snprintf(code,sizeof(code),"%06X",(unsigned)(rand()%0x1000000));
        ludo_init(&rooms[i],code);rooms[i].private_room=priv;rooms[i].public_room=pub;rooms[i].count=1;rooms[i].human_count=1;
        set_name(rooms[i].player[0].name,name);rooms[i].player[0].connected=true;return i;
    }
    return -1;
}
int ludo_room_join(LudoRoom *r,const char *name) {
    if(!r||r->started||r->count>=LUDO_PLAYERS)return -1;
    int i=r->count++;LudoPlayer *p=&r->player[i];memset(p,0,sizeof(*p));set_name(p->name,name);p->connected=true;r->human_count++;return i;
}
void ludo_room_start(LudoRoom *r) {
    if(!r||r->started||r->human_count<1)return;
    while(r->count<LUDO_PLAYERS) { int i=r->count++;LudoPlayer *p=&r->player[i];memset(p,0,sizeof(*p));p->bot=true;snprintf(p->name,LUDO_NAME,"%s Bot",COLORS[i]); }
    r->started=true;r->current=0;
}
void ludo_disconnect(LudoRoom *r,int p) { if(r&&p>=0&&p<r->count&&!r->player[p].bot) { r->player[p].connected=false;r->player[p].owner[0]=0;if(r->started) { r->player[p].bot=true;snprintf(r->player[p].name,LUDO_NAME,"%s Bot",COLORS[p]); } } }
int ludo_bot_move(const LudoRoom *g,int p,int die) {
    if(!g||p<0||p>=g->count)return -1;int best=-1,score=-100000;
    for(int i=0;i<LUDO_TOKENS;i++)if(ludo_valid_move(g,p,i,die)){
        const LudoToken *t=&g->player[p].token[i];int step=t->state==TOKEN_BASE?die-1:t->step+die;
        int s=(t->state==TOKEN_BASE?20:step);if(step==56)s+=10000;
        if(step<51){int sq=ludo_track_square(p,step);if(ludo_safe_square(sq))s+=35;
            for(int q=0;q<g->count;q++)if(q!=p)for(int k=0;k<2;k++)if(g->player[q].token[k].state==TOKEN_TRACK&&ludo_track_square(q,g->player[q].token[k].step)==sq)s+=500;
        }if(s>score){score=s;best=i;}
    }return best;
}
