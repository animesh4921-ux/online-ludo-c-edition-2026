#include "ludo.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    LudoRoom g; ludo_init(&g,"ABC123");g.count=4;g.human_count=4;
    for(int p=0;p<4;p++){snprintf(g.player[p].name,LUDO_NAME,"P%d",p);g.player[p].connected=true;}
    ludo_room_start(&g);
    assert(ludo_track_square(RED,0)==39&&ludo_track_square(GREEN,0)==0);
    assert(ludo_track_square(YELLOW,0)==13&&ludo_track_square(BLUE,0)==26);
    /* A 1 deploys, and the entry square is the first space. */
    assert(ludo_roll(&g,1)==1);assert(ludo_move(&g,0));assert(g.player[0].token[0].step==0);
    /* Exact full dice distance and capture on a non-safe square. */
    g.current=0;g.player[0].token[0]=(LudoToken){TOKEN_TRACK,0};
    g.player[1].token[0]=(LudoToken){TOKEN_TRACK,41}; /* both land on absolute square 41 */
    assert(ludo_roll(&g,2)==2);assert(ludo_move(&g,0));
    assert(g.player[0].token[0].step==2);assert(g.player[1].token[0].state==TOKEN_BASE);
    /* Safe square protection: Red step 13 maps to absolute 0. */
    g.current=0;g.player[0].token[0]=(LudoToken){TOKEN_TRACK,11};g.player[1].token[0]=(LudoToken){TOKEN_TRACK,0};
    assert(ludo_roll(&g,2)==2);assert(ludo_move(&g,0));assert(g.player[1].token[0].state==TOKEN_TRACK);
    /* Home-lane steps are off the shared track and must never cause captures. */
    g.current=0;g.player[0].token[0]=(LudoToken){TOKEN_TRACK,50};
    g.player[1].token[0]=(LudoToken){TOKEN_TRACK,54};
    assert(ludo_roll(&g,1)==1);assert(ludo_move(&g,0));
    assert(g.player[0].token[0].step==51);
    assert(g.player[1].token[0].state==TOKEN_TRACK&&g.player[1].token[0].step==54);
    g.current=0;g.player[0].token[0]=(LudoToken){TOKEN_TRACK,52};
    g.player[1].token[0]=(LudoToken){TOKEN_TRACK,55};
    assert(ludo_roll(&g,1)==1);assert(ludo_move(&g,0));
    assert(g.player[0].token[0].step==53);
    assert(g.player[1].token[0].state==TOKEN_TRACK&&g.player[1].token[0].step==55);
    /* Base exits without a six; exact finish, ranking and skipping are enforced. */
    g.current=0;g.player[0].token[0]=(LudoToken){TOKEN_FINISHED,56};g.player[0].token[1]=(LudoToken){TOKEN_TRACK,54};
    assert(ludo_roll(&g,2)==2);assert(ludo_move(&g,1));assert(g.player[0].done&&g.player[0].rank==1);assert(g.current==1);
    assert(!ludo_valid_move(&g,0,0,1));
    /* Room seats and bot takeover. */
    LudoRoom rooms[LUDO_MAX_ROOMS]={0};int ri=ludo_room_create(rooms,"Host",true,false);assert(ri>=0);
    assert(ludo_room_join(&rooms[ri],"Friend")==1);ludo_room_start(&rooms[ri]);assert(rooms[ri].count==4&&rooms[ri].player[3].bot);
    ludo_disconnect(&rooms[ri],0);assert(rooms[ri].player[0].bot);
    puts("PASS: deployment, track orientation, exact movement, safe-square protection, ranks, active-turn skipping, private seats, bot fill and disconnect takeover");
    return 0;
}
