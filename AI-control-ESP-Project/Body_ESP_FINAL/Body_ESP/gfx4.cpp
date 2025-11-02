#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include "gfx4.h"

extern Arduino_GFX *gfx;

static const uint16_t COL_BG=BLACK, COL_FRAME=0xC618, COL_GRID=0x8410, COL_TEXT=WHITE, COL_LABELBG=0x001F;  // Blauw i.p.v. groen
static const uint16_t COL_WAVE[5]={0xF800,0xFFE0,0x07FF,0x001F,0xF81F}; // 5e kleur: Magenta
// Knoppen kleuren voor nieuwe interface
static const uint16_t COL_BTN_REC_READY=0x07E0;  // GROEN: klaar om op te nemen
static const uint16_t COL_BTN_REC_ACTIVE=0xF800; // ROOD: bezig met opnemen
static const uint16_t COL_BTN_PLAY_READY=0x780F; // PAARS: klaar om af te spelen
static const uint16_t COL_BTN_PLAY_ACTIVE=0xF800;// ROOD: bezig met afspelen
static const uint16_t COL_BTN_MENU_BG=0xCE59;    // Lichtgrijs voor Menu
static const uint16_t COL_BTN_MENU_HI=0x001F;    // Blauw voor Menu actief
static const uint16_t COL_BTN_OVERRULE_OFF=0x07E0; // GROEN: AI overrule uit
static const uint16_t COL_BTN_OVERRULE_ON=0xF800;  // ROOD: AI overrule aan
static const uint16_t COL_BTN_TX=WHITE;          // Witte tekst voor leesbaarheid
static const uint16_t COL_STATUS_BG=0x2104, COL_STATUS_TX=WHITE;

static const int16_t MARGIN=6, LABEL_W=60, BTN_H=36, BTN_GAP=6, STATUS_H=16, STATUS_GAP=4;
static int16_t SCR_W,SCR_H, bandX, bandY0, bandW, bandH, perBandH, statusY;

static int16_t cursorX[G4_NUM], prevY[G4_NUM];
static float envAbs[G4_NUM], divScale[G4_NUM];
static char labelTxt[G4_NUM][12]={"Hart","Temp","Huid","Oxy","Machine"};

static inline int16_t clampi(int16_t v,int16_t lo,int16_t hi){ return v<lo?lo:(v>hi?hi:v); }

static void layoutCompute(){
  SCR_W=gfx->width(); SCR_H=gfx->height();
  bandX=MARGIN+LABEL_W+4; bandW=SCR_W-bandX-MARGIN;
  int16_t vertical=SCR_H-(MARGIN+STATUS_H+STATUS_GAP+BTN_H+MARGIN);
  perBandH=vertical/G4_NUM; bandY0=MARGIN; bandH=perBandH-8;
  statusY = SCR_H - (BTN_H + MARGIN + STATUS_H + STATUS_GAP);
}
static void getBandRect(uint8_t ch,int16_t &x,int16_t &y,int16_t &w,int16_t &h){
  layoutCompute(); x=bandX; y=bandY0+ch*perBandH; w=bandW; h=bandH;
}
static void drawBandChrome(uint8_t ch){
  int16_t x,y,w,h; getBandRect(ch,x,y,w,h);
  gfx->fillRect(MARGIN,y,LABEL_W,20,COL_LABELBG);
  gfx->drawRect(MARGIN,y,LABEL_W,20,COL_TEXT);
  gfx->setTextColor(WHITE); gfx->setTextSize(1); gfx->setCursor(MARGIN+6,y+6); gfx->print(labelTxt[ch]);  // Witte tekst
  gfx->drawRect(x-1,y-1,w+2,h+2,COL_FRAME);
  gfx->drawFastHLine(x,y+h/2,w,COL_GRID);
  for(int gx=0; gx<w; gx+=25) gfx->drawFastVLine(x+gx,y,h,(gx%100==0)?COL_GRID:0x6318);
}
static void clearBand(uint8_t ch){ int16_t x,y,w,h; getBandRect(ch,x,y,w,h); gfx->fillRect(x,y,w,h,COL_BG); drawBandChrome(ch); }
static void drawRecButton(int16_t x,int16_t y,int16_t w,int16_t h,bool recording, bool disabled=false){
  uint16_t color;
  const char* text;
  
  if (disabled) {
    color = 0x39E7;  // Grijs voor uitgeschakeld
    text = "---";
  } else {
    color = recording ? COL_BTN_REC_ACTIVE : COL_BTN_REC_READY;
    text = recording ? "STOP" : "REC";
  }
  
  gfx->fillRect(x,y,w,h,color); 
  gfx->drawRect(x,y,w,h,COL_FRAME);
  int16_t tw=strlen(text)*12; gfx->setTextColor(COL_BTN_TX); gfx->setTextSize(2);
  gfx->setCursor(x+(w-tw)/2, y+(h-16)/2); gfx->print(text);
}

static void drawPlayButton(int16_t x,int16_t y,int16_t w,int16_t h,bool playing){
  uint16_t color = playing ? COL_BTN_PLAY_ACTIVE : COL_BTN_PLAY_READY;
  const char* text = playing ? "STOP" : "PLAY";
  gfx->fillRect(x,y,w,h,color); 
  gfx->drawRect(x,y,w,h,COL_FRAME);
  int16_t tw=strlen(text)*12; gfx->setTextColor(COL_BTN_TX); gfx->setTextSize(2);
  gfx->setCursor(x+(w-tw)/2, y+(h-16)/2); gfx->print(text);
}

static void drawMenuButton(int16_t x,int16_t y,int16_t w,int16_t h,bool active){
  uint16_t color = active ? COL_BTN_MENU_HI : COL_BTN_MENU_BG;
  gfx->fillRect(x,y,w,h,color); 
  gfx->drawRect(x,y,w,h,COL_FRAME);
  int16_t tw=4*12; gfx->setTextColor(COL_BTN_TX); gfx->setTextSize(2);
  gfx->setCursor(x+(w-tw)/2, y+(h-16)/2); gfx->print("MENU");
}

static void drawOverruleButton(int16_t x,int16_t y,int16_t w,int16_t h,bool active){
  uint16_t color = active ? COL_BTN_OVERRULE_ON : COL_BTN_OVERRULE_OFF;
  const char* text = active ? "AI ON" : "AI OFF";
  gfx->fillRect(x,y,w,h,color); 
  gfx->drawRect(x,y,w,h,COL_FRAME);
  int16_t tw=strlen(text)*6; gfx->setTextColor(BLACK); gfx->setTextSize(1);  // Kleinere tekst en zwarte kleur
  gfx->setCursor(x+(w-tw)/2, y+(h-8)/2); gfx->print(text);
}

void gfx4_begin(){
  layoutCompute(); gfx->fillScreen(COL_BG);
  for(uint8_t ch=0; ch<G4_NUM; ++ch){ cursorX[ch]=0; prevY[ch]=-1; envAbs[ch]=100.0f; divScale[ch]=300.0f; drawBandChrome(ch); }
  gfx->fillRect(MARGIN,statusY,SCR_W-2*MARGIN,STATUS_H,COL_STATUS_BG); gfx->drawRect(MARGIN,statusY,SCR_W-2*MARGIN,STATUS_H,COL_FRAME);
  int16_t y=SCR_H-BTN_H-MARGIN; int16_t usableW=SCR_W-(MARGIN*2)-(BTN_GAP*3); int16_t w=usableW/4;
  int16_t x1=MARGIN, x2=x1+w+BTN_GAP, x3=x2+w+BTN_GAP, x4=x3+w+BTN_GAP;
  drawRecButton(x1,y,w,BTN_H,false,false);  // REC knop (niet recording, niet disabled)
  drawPlayButton(x2,y,w,BTN_H,false); // PLAY knop (niet playing)
  drawMenuButton(x3,y,w,BTN_H,false); // MENU knop (niet actief)
  drawOverruleButton(x4,y,w,BTN_H,false); // OVERRULE knop (niet actief)
}
void gfx4_clear(){ for(uint8_t ch=0; ch<G4_NUM; ++ch){ clearBand(ch); cursorX[ch]=0; prevY[ch]=-1; } }
void gfx4_setLabel(uint8_t ch,const char* name){
  if(ch>=G4_NUM||!name) return; strncpy(labelTxt[ch],name,sizeof(labelTxt[ch])-1); labelTxt[ch][sizeof(labelTxt[ch])-1]='\0';
  int16_t x,y,w,h; getBandRect(ch,x,y,w,h);
  gfx->fillRect(MARGIN,y,LABEL_W,20,COL_LABELBG); gfx->drawRect(MARGIN,y,LABEL_W,20,COL_TEXT);
  gfx->setTextColor(WHITE); gfx->setTextSize(1); gfx->setCursor(MARGIN+6,y+6); gfx->print(labelTxt[ch]);  // Witte tekst
}
static int16_t mapToY(uint8_t ch,float v){
  int16_t x,y,w,h; getBandRect(ch,x,y,w,h);
  float a=fabsf(v); envAbs[ch]=(a>envAbs[ch])?(a*0.8f+envAbs[ch]*0.2f):(envAbs[ch]*0.995f+a*0.005f);
  float targetDiv=fmaxf(50.0f, envAbs[ch]/0.90f); divScale[ch]=divScale[ch]*0.9f+targetDiv*0.1f;
  float norm=v/divScale[ch]; int16_t yy = y + (h/2) - int(norm * (h/2 - 3));
  return clampi(yy, y+1, y+h-2);
}
void gfx4_pushSample(uint8_t ch,float value,bool mark){
  if(ch>=G4_NUM) return; int16_t x,y,w,h; getBandRect(ch,x,y,w,h);
  int16_t cx=x+cursorX[ch]; gfx->drawFastVLine(cx,y,h,COL_BG);
  if((cursorX[ch]%25)==0) gfx->drawFastVLine(cx,y,h,(cursorX[ch]%100==0)?COL_GRID:0x6318);
  gfx->drawPixel(cx, y+h/2, COL_GRID);
  int16_t yy=mapToY(ch,value);
  if(prevY[ch]<0) prevY[ch]=yy;
  if(yy==prevY[ch]) gfx->drawPixel(cx,yy,COL_WAVE[ch]);
  else { int16_t y0=(prevY[ch]<yy)?prevY[ch]:yy, y1=(prevY[ch]>yy)?prevY[ch]:yy; gfx->drawFastVLine(cx,y0,(y1-y0)+1,COL_WAVE[ch]); }
  prevY[ch]=yy;
  if(mark){ for(int i=0;i<5;i++) gfx->drawPixel(cx,y+i,COL_WAVE[ch]); }
  cursorX[ch]++; if(cursorX[ch]>=w){ cursorX[ch]=0; clearBand(ch); }
}
void gfx4_drawButtons(bool recording,bool playing,bool menu, bool overruleActive, bool recDisabled){
  layoutCompute(); int16_t y=SCR_H-BTN_H-MARGIN; int16_t usableW=SCR_W-(MARGIN*2)-(BTN_GAP*3); int16_t w=usableW/4;
  int16_t x1=MARGIN, x2=x1+w+BTN_GAP, x3=x2+w+BTN_GAP, x4=x3+w+BTN_GAP;
  drawRecButton(x1,y,w,BTN_H,recording,recDisabled);   // REC/STOP knop (disabled tijdens playback)
  drawPlayButton(x2,y,w,BTN_H,playing);               // PLAY/STOP knop
  drawMenuButton(x3,y,w,BTN_H,menu);                  // MENU knop
  drawOverruleButton(x4,y,w,BTN_H,overruleActive);    // OVERRULE knop
}
void gfx4_drawStatus(const char* text){
  layoutCompute(); int16_t x=MARGIN, w=SCR_W-2*MARGIN;
  gfx->fillRect(x,statusY,w,STATUS_H,COL_STATUS_BG); gfx->drawRect(x,statusY,w,STATUS_H,COL_FRAME);
  gfx->setTextColor(COL_STATUS_TX); gfx->setTextSize(1); gfx->setCursor(x+6,statusY+4);
  if(text && *text) gfx->print(text); else gfx->print(" ");
}

