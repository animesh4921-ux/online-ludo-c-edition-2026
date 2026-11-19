'use strict';
const API='';
const CLIENT_ID=localStorage.getItem('ludoClientId')||(()=>{const x='c-'+Math.random().toString(36).slice(2);localStorage.setItem('ludoClientId',x);return x;})();
const socket={id:CLIENT_ID,handlers:{},roomCode:null,poll:null,
 on(event,fn){this.handlers[event]=fn;},
 async emit(event,payload,callback){
  if(typeof payload==='function'){callback=payload;payload={};} payload=payload||{};
  const routes={createRoom:'/api/create',joinRoom:'/api/join',playBots:'/api/bots',publicMatch:'/api/match',startGame:'/api/start',roll:'/api/roll',move:'/api/move',leaveRoom:'/api/leave'};
  const path=routes[event];if(!path){callback?.({ok:false,error:'Unknown action'});return;}
  const data={...payload,clientId:CLIENT_ID};if(event==='move')data.token=Number(String(payload.tokenId||'').slice(-1));
  if(this.roomCode&&event!=='createRoom'&&event!=='joinRoom'&&event!=='playBots'&&event!=='publicMatch')data.code=this.roomCode;
  try{const res=await fetch(API+path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});const result=await res.json();
   if(result.ok===false){callback?.(result);return;}if(result.code)this.roomCode=result.code;
   callback?.({ok:true,code:result.code||this.roomCode});if(result.state)this.handlers.state?.(result.state);
   if(result.code&&!this.poll)this.beginPoll();
  }catch(e){callback?.({ok:false,error:'Cannot reach the C game server. Is it running on port 3001?'});}
 },
 beginPoll(){this.poll=setInterval(async()=>{try{const res=await fetch(API+'/api/state',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({code:this.roomCode,clientId:CLIENT_ID})});if(res.ok)this.handlers.state?.(await res.json());}catch{}},600);}
};
setTimeout(()=>socket.handlers.connect?.(),0);
const $=id=>document.getElementById(id);
const COLORS=['red','green','yellow','blue'];
const TRACK=[[6,1],[6,2],[6,3],[6,4],[6,5],[5,6],[4,6],[3,6],[2,6],[1,6],[0,6],[0,7],[0,8],[1,8],[2,8],[3,8],[4,8],[5,8],[6,9],[6,10],[6,11],[6,12],[6,13],[6,14],[7,14],[8,14],[8,13],[8,12],[8,11],[8,10],[8,9],[9,8],[10,8],[11,8],[12,8],[13,8],[14,8],[14,7],[14,6],[13,6],[12,6],[11,6],[10,6],[9,6],[8,5],[8,4],[8,3],[8,2],[8,1],[8,0],[7,0],[6,0]];
// Seat order stays Red, Green, Yellow, Blue; board positions follow the requested classic orientation.
const START=[39,0,13,26];
const HOME=[[[13,7],[12,7],[11,7],[10,7],[9,7],[8,7]],[[7,1],[7,2],[7,3],[7,4],[7,5],[7,6]],[[1,7],[2,7],[3,7],[4,7],[5,7],[6,7]],[[7,13],[7,12],[7,11],[7,10],[7,9],[7,8]]];
const BASE=[[[11,2],[12,3]],[[2,2],[3,3]],[[2,11],[3,12]],[[11,11],[12,12]]];
const SAFE=[[6,1],[2,6],[1,8],[6,12],[8,13],[12,8],[13,6],[8,2]];
let state=null,roomCode=null;
function toast(msg){$('toast').textContent=msg;$('toast').classList.add('show');setTimeout(()=>$('toast').classList.remove('show'),2800)}
function playerName(){return $('guestName').value.trim().slice(0,18)||'Guest'}
function showGame(code){roomCode=code;$('lobby').classList.add('hidden');$('game').classList.remove('hidden');$('codeDisplay').textContent=code;}
function enter(event,payload){socket.emit(event,payload,res=>{if(!res?.ok)return toast(res?.error||'Could not join.');showGame(res.code);});}
$('bots').onclick=()=>enter('playBots',{name:playerName(),humanCount:$('humanCount').value});
$('create').onclick=()=>enter('createRoom',{name:playerName()});
$('join').onclick=()=>enter('joinRoom',{name:playerName(),code:$('roomCode').value});
$('match').onclick=()=>enter('publicMatch',{name:playerName()});
$('start').onclick=()=>socket.emit('startGame',r=>{if(!r?.ok)toast(r?.error||'Could not start');});
$('roll').onclick=()=>socket.emit('roll',r=>{if(!r?.ok)toast(r?.error||'Cannot roll now');});
$('leave').onclick=()=>socket.emit('leaveRoom',()=>location.reload());
$('copy').onclick=async()=>{const link=location.origin+'/?room='+roomCode;try{await navigator.clipboard.writeText(`Join my Ludo room ${roomCode}: ${link}`);toast('Invite copied');}catch{toast('Share room code: '+roomCode)}};
function cell(r,c){return $('board').children[r*15+c]}
function makeBoard(){const b=$('board');b.innerHTML='';for(let r=0;r<15;r++)for(let c=0;c<15;c++){const el=document.createElement('div');el.className='cell';el.dataset.r=r;el.dataset.c=c;
 if(r<6&&c<6)el.classList.add('greenhome');else if(r<6&&c>8)el.classList.add('yellowhome');else if(r>8&&c>8)el.classList.add('bluehome');else if(r>8&&c<6)el.classList.add('redhome');
 if(TRACK.some(([rr,cc])=>rr===r&&cc===c))el.classList.add('track');
 if(HOME[0].some(([rr,cc])=>rr===r&&cc===c))el.classList.add('redlane');if(HOME[1].some(([rr,cc])=>rr===r&&cc===c))el.classList.add('greenlane');if(HOME[2].some(([rr,cc])=>rr===r&&cc===c))el.classList.add('yellowlane');if(HOME[3].some(([rr,cc])=>rr===r&&cc===c))el.classList.add('bluelane');
 if(SAFE.some(([rr,cc])=>rr===r&&cc===c))el.classList.add('safe');b.appendChild(el);}const hub=document.createElement('div');hub.className='center-hub';hub.setAttribute('aria-hidden','true');hub.innerHTML='<span class="hub-wedge green"></span><span class="hub-wedge yellow"></span><span class="hub-wedge red"></span><span class="hub-wedge blue"></span><span class="hub-pin"></span>';b.appendChild(hub);}
function tokenPos(seat,t,id){if(t.state==='home')return BASE[seat][id];if(t.state==='finish')return HOME[seat][5];if(t.step>=51)return HOME[seat][t.step-51];return TRACK[(START[seat]+t.step)%52];}
function draw(){if(!state)return;for(const el of $('board').querySelectorAll('.cell'))el.innerHTML='';
 const entries=[];state.players.forEach(p=>p.tokens.forEach((t,i)=>{const [r,c]=tokenPos(p.seat,t,i);entries.push({p,t,i,r,c});}));
 const byCell=new Map();for(const item of entries){const key=`${item.r},${item.c}`;if(!byCell.has(key))byCell.set(key,[]);byCell.get(key).push(item);}
 for(const occupants of byCell.values())occupants.forEach((item,slot)=>{const {p,t,i,r,c}=item;const el=document.createElement('button');el.className=`token ${COLORS[p.seat]}`;el.title=`${p.name}, token ${i+1}`;el.setAttribute('aria-label',el.title);el.dataset.playerId=p.id;el.dataset.tokenId=t.id;const isTurn=state.players[state.current]?.ownerId===socket.id;
 const eligible=state.status==='playing'&&state.roll!==null&&isTurn&&p.id===state.players[state.current]?.id&&state.valid?.includes(t.id);
 if(eligible)el.classList.add('selectable');else el.disabled=true;
 // Fan tokens within a shared cell so each remains a distinct, clickable target.
 const columns=Math.ceil(Math.sqrt(occupants.length)),rows=Math.ceil(occupants.length/columns),col=slot%columns,row=Math.floor(slot/columns);el.style.width=`${Math.min(76,88/columns)}%`;el.style.height=`${Math.min(76,88/rows)}%`;el.style.left=`${((col+.5)/columns)*100}%`;el.style.top=`${((row+.5)/rows)*100}%`;el.style.transform='translate(-50%, -50%)';
 el.onclick=()=>{const eligibleHere=byCell.get(`${r},${c}`).filter(({p:owner,t:token})=>owner.id===state.players[state.current]?.id&&state.valid?.includes(token.id));if(!eligibleHere.some(({p:owner,t:token})=>owner.id===p.id&&token.id===t.id))return;socket.emit('move',{playerId:p.id,tokenId:t.id},res=>{if(!res?.ok)toast(res?.error||'Move rejected');});};cell(r,c).appendChild(el);});
 $('players').innerHTML='';state.players.forEach((p,i)=>{const row=document.createElement('div');row.className='player'+(i===state.current&&state.status==='playing'?' current':'')+(p.finished?' finished':'');const dot=document.createElement('span');dot.className='dot';dot.style.background=`var(--${COLORS[p.seat]})`;const label=document.createElement('span');label.textContent=p.name;const meta=document.createElement('small');meta.textContent=p.finished?`${p.finishPosition}${p.finishPosition===1?'st':p.finishPosition===2?'nd':p.finishPosition===3?'rd':'th'} place · FINISHED`:`${p.bot?'BOT':p.connected?'ONLINE':'OFFLINE'} · ${p.tokens.filter(t=>t.state==='finish').length}/2 home`;row.append(dot,label,meta);$('players').appendChild(row);});
 $('status').textContent=state.message;$('hint').textContent=state.status==='waiting'?`Players joined: ${state.players.length}/4`:state.status==='finished'?`Final rankings: ${state.rankings.map(p=>`${p.position}. ${p.name}`).join(' · ')}`:state.roll===null?'Roll the dice on your turn':`Rolled ${state.roll}. Select a highlighted token.`;
 $('dice').textContent=state.roll??'🎲';$('roll').disabled=state.status!=='playing'||state.roll!==null||state.players[state.current]?.ownerId!==socket.id;
 $('start').classList.toggle('hidden',!(state.mode==='private'&&state.status==='waiting'&&state.players.some(p=>p.ownerId===socket.id&&p.seat===0)));$('start').disabled=!state.players.some(p=>!p.bot);
 $('codeDisplay').textContent=state.code;
}
socket.on('state',s=>{state=s;draw();});
socket.on('connect',()=>{document.querySelector('.online').innerHTML='<i></i> Server connected';});
socket.on('disconnect',()=>{document.querySelector('.online').textContent='Reconnecting…';});
makeBoard();const invite=new URLSearchParams(location.search).get('room');if(invite)$('roomCode').value=invite.toUpperCase();
