#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cyc_ui.h"
#include "cyc_settings.h"
#include "cyc_png.h"
#include "recomp_runtime_ui.h"
static int applied,saved,state_saved;
static void apply(void){applied++;}
static void save(void){saved++;}
static bool save_state(void){state_saved++;return true;}
static void nav(int input){cyc_ui_nav(input,false);}
int main(int argc,char **argv){
 SDL_SetMainReady();assert(SDL_Init(0)==0);
 SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,640,480,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
 SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);assert(r);
 CycSettings s;cyc_settings_default(&s);CycSettings initial=s;
 CycUiHost h={0};h.settings=&s;h.apply=apply;h.save=save;h.save_state=save_state;
 assert(cyc_ui_init(NULL,r,&h));cyc_ui_toggle_menu();assert(cyc_ui_menu_open());
 const char *names[]={"inicio","som","video","controles","sistema"};
 for(int i=0;i<5;i++){
  cyc_ui_render();SDL_RenderPresent(r);char path[1024];
  snprintf(path,sizeof path,"%s-%s.png",argc>1?argv[1]:"build/menu",names[i]);
  assert(cyc_write_png(path,surface->pixels,640,480));nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);
 }
 assert(memcmp(&s,&initial,sizeof s)==0 && applied==0 && saved==0);
 /* One header Right is Sound. Down enters the options, where Right edits. */
 nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);nav(RECOMP_RUNTIME_UI_INPUT_DOWN);
 int old=s.audio_enabled;nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);
 assert(s.audio_enabled==!old && applied==1 && saved==1);
 nav(RECOMP_RUNTIME_UI_INPUT_BACK);nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);nav(RECOMP_RUNTIME_UI_INPUT_DOWN);
 old=s.view_mode;nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);assert(s.view_mode==(old+1)%3);
 nav(RECOMP_RUNTIME_UI_INPUT_DOWN);
 old=s.fullscreen;nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);assert(s.fullscreen==!old);
 nav(RECOMP_RUNTIME_UI_INPUT_BACK);nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);
 nav(RECOMP_RUNTIME_UI_INPUT_DOWN);nav(RECOMP_RUNTIME_UI_INPUT_DOWN);nav(RECOMP_RUNTIME_UI_INPUT_ACCEPT);
 SDL_Event ev;SDL_zero(ev);ev.type=SDL_KEYDOWN;ev.key.keysym.scancode=SDL_SCANCODE_K;
 cyc_ui_process_event(&ev);assert(s.bind.button[0][0].key==SDL_SCANCODE_K);
 /* The captured event must not also navigate or fire an action. */
 nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);
 ev.type=SDL_KEYUP;cyc_ui_process_event(&ev);
 nav(RECOMP_RUNTIME_UI_INPUT_BACK);nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);
 nav(RECOMP_RUNTIME_UI_INPUT_DOWN);nav(RECOMP_RUNTIME_UI_INPUT_DOWN);
 nav(RECOMP_RUNTIME_UI_INPUT_RIGHT);assert(state_saved==0);
 nav(RECOMP_RUNTIME_UI_INPUT_ACCEPT);assert(state_saved==1);
 cyc_ui_nav(RECOMP_RUNTIME_UI_INPUT_ACCEPT,true);assert(state_saved==1);
 nav(RECOMP_RUNTIME_UI_INPUT_BACK);nav(RECOMP_RUNTIME_UI_INPUT_BACK);assert(!cyc_ui_menu_open());
 cyc_ui_shutdown();SDL_DestroyRenderer(r);SDL_FreeSurface(surface);SDL_Quit();return 0;
}
