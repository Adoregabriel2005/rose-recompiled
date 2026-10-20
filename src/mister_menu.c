/* Rose Recompiled tabs; the cycle host owns pause and persistence. */
#include "cyc_ui.h"
#include "cyc_settings.h"
#include "cyc_overlay.h"
#include "recomp_runtime_ui.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
static CycUiHost host;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static bool opened, nav_blocked;
static int page, row, capture=-1;
static uint32_t pixels[640*480];
static char toast[96];
static uint64_t toast_until;
enum { HOME,AUDIO,VIDEO,CONTROLS,SYSTEM,PAGES };
enum { RESUME,SAVE,LOAD,QUIT,SOUND,VOLUME,VIEWMODE,FULLSCREEN,SCALE,INTEGER,FILTER,VSYNC,DEADZONE,DEFAULTS,KEY_A,KEY_B,KEY_SELECT,KEY_START,KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT };
typedef struct {const char *label;int action;} Item;
static const Item home_items[]={{"CONTINUAR",RESUME},{"SALVAR ESTADO",SAVE},{"CARREGAR ESTADO",LOAD},{"SAIR",QUIT}};
static const Item audio_items[]={{"SOM",SOUND},{"VOLUME",VOLUME},{"RESTAURAR SOM",DEFAULTS}};
static const Item video_items[]={{"FORMATO DA IMAGEM",VIEWMODE},{"TELA CHEIA",FULLSCREEN},{"ESCALA DA JANELA",SCALE},{"ESCALA INTEIRA",INTEGER},{"FILTRO SUAVE",FILTER},{"SINCRONIZACAO VERTICAL",VSYNC},{"RESTAURAR VIDEO",DEFAULTS}};
static const Item control_items[]={{"DEADZONE DO CONTROLE",DEADZONE},{"TECLA A",KEY_A},{"TECLA B",KEY_B},{"TECLA SELECT",KEY_SELECT},{"TECLA START",KEY_START},{"TECLA CIMA",KEY_UP},{"TECLA BAIXO",KEY_DOWN},{"TECLA ESQUERDA",KEY_LEFT},{"TECLA DIREITA",KEY_RIGHT},{"RESTAURAR CONTROLES",DEFAULTS}};
static const Item system_items[]={{"RESTAURAR CONFIGURACOES",DEFAULTS},{"SAIR",QUIT}};
static const struct {const char *title;const Item *items;int count;} pages[PAGES]={
 {"INICIO",home_items,4},{"SOM",audio_items,3},{"VIDEO",video_items,7},{"CONTROLES",control_items,10},{"SISTEMA",system_items,2}};
int cyc_ui_launcher(struct CycSettings *s,const char *p,const CycHostExtras *e,const char **rom,bool fds,const CycFdsBiosLookup *bios,const void *mods)
{(void)s;(void)p;(void)e;(void)rom;(void)fds;(void)bios;(void)mods;return 0;}
bool cyc_ui_init(SDL_Window *win,SDL_Renderer *ren,const CycUiHost *h)
{
 (void)win;host=*h;renderer=ren;opened=false;page=HOME;row=-1;capture=-1;nav_blocked=false;toast_until=0;
 texture=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,480);return texture!=NULL;
}
void cyc_ui_shutdown(void){SDL_DestroyTexture(texture);texture=NULL;}
bool cyc_ui_menu_open(void){return opened;}
void cyc_ui_toggle_menu(void){opened=!opened;capture=-1;if(opened){page=HOME;row=-1;}}
void cyc_ui_set_toast(const char *title,const char *body)
{
 if(!title&&!body){toast[0]=0;toast_until=0;return;}
 snprintf(toast,sizeof toast,"%s: %s",title?title:"",body?body:"");toast_until=SDL_GetTicks64()+3000;
}
static void commit(void){if(host.apply)host.apply();if(host.save)host.save();}
void cyc_ui_process_event(const union SDL_Event *ev)
{
 nav_blocked=false;
 if(!opened||capture<0||ev->type!=SDL_KEYDOWN)return;
 nav_blocked=true;if(ev->key.repeat)return;
 int key=ev->key.keysym.scancode;
 if(key==SDL_SCANCODE_ESCAPE){capture=-1;return;}
 if(key==SDL_SCANCODE_DELETE)key=0;
 if(key){
  for(int i=0;i<CYC_SC_COUNT;i++)if(host.settings->bind.shortcut[i].key==key){cyc_ui_set_toast("TECLA RESERVADA","ESCOLHA OUTRA");return;}
  for(int i=0;i<CYC_INPUT_BUTTONS;i++)if(i!=capture&&host.settings->bind.button[0][i].key==key){cyc_ui_set_toast("TECLA EM USO","ESCOLHA OUTRA");return;}
 }
 host.settings->bind.button[0][capture].key=key;capture=-1;commit();
}
static void restore_page(void)
{
 CycSettings def;cyc_settings_default(&def);CycSettings *s=host.settings;
 if(page==AUDIO||page==SYSTEM){s->audio_enabled=def.audio_enabled;s->volume=def.volume;}
 if(page==VIDEO||page==SYSTEM){s->fullscreen=def.fullscreen;s->window_scale=def.window_scale;s->integer_scale=def.integer_scale;s->linear_filter=def.linear_filter;s->vsync=def.vsync;s->view_mode=0;if(host.extras&&host.extras->set_view_mode)host.extras->set_view_mode(host.extras->ctx,0);}
 if(page==CONTROLS||page==SYSTEM)s->bind=def.bind;
}
void cyc_ui_nav(int input,bool repeat)
{
 if(input==RECOMP_RUNTIME_UI_INPUT_TOGGLE){if(!repeat)cyc_ui_toggle_menu();return;}
 if(!opened||nav_blocked)return;
 if(input==RECOMP_RUNTIME_UI_INPUT_BACK){if(capture>=0)capture=-1;else if(row>=0)row=-1;else opened=false;return;}
 if(capture>=0)return;
 int count=pages[page].count;
 if(input==RECOMP_RUNTIME_UI_INPUT_UP){row=row<0?count-1:row-1;return;}
 if(input==RECOMP_RUNTIME_UI_INPUT_DOWN){row=row==count-1?-1:row+1;return;}
 if(input!=RECOMP_RUNTIME_UI_INPUT_LEFT&&input!=RECOMP_RUNTIME_UI_INPUT_RIGHT&&input!=RECOMP_RUNTIME_UI_INPUT_ACCEPT)return;
 int d=input==RECOMP_RUNTIME_UI_INPUT_LEFT?-1:1;
 if(row<0){if(input==RECOMP_RUNTIME_UI_INPUT_ACCEPT)row=0;else page=(page+d+PAGES)%PAGES;return;}
 int action=pages[page].items[row].action;CycSettings *s=host.settings;
 if(action==RESUME||action==SAVE||action==LOAD||action==QUIT||action==DEFAULTS||action>=KEY_A){
  if(input!=RECOMP_RUNTIME_UI_INPUT_ACCEPT||repeat)return;
  switch(action){
  case RESUME:opened=false;return;
  case SAVE:cyc_ui_set_toast("SALVAR",host.save_state&&host.save_state()?"OK":"FALHOU");return;
  case LOAD:cyc_ui_set_toast("CARREGAR",host.load_state&&host.load_state()?"OK":"FALHOU");return;
  case QUIT:if(host.quit)host.quit();return;
  case DEFAULTS:restore_page();break;
  default:capture=action-KEY_A;return;
  }
 }else switch(action){
 case VIEWMODE:s->view_mode=(s->view_mode+d+3)%3;if(host.extras&&host.extras->set_view_mode)host.extras->set_view_mode(host.extras->ctx,s->view_mode);break;
 case FULLSCREEN:s->fullscreen=!s->fullscreen;break;
 case SCALE:s->window_scale=(s->window_scale-1+d+8)%8+1;break;
 case INTEGER:s->integer_scale=!s->integer_scale;break;
 case FILTER:s->linear_filter=!s->linear_filter;break;
 case VSYNC:s->vsync=!s->vsync;break;
 case SOUND:s->audio_enabled=!s->audio_enabled;break;
 case VOLUME:s->volume+=d*5;if(s->volume<0)s->volume=0;if(s->volume>100)s->volume=100;break;
 case DEADZONE:s->bind.deadzone[0]+=d*5;if(s->bind.deadzone[0]<0)s->bind.deadzone[0]=0;if(s->bind.deadzone[0]>95)s->bind.deadzone[0]=95;break;
 }
 commit();
}
static void box(int x,int y,int w,int h,uint32_t color)
{for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)pixels[j*640+i]=color|0xff000000;}
static void text(int x,int y,uint32_t color,const char *str)
{cyc_overlay_text(pixels,640,480,x,y,2,color,str);}
static void vertical(int x,int y,uint32_t color,const char *str)
{
 for(int n=0;str[n];n++){
  const uint8_t *g=cyc_overlay_glyph(str[n]);if(!g)continue;
  for(int j=0;j<5;j++)for(int i=0;i<3;i++)if(g[j]&(1<<(2-i)))box(x+j*2,y-n*8-i*2,2,2,color);
 }
}
static void arrow(int x,int y,int d,uint32_t color)
{for(int n=0;n<6;n++)box(x+d*n,y-n,2,n*2+2,color);}
static void value(int action,char *out,size_t size)
{
 CycSettings *s=host.settings;out[0]=0;
 if(action>=KEY_A){if(capture==action-KEY_A)snprintf(out,size,"PRESSIONE...");else cyc_key_text(s->bind.button[0][action-KEY_A].key,out,size);return;}
 switch(action){
 case VIEWMODE:{static const char *names[]={"ORIGINAL 4:3","16:9 ESTICADO","16:9 AMPLO"};snprintf(out,size,"%s",names[s->view_mode<0||s->view_mode>2?0:s->view_mode]);break;}
 case FULLSCREEN:snprintf(out,size,"%s",s->fullscreen?"SIM":"NAO");break;
 case SCALE:snprintf(out,size,"%dX",s->window_scale);break;
 case INTEGER:snprintf(out,size,"%s",s->integer_scale?"SIM":"NAO");break;
 case FILTER:snprintf(out,size,"%s",s->linear_filter?"SIM":"NAO");break;
 case VSYNC:snprintf(out,size,"%s",s->vsync?"SIM":"NAO");break;
 case SOUND:snprintf(out,size,"%s",s->audio_enabled?"LIGADO":"DESLIGADO");break;
 case VOLUME:snprintf(out,size,"%d / 100",s->volume);break;
 case DEADZONE:snprintf(out,size,"%d / 100",s->bind.deadzone[0]);break;
 }
}
void cyc_ui_render(void)
{
 if(!opened&&SDL_GetTicks64()>=toast_until)return;
 memset(pixels,0,sizeof pixels);const uint32_t light=0xf4d6d3,dark=0x310b16,ink=0x34151d;
 if(opened){
  for(int n=0;n<640*480;n++)pixels[n]=0x70000000;
  box(32,24,576,32,light);text(46,35,ink,"ROSE RECOMPILED");text(500,35,ink,"V0.2");
  box(32,88,576,344,light);box(64,88,544,344,dark);
  for(int y=116;y<432;y+=4)box(64,y,544,1,0x350d18);
  vertical(44,300,ink,pages[page].title);
  uint32_t head=row<0?light:dark,headink=row<0?ink:light;
  box(64,88,544,28,head);arrow(82,101,1,headink);arrow(586,101,-1,headink);
  text(336-(int)strlen(pages[page].title)*4,97,headink,pages[page].title);
  for(int n=0;n<pages[page].count;n++){
   int y=132+n*23;uint32_t color=n==row?ink:light;
   if(n==row)box(72,y-5,528,21,light);
   text(82,y,color,pages[page].items[n].label);
   char v[48];value(pages[page].items[n].action,v,sizeof v);
   int len=(int)strlen(v);if(len>20)len=20;v[len]=0;text(588-len*8,y,color,v);
  }
  for(int n=0;n<PAGES;n++)box(308+n*12,406,6,4,n==page?light:0x703646);
  if(capture>=0)text(46,450,light,"PRESSIONE UMA TECLA. DELETE: LIMPAR. ESC: CANCELAR.");
  else if(row<0)text(46,450,light,"ESQ/DIR: ABAS  BAIXO: OPCOES  ESC: CONTINUAR");
  else text(46,450,light,"ESQ/DIR: AJUSTAR  ENTER: ESCOLHER  ESC: CABECALHO");
 }
 if(SDL_GetTicks64()<toast_until){box(32,4,576,16,light);text(40,6,ink,toast);}
 SDL_UpdateTexture(texture,NULL,pixels,640*4);SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);SDL_RenderCopy(renderer,texture,NULL,NULL);
}
