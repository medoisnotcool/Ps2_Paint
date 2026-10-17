/* Inkwell Studio for PS2 (PS2SDK + gsKit + libpad). The whole screen is drawn in software
   into one 512x256 texture each frame, so there is no sprite-count limit. No sound. */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define RGB(r,g,b) ((u32)(r)|((u32)(g)<<8)|((u32)(b)<<16)|0x80000000u)
#define SW 320
#define TOP 12
#define MAXC 512
#define WHITE RGB(255,255,255)
#define COL hsv(hue,sat,val)

static u32 fb[512*256] __attribute__((aligned(128)));
static u32 cv[MAXC*MAXC] __attribute__((aligned(128)));
static u32 ud[MAXC*MAXC] __attribute__((aligned(128)));
static u8 seen[MAXC*MAXC];
static int stk[131072];
static char padBuf[256] __attribute__((aligned(64)));
static int SH=224,CW=256,CH=192,mw=256,mh=192,tool=0,zi=1,sticker=0,hue=210,sat=70,val=90,menu=1,msel=0,drawing=0,rcnt[8];
static int sz[5]={8,22,16,1,72},stb[5]={45,35,0,0,0};
static float zm=1,vx=0,vy=0,cx=160,cy=112,lzx,lzy,fol=1,SX[10],SY[10];
static u32 pc;static int drag=-1,ign=0,penv=0;
static const float ZL[5]={.5f,1,2,4,8};
static const u32 BG=RGB(255,255,255),BAR=RGB(27,32,48),OUTC=RGB(40,44,58),DARK=RGB(43,36,51);
static const char*TN[5]={"BRUSH","MARKER","ERASER","FILL","STICKER"};
static const char*SN[8]={"STAR","HEART","SMILEY","SUN","FLOWER","CLOUD","TREE","ROCKET"};
static const char*ML[12]={"TOOL","SIZE","STABILIZER","HUE","SATURATION","BRIGHTNESS","STICKER","CANVAS WIDTH","CANVAS HEIGHT","NEW CANVAS","CLEAR CANVAS","BACK TO DRAWING"};

/* 5x7 font, ASCII 32..90 (uppercase only) */
static const u8 F[59][5]={
{0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},
{0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{8,0x2A,0x1C,0x2A,8},{8,8,0x3E,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,8,4,2},
{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},
{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},
{0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x49,0x49,0x7A},
{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}};

static int clampi(int v,int a,int b){return v<a?a:v>b?b:v;}
static float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
static void px(int x,int y,u32 c){if(x>=0&&y>=0&&x<SW&&y<SH)fb[y*512+x]=c;}
static void rect(int x,int y,int w,int h,u32 c){for(int j=0;j<h;j++)for(int i=0;i<w;i++)px(x+i,y+j,c);}
static void chr(int x,int y,char ch,u32 c){if(ch>='a'&&ch<='z')ch-=32;if(ch<32||ch>90)ch='?';const u8*g=F[ch-32];for(int i=0;i<5;i++)for(int j=0;j<7;j++)if((g[i]>>j)&1)px(x+i,y+j,c);}
static void txt(int x,int y,const char*s,u32 c){for(;*s;s++,x+=6)chr(x,y,*s,c);}
static void tx(int x,int y,const char*s,u32 c){txt(x+1,y+1,s,RGB(0,0,0));txt(x,y,s,c);}

static u32 hsv(int h,int s,int v){static const int a[3]={5,3,1};float S=s/100.0f,V=v/100.0f,f[3];
 for(int n=0;n<3;n++){float k=fmodf(a[n]+h/60.0f,6.0f),m=fminf(fminf(k,4-k),1.0f);if(m<0)m=0;f[n]=V-V*S*m;}
 return RGB((int)(f[0]*255),(int)(f[1]*255),(int)(f[2]*255));}
static u32 mix(u32 a,u32 b){u32 r=0;for(int i=0;i<24;i+=8)r|=((((a>>i)&255)*45+((b>>i)&255)*55)/100)<<i;return r|0x80000000u;}

/* ---------- painting ---------- */
static void cp(int x,int y){if(x<0||y<0||x>=CW||y>=CH)return;int i=y*MAXC+x;cv[i]=tool==1?mix(ud[i],pc):pc;}
static void stamp(float fx,float fy){float r=sz[tool]/2.0f;
 if(tool==1){for(int i=-(int)r;i<=(int)r;i++){int x=(int)floorf(fx+i*.7f),y=(int)floorf(fy-i*.7f);for(int d=-1;d<=1;d++)cp(x+d,y);}return;}
 int x0=(int)floorf(fx-r),x1=(int)ceilf(fx+r),y0=(int)floorf(fy-r),y1=(int)ceilf(fy+r);
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){float dx=x+.5f-fx,dy=y+.5f-fy;if(dx*dx+dy*dy<=r*r+.3f)cp(x,y);}}
static void seg(float ax,float ay,float bx,float by){float dx=bx-ax,dy=by-ay,d=sqrtf(dx*dx+dy*dy),st=tool==1?1.0f:fmaxf(1.0f,sz[tool]/4.0f);
 int n=(int)(d/st)+1;for(int i=1;i<=n;i++)stamp(ax+dx*i/n,ay+dy*i/n);}

#define MT(i) (abs((int)(cv[i]&255)-tr)+abs((int)((cv[i]>>8)&255)-tg)+abs((int)((cv[i]>>16)&255)-tb)<=60)
static void fillat(int x,int y){if(x<0||y<0||x>=CW||y>=CH)return;
 u32 t=cv[y*MAXC+x],c=COL;int tr=t&255,tg=(t>>8)&255,tb=(t>>16)&255,sp=0;memset(seen,0,sizeof seen);
 stk[sp++]=x;stk[sp++]=y;
 while(sp>0){int yy=stk[--sp],xx=stk[--sp],l=xx,u=0,d=0;
  while(l>=0&&!seen[yy*MAXC+l]&&MT(yy*MAXC+l))l--;l++;
  for(int i=l;i<CW&&!seen[yy*MAXC+i]&&MT(yy*MAXC+i);i++){int k=yy*MAXC+i;seen[k]=1;cv[k]=c;
   if(yy>0){int o=!seen[k-MAXC]&&MT(k-MAXC);if(o&&!u&&sp<131070){stk[sp++]=i;stk[sp++]=yy-1;}u=o;}
   if(yy<CH-1){int o=!seen[k+MAXC]&&MT(k+MAXC);if(o&&!d&&sp<131070){stk[sp++]=i;stk[sp++]=yy+1;}d=o;}}}}

/* ---------- stickers (drawn from shapes, outlined) ---------- */
typedef struct{char t;short v[6];u32 c;}Sh;
#define YEL RGB(255,210,63)
#define PNK RGB(255,90,115)
#define ORG RGB(255,140,40)
#define GRN RGB(47,191,113)
#define CLD RGB(236,242,255)
#define BLU RGB(76,201,240)
#define RED RGB(239,71,111)
#define BRN RGB(141,90,59)
#define FLW RGB(255,143,171)
static const Sh SHP[36]={
{'S',{0,0,100,40},YEL},
{'c',{-46,-28,50},PNK},{'c',{46,-28,50},PNK},{'t',{-92,-5,92,-5,0,98},PNK},
{'c',{0,0,92},YEL},{'c',{-32,-26,11},DARK},{'c',{32,-26,11},DARK},{'e',{0,30,40,24},DARK},{'e',{0,18,46,22},YEL},
{'c',{0,-80,15},YEL},{'c',{0,80,15},YEL},{'c',{-80,0,15},YEL},{'c',{80,0,15},YEL},{'c',{57,57,15},YEL},{'c',{-57,57,15},YEL},{'c',{57,-57,15},YEL},{'c',{-57,-57,15},YEL},{'c',{0,0,50},ORG},
{'c',{0,-52,34},FLW},{'c',{49,-16,34},FLW},{'c',{31,42,34},FLW},{'c',{-31,42,34},FLW},{'c',{-49,-16,34},FLW},{'c',{0,0,26},YEL},
{'c',{-50,18,34},CLD},{'c',{0,-6,46},CLD},{'c',{52,18,34},CLD},{'q',{-50,18,52,52},CLD},
{'t',{0,-98,-58,-8,58,-8},GRN},{'t',{0,-58,-74,48,74,48},GRN},{'q',{-14,48,14,96},BRN},
{'t',{-16,66,16,66,0,100},ORG},{'t',{-30,20,-72,76,-30,64},RED},{'t',{30,20,72,76,30,64},RED},{'e',{0,-8,36,72},CLD},{'c',{0,-26,15},BLU}};
static const int SK[8][2]={{0,1},{1,3},{4,5},{9,9},{18,6},{24,4},{28,3},{31,5}};

static int in(const Sh*s,float x,float y,float g){const short*v=s->v;float dx,dy;
 if(s->t=='c'){dx=x-v[0];dy=y-v[1];return dx*dx+dy*dy<=(v[2]+g)*(v[2]+g);}
 if(s->t=='e'){dx=(x-v[0])/(v[2]+g);dy=(y-v[1])/(v[3]+g);return dx*dx+dy*dy<=1;}
 if(s->t=='q')return x>=v[0]-g&&x<=v[2]+g&&y>=v[1]-g&&y<=v[3]+g;
 if(s->t=='t'){float gx=(v[0]+v[2]+v[4])/3.0f,gy=(v[1]+v[3]+v[5])/3.0f,k=1+g*.05f,p[6];
  for(int i=0;i<3;i++){p[i*2]=gx+(v[i*2]-gx)*k;p[i*2+1]=gy+(v[i*2+1]-gy)*k;}
  float d1=(x-p[2])*(p[1]-p[3])-(p[0]-p[2])*(y-p[3]),d2=(x-p[4])*(p[3]-p[5])-(p[2]-p[4])*(y-p[5]),d3=(x-p[0])*(p[5]-p[1])-(p[4]-p[0])*(y-p[1]);
  return !((d1<0||d2<0||d3<0)&&(d1>0||d2>0||d3>0));}
 {float k=(v[2]+g)/v[2];int c=0;
  for(int i=0,j=9;i<10;j=i++){float ri=(i&1?v[3]:v[2])*k,rj=(j&1?v[3]:v[2])*k,xi=v[0]+SX[i]*ri,yi=v[1]+SY[i]*ri,xj=v[0]+SX[j]*rj,yj=v[1]+SY[j]*rj;
   if(((yi>y)!=(yj>y))&&(x<(xj-xi)*(y-yi)/(yj-yi)+xi))c=!c;}
  return c;}}
static void stkdraw(int id,float ox,float oy,float size,u32*d,int stride,int w,int h){float r=size/2;const Sh*a=SHP+SK[id][0];int n=SK[id][1];
 for(int y=(int)floorf(oy-r*1.1f-2);y<=(int)ceilf(oy+r*1.1f+2);y++)for(int x=(int)floorf(ox-r*1.1f-2);x<=(int)ceilf(ox+r*1.1f+2);x++){
  if(x<0||y<0||x>=w||y>=h)continue;
  float ux=(x+.5f-ox)*100/r,uy=(y+.5f-oy)*100/r;int hit=-1;
  for(int k=0;k<n;k++)if(in(a+k,ux,uy,0))hit=k;
  if(hit>=0)d[y*stride+x]=a[hit].c;
  else for(int k=0;k<n;k++)if(in(a+k,ux,uy,7)){d[y*stride+x]=DARK;break;}}}

/* ---------- canvas + view ---------- */
static void fit(void){int i;for(i=4;i>0;i--)if(CW*ZL[i]<=SW&&CH*ZL[i]<=SH-TOP)break;zi=i;zm=ZL[i];vx=CW/2.0f-SW/zm/2;vy=CH/2.0f-(SH-TOP)/zm/2;}
static void newcanvas(void){CW=mw;CH=mh;for(int i=0;i<MAXC*MAXC;i++)cv[i]=BG;memcpy(ud,cv,sizeof cv);fit();}
static void snapshot(void){memcpy(ud,cv,sizeof cv);}
static void undo(void){for(int y=0;y<CH;y++)for(int x=0;x<CW;x++){int i=y*MAXC+x;u32 t=cv[i];cv[i]=ud[i];ud[i]=t;}}
static void zoomby(int d){int n=zi+d;if(n<0||n>4)return;float tx0=vx+cx/zm,ty0=vy+(cy-TOP)/zm;zi=n;zm=ZL[n];vx=tx0-cx/zm;vy=ty0-(cy-TOP)/zm;}
static void sizeadj(int d){if(tool==3)return;int s=sz[tool],lo=tool==4?16:tool==2?2:1,hi=tool==4?200:tool==1?96:tool==2?128:64;s+=d*(s>32?4:s>12?2:1);sz[tool]=clampi(s,lo,hi);}
static void menuadj(int d){switch(msel){
 case 0:tool=(tool+d+5)%5;break;
 case 1:sizeadj(d);break;
 case 2:if(tool<2)stb[tool]=clampi(stb[tool]+d*5,0,95);break;
 case 3:hue=(hue+d*6+360)%360;break;
 case 4:sat=clampi(sat+d*4,0,100);break;
 case 5:val=clampi(val+d*4,0,100);break;
 case 6:sticker=(sticker+d+8)%8;break;
 case 7:mw=clampi(mw+d*16,16,512);break;
 case 8:mh=clampi(mh+d*16,16,512);break;}}

/* ---------- drawing the screen ---------- */
static void render(void){static int xs[SW];
 for(int x=0;x<SW;x++)xs[x]=(int)floorf(vx+x/zm);
 for(int y=0;y<SH;y++){u32*r=fb+y*512;
  if(y<TOP){for(int x=0;x<SW;x++)r[x]=BAR;continue;}
  int yy=(int)floorf(vy+(y-TOP)/zm);
  for(int x=0;x<SW;x++){int xx=xs[x];r[x]=(xx<0||yy<0||xx>=CW||yy>=CH)?OUTC:cv[yy*MAXC+xx];}}
 char b[64];sprintf(b,"%s  SIZE %d  STAB %d  ZOOM %d%%",TN[tool],tool==3?0:sz[tool],tool<2?stb[tool]:0,(int)(zm*100));
 tx(3,2,b,WHITE);rect(SW-32,1,28,10,WHITE);rect(SW-31,2,26,8,COL);}
static void ring(void){float r=(tool==3?1:sz[tool])*zm/2;
 if(tool!=3){for(int y=(int)(cy-r-2);y<=(int)(cy+r+2);y++)for(int x=(int)(cx-r-2);x<=(int)(cx+r+2);x++){
  if(x<0||x>=SW||y<TOP||y>=SH)continue;float dx=x+.5f-cx,dy=y+.5f-cy;if(fabsf(sqrtf(dx*dx+dy*dy)-r)<.8f)fb[y*512+x]^=0x00FFFFFF;}}
 }
static void cross(void){int ix=(int)cx,iy=(int)cy;
 for(int i=-8;i<=8;i++){if(i>-3&&i<3)continue;int x=ix+i,y=iy+i;if(x>=0&&x<SW&&iy>=0&&iy<SH)fb[iy*512+x]^=0x00FFFFFF;if(y>=0&&y<SH&&ix>=0&&ix<SW)fb[y*512+ix]^=0x00FFFFFF;}
 if(penv)for(int j=-1;j<=1;j++)for(int i=-1;i<=1;i++){int x=ix+i,y=iy+j;if(x>=0&&x<SW&&y>=0&&y<SH)fb[y*512+x]^=0x00FFFFFF;}}
static void valstr(int i,char*b){switch(i){
 case 0:strcpy(b,TN[tool]);break;
 case 1:if(tool==3)strcpy(b,"-");else sprintf(b,"%d",sz[tool]);break;
 case 2:if(tool<2)sprintf(b,"%d",stb[tool]);else strcpy(b,"-");break;
 case 3:sprintf(b,"%d",hue);break;case 4:sprintf(b,"%d",sat);break;case 5:sprintf(b,"%d",val);break;
 case 6:strcpy(b,SN[sticker]);break;case 7:sprintf(b,"%d",mw);break;case 8:sprintf(b,"%d",mh);break;
 default:b[0]=0;}}
static void drawmenu(void){char b[40];
 rect(20,16,280,SH-28,RGB(20,24,36));rect(20,16,280,2,RGB(127,149,255));
 tx(28,22,"INKWELL STUDIO",WHITE);rect(236,20,56,9,WHITE);rect(237,21,54,7,COL);
 for(int i=0;i<12;i++){int y=36+i*12;if(i==msel)rect(24,y-2,272,11,RGB(58,76,170));
  tx(30,y,ML[i],WHITE);valstr(i,b);if(i<9){tx(132,y,"<",WHITE);tx(200,y,">",WHITE);}tx(143,y,b,RGB(255,224,130));
  if(i>=3&&i<=5){int v=i==3?hue:i==4?sat:val,mx=i==3?359:100;
   for(int k=0;k<80;k++)rect(212+k,y,1,7,i==3?hsv(k*359/79,100,100):i==4?hsv(hue,k*100/79,val):hsv(hue,sat,k*100/79));
   int p=212+v*79/mx;rect(p-1,y-2,3,11,WHITE);rect(p,y-1,1,9,DARK);}}
 stkdraw(sticker,258,142,48,fb,512,SW,SH);
 tx(28,184,"X CLICK/DRAW  O UNDO  TRIANGLE NEXT TOOL",RGB(170,180,210));
 tx(28,194,"L1/R1 SIZE  L2/R2 ZOOM  R-STICK PAN",RGB(170,180,210));
 tx(28,204,"L-STICK/DPAD MOVE CROSSHAIR  START MENU",RGB(170,180,210));}

/* ---------- pad helpers ---------- */
static float ax(int v){int d=v-128;return(d>-26&&d<26)?0:d/128.0f;}
static int rep(u32 held,u32 np,u32 bit,int idx){if(np&bit){rcnt[idx]=0;return 1;}if(held&bit){if(++rcnt[idx]>20&&rcnt[idx]%3==0)return 1;}else rcnt[idx]=0;return 0;}
static void waitpad(void){for(int i=0;i<200000;i++){int s=padGetState(0,0);if(s==PAD_STATE_STABLE||s==PAD_STATE_FINDCTP1)break;}}

int main(void){
 for(int i=0;i<10;i++){float a=-3.14159265f/2+i*3.14159265f/5;SX[i]=cosf(a);SY[i]=sinf(a);}
 SifInitRpc(0);
 SifLoadModule("rom0:SIO2MAN",0,NULL);
 SifLoadModule("rom0:PADMAN",0,NULL);
 padInit(0);padPortOpen(0,0,padBuf);waitpad();
 padSetMainMode(0,0,PAD_MMODE_DUALSHOCK,PAD_MMODE_LOCK);waitpad();

 GSGLOBAL*gs=gsKit_init_global();
 gs->ZBuffering=GS_SETTING_OFF;
 dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
 dmaKit_chan_init(DMA_CHANNEL_GIF);
 gsKit_init_screen(gs);
 gsKit_mode_switch(gs,GS_ONESHOT);
 SH=gs->Height/2;if(SH>256)SH=256;
 GSTEXTURE tex;memset(&tex,0,sizeof tex);
 tex.Width=512;tex.Height=256;tex.PSM=GS_PSM_CT32;tex.Mem=fb;tex.Filter=GS_FILTER_NEAREST;
 tex.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(512,256,GS_PSM_CT32),GSKIT_ALLOC_USERBUFFER);
 newcanvas();

 u32 old=0;
 while(1){
  struct padButtonStatus b;u32 pd=0;int jx=128,jy=128,kx=128,ky=128;
  if(padGetState(0,0)==PAD_STATE_STABLE&&padRead(0,0,&b)){pd=0xffff^b.btns;
   if(padInfoMode(0,0,PAD_MODECURID,0)==PAD_TYPE_DUALSHOCK){jx=b.ljoy_h;jy=b.ljoy_v;kx=b.rjoy_h;ky=b.rjoy_v;}}
  u32 np=pd&~old;old=pd;penv=(pd&PAD_CROSS)!=0;
  if(np&PAD_START){menu=!menu;drawing=0;if(!menu)ign=1;}
  {float mx=ax(jx)*3.5f,my=ax(jy)*3.5f;
   if(pd&PAD_LEFT)mx-=1.5f;if(pd&PAD_RIGHT)mx+=1.5f;if(pd&PAD_UP)my-=1.5f;if(pd&PAD_DOWN)my+=1.5f;
   cx=clampf(cx+mx,0,SW-1);cy=clampf(cy+my,menu?0:TOP,SH-1);}
  if(menu){
   int hx=(int)cx,hov=(cy>=34&&cy<178&&hx>=24&&hx<296);
   if(hov){msel=clampi(((int)cy-34)/12,0,11);
    if(msel<9){if(hx>=128&&hx<140){if(rep(pd,np,PAD_CROSS,6))menuadj(-1);}else if(hx>=198&&hx<210){if(rep(pd,np,PAD_CROSS,7))menuadj(1);}}
    if((np&PAD_CROSS)&&(msel==0||msel==6)&&hx<128)menuadj(1);
    if((np&PAD_CROSS)&&msel>=3&&msel<=5&&hx>=210)drag=msel;
    if(np&PAD_CROSS){if(msel==9){newcanvas();menu=0;ign=1;}else if(msel==10){snapshot();for(int y=0;y<CH;y++)for(int x=0;x<CW;x++)cv[y*MAXC+x]=BG;}else if(msel==11){menu=0;ign=1;}}}
   if(!(pd&PAD_CROSS))drag=-1;
   if(drag>=0){int mxv=drag==3?359:100,v=clampi((hx-212)*mxv/79,0,mxv);if(drag==3)hue=v;else if(drag==4)sat=v;else val=v;}
   if(np&PAD_CIRCLE){menu=0;ign=1;}
  }else{
   vx+=ax(kx)*4/zm;vy+=ax(ky)*4/zm;
   if(np&PAD_TRIANGLE)tool=(tool+1)%5;
   if(rep(pd,np,PAD_L1,4))sizeadj(-1);
   if(rep(pd,np,PAD_R1,5))sizeadj(1);
   if(np&PAD_R2)zoomby(1);
   if(np&PAD_L2)zoomby(-1);
   if(np&PAD_CIRCLE)undo();
   float tx0=vx+cx/zm,ty0=vy+(cy-TOP)/zm;if(!(pd&PAD_CROSS))ign=0;int hold=(pd&PAD_CROSS)&&!ign;
   if(tool<3){pc=tool==2?BG:COL;fol=1.0f-stb[tool]*.0093f;
    if(hold&&!drawing){snapshot();drawing=1;lzx=tx0;lzy=ty0;stamp(lzx,lzy);}
    else if(hold){float nx=lzx+(tx0-lzx)*fol,ny=lzy+(ty0-lzy)*fol;seg(lzx,lzy,nx,ny);lzx=nx;lzy=ny;}
    else if(drawing){for(int i=0;i<60;i++){float dx=tx0-lzx,dy=ty0-lzy;if(dx*dx+dy*dy<.5f)break;float nx=lzx+dx*fol,ny=lzy+dy*fol;seg(lzx,lzy,nx,ny);lzx=nx;lzy=ny;}drawing=0;}
   }else if((np&PAD_CROSS)&&!ign){snapshot();if(tool==3){pc=COL;fillat((int)floorf(tx0),(int)floorf(ty0));}else stkdraw(sticker,tx0,ty0,sz[4],cv,MAXC,CW,CH);}
  }
  render();
  if(menu)drawmenu();else ring();cross();
  FlushCache(0);
  gsKit_texture_upload(gs,&tex);
  gsKit_clear(gs,GS_SETREG_RGBAQ(0,0,0,0,0));
  gsKit_prim_sprite_texture(gs,&tex,0,0,0,0,gs->Width,gs->Height,SW,SH,1,GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
  gsKit_queue_exec(gs);
  gsKit_sync_flip(gs);
 }
 return 0;
}
