#include "app_web_album.h"
#include "app_album.h"
#include "app_config.h"

#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static WebServer *g_srv = nullptr;
static File g_up_file;
static bool g_up_ok = false;
static char g_up_name[24] = "";
static File g_cast_file;
static bool g_cast_ok = false;
static size_t g_cast_bytes = 0;

static bool albumCastBlocked() {
  return albumCastBusy() || albumDisplayPending();
}

void webAlbumAppendPage(String &b) {
  b += F(
      "<div class=ed-shell>"
      "<div class=ed-work>"
      "<div class=ed-toolbar>"
      "<div class=ed-tool-main>"
      "<label class=file-pick>"
      "<span class=file-cta><input type=file id=pick accept='image/*,.2bp,application/octet-stream' multiple>选取图片 / .2bp</span>"
      "<span class='file-name empty' id=fileName>未选择 · 可多选批量导入</span>"
      "</label>"
      "<div class=ed-sep></div>"
      "<div class=ed-tool-group title=画面>"
      "<button type=button id=btnPan class=secondary>平移</button>"
      "<button type=button id=btnFit class=secondary>铺满</button>"
      "<button type=button id=btnContain class=secondary>完整</button>"
      "<button type=button id=btnInvert class=secondary title=反色>反色</button>"
      "<button type=button id=btnReset class=secondary>复位</button>"
      "</div>"
      "<div class=ed-sep></div>"
      "<div class='ed-tool-group ed-paint' title=笔触>"
      "<button type=button id=btnBrush class=secondary>画笔</button>"
      "<button type=button id=btnErase class=secondary>橡皮</button>"
      "<div class=pal-sw id=brushPal>"
      "<button type=button id=br0 class='pal-k on' title=黑 aria-label=黑></button>"
      "<button type=button id=br1 class=pal-w title=白 aria-label=白></button>"
      "<button type=button id=br2 class=pal-y title=黄 aria-label=黄></button>"
      "<button type=button id=br3 class=pal-r title=红 aria-label=红></button>"
      "</div>"
      "<div class=ed-erase-sz>"
      "<span id=brushLab>笔触</span>"
      "<input type=range id=brush min=1 max=40 value=6>"
      "<em id=brushV>6</em>"
      "</div>"
      "</div>"
      "<div class=ed-sep></div>"
      "<div class=ed-tool-group title=输出>"
      "<button type=button id=btnConvert class=secondary>重新抖动</button>"
      "<button type=button id=btnUpload disabled>上传</button>"
      "<button type=button id=btnSaveAs class=secondary disabled>另存</button>"
      "<button type=button id=btnCast class=secondary disabled title=投放到墨水屏>投屏</button>"
      "</div>"
      "</div>"
      "<button type=button id=btnSideReset class='secondary ed-tool-end'>复位调节</button>"
      "</div>"
      "<div class=ed-body>"
         "<div class=ed-main>"
         "<div class=ed-pair>"
         "<div class=ed-stage id=srcStage>"
         "<div class=ed-fit>"
         "<canvas id=src class=ed-can width=400 height=300></canvas>"
         "<canvas id=loupe width=96 height=96></canvas>"
         "</div></div>"
         "<div class=ed-stage id=pvStage>"
         "<div class=ed-fit>"
         "<div id=pvInner class=ed-zoom>"
         "<canvas id=preview class=ed-can width=400 height=300></canvas>"
         "<canvas id=brushCur class=ed-cur width=400 height=300></canvas>"
         "</div></div></div>"
         "<div class=ed-foot>"
         "<h3>原图</h3>"
         "<div class=ed-row style=flex:1;margin:0;min-width:120px>"
         "<span>缩放</span><input type=range id=zoom min=12 max=300 value=100 title='最小约原下限 50% 的 1/4'><em id=zoomV>100%</em>"
         "</div>"
         "</div>"
         "<div class=ed-foot>"
         "<h3>四色预览</h3>"
         "<div class=pal-sw id=fltPal title=过滤色点>"
         "<button type=button id=flt0 class=pal-k title=只看黑 aria-label=只看黑></button>"
         "<button type=button id=flt1 class=pal-w title=只看白 aria-label=只看白></button>"
         "<button type=button id=flt2 class=pal-y title=只看黄 aria-label=只看黄></button>"
         "<button type=button id=flt3 class=pal-r title=只看红 aria-label=只看红></button>"
         "</div>"
         "<p class=meta id=convInfo></p>"
         "</div></div>"
      "<div class=ed-maps>"
      "<div class=ed-map-bar>"
      "<button type=button id=btnPick class=secondary>取色</button>"
      "<button type=button id=btnClearMaps class=secondary>清空绑定</button>"
      "<div class='ed-mode ed-mixn' id=mixNBar title=混合粒度>"
      "<button type=button id=mixN1 title='1 份墨水 · 4 色'>1×1</button>"
      "<button type=button id=mixN2 title='2 份墨水 · 10 色'>1×2</button>"
      "<button type=button id=mixN3 title='3 份墨水 · 20 色'>√3</button>"
      "<button type=button id=mixN4 class=on title='4 份墨水 · 35 色'>2×2</button>"
      "<button type=button id=mixN9 title='9 份墨水 · 220 色'>3×3</button>"
      "</div>"
      "</div>"
      "<div class=ed-map-body>"
      "<div class=ed-cmap id=wireWrap>"
      "<div class='ed-cmap-edge e-tl'>黑</div>"
      "<div class='ed-cmap-edge e-tr'>白</div>"
      "<div class='ed-cmap-edge e-bl'>黄</div>"
      "<div class='ed-cmap-edge e-br'>红</div>"
      "<div class=ed-cmap-plot id=cmapPlot></div>"
      "<svg id=wires class=ed-wires></svg>"
      "</div>"
      "<div id=tolPop class=tol-pop>"
      "<span id=tolLab>容差</span>"
      "<input type=range id=tol class=tol-v min=8 max=120 value=36 orient=vertical>"
      "<em id=tolV>36</em>"
      "</div>"
      "</div>"
      "<p class=hint ed-map-hint id=mapHint>色盘正方形（左上黑 / 右上白 / 左下黄 / 右下红）。可选 1×1 / 1×2 / √3 / 2×2 / 3×3 混合粒度。初始按最近原始色匹配；点原始色后再在左图取色绑定。</p>"
      "<div id=rules class=rules></div>"
      "</div></div>"
      "<aside class=ed-side>"
      "<div class=ed-mode>"
      "<button type=button id=modeGrain class=on>颗粒</button>"
      "<button type=button id=modeSolid>色块</button>"
      "</div>"
      "<div class=ed-side-ops>"
      "<div class=ed-block>"
      "<div class=hist-wrap><canvas id=inkHist width=1024 height=160></canvas></div>"
      "<div class=ink-hist-read>"
      "<span id=ink0>黑 —</span><span id=ink1>白 —</span><span id=ink2>黄 —</span><span id=ink3>红 —</span>"
      "</div>"
      "<div class=ed-row><span>亮度</span><input type=range id=brt min=20 max=180 value=100 title=亮度><em id=brtV>100%</em></div>"
      "<div class=ed-row><span>对比</span><input type=range id=con min=20 max=220 value=100 title=对比度><em id=conV>100%</em></div>"
      "<div class=ed-row><span>伽马</span><input type=range id=gma min=0.20 max=3.00 step=0.01 value=1.00 title=伽马><em id=gmaV>1.00</em></div>"
      "<div class=ed-row><span>饱和</span><input type=range id=sat min=0 max=250 value=100 title=饱和度><em id=satV>100%</em></div>"
      "<div class=ed-row><span>色相</span><input type=range id=hue min=-180 max=180 value=0 title=色相><em id=hueV>0°</em></div>"
      "<div class=ed-row><span>色温</span><input type=range id=tmp min=-80 max=80 value=0 title='色温 冷←→暖'><em id=tmpV>0</em></div>"
      "<div class=ed-row><span>色调</span><input type=range id=tint min=-80 max=80 value=0 title='色调 绿←→品红'><em id=tintV>0</em></div>"
      "<div class=ed-row id=dithRow><span>颗粒</span><input type=range id=dith min=20 max=160 value=100 title=抖动强度><em id=dithV>100%</em></div>"
      "</div>"
      "<div class=ed-levels>"
      "<div class=ed-block>"
      "<div class=hist-wrap><canvas id=hist width=1024 height=160 tabindex=0></canvas></div>"
      "<div class=hist-read><span id=lvBlk>黑 0</span><span id=lvMid>γ 1.00</span><span id=lvWht>白 255</span></div>"
      "<div class=ed-row><span>黑场</span><input type=range id=inBlk min=0 max=254 step=1 value=0 title=输入黑场><input type=number id=inBlkN class=ed-num min=0 max=254 step=1 value=0></div>"
      "<div class=ed-row><span>中点</span><input type=range id=inMid min=1 max=254 step=1 value=128 title='输入中点（与伽马联动）'><input type=number id=inMidN class=ed-num min=1 max=254 step=1 value=128></div>"
      "<div class=ed-row><span>白场</span><input type=range id=inWht min=1 max=255 step=1 value=255 title=输入白场><input type=number id=inWhtN class=ed-num min=1 max=255 step=1 value=255></div>"
      "<div>"
      "<div class=ed-row><span>出黑</span><input type=range id=outBlk min=0 max=254 step=1 value=0 title=输出黑场><em id=outBlkV>0</em></div>"
      "<div class=ed-row><span>出白</span><input type=range id=outWht min=1 max=255 step=1 value=255 title=输出白场><em id=outWhtV>255</em></div>"
      "<div class=ed-row><span>黄墨</span><input type=range id=yel min=0 max=220 value=100 title=黄墨权重><em id=yelV>100%</em></div>"
      "<div class=ed-row><span>红墨</span><input type=range id=red min=0 max=220 value=100 title=红墨权重><em id=redV>100%</em></div>"
      "<div class=ed-row><span>灰阈</span><input type=range id=chr min=0 max=80 value=8 title='低饱和转灰阈值'><em id=chrV>8</em></div>"
      "</div>"
      "</div>"
      "</div>"
      "</div>"
      "<p class=hint ed-help>左图载入后一直是原图。调色、色块和颜色绑定只改右图。色盘是原始色；初始自动就近匹配，再点原始色去左图取色绑定。色块是不带颗粒的纯色块。空格+拖动平移；按住 Shift 只沿水平或垂直移动。</p>"
      "</aside>"
      "</div></div>"
      "<div id=toast class=ed-toast></div>"
      "<div class=ed-album>");

  b += F("<div class=ed-album-bar>");
  {
    const AppConfig &ac = getAppConfig();
    uint32_t d = 0, h = 0, m = 0;
    weatherRefreshToDhm(ac.album_refresh_sec, d, h, m);
    b += F("<div class=ed-album-panel>"
           "<h3>设备</h3>"
           "<label class=ed-sw><input type=checkbox id=showHeader");
    if (ac.album_show_header) b += F(" checked");
    b += F("><span>标题栏</span></label>"
           "<form id=albumSched method=POST action=/album/schedule class=ed-album-sched>"
           "<label class=ed-sw><input type=checkbox name=refresh_en value=1");
    if (ac.album_refresh_en) b += F(" checked");
    b += F("><span>定时翻页</span></label>"
           "<span class=ed-dhm>"
           "<label><input name=refresh_d type=number min=0 max=365 step=1 value=");
    b += String(d);
    b += F(">天</label>"
           "<label><input name=refresh_h type=number min=0 max=23 step=1 value=");
    b += String(h);
    b += F(">时</label>"
           "<label><input name=refresh_m type=number min=0 max=59 step=1 value=");
    b += String(m);
    b += F(">分</label></span>"
           "<button type=submit>保存</button></form>"
           "</div>");
  }

  b += F("<div class=ed-album-panel>"
         "<h3>管理</h3>"
         "<div class=ed-album-acts>"
         "<label class=ed-album-import><input type=file id=pickBatch accept='image/*,.2bp,application/octet-stream' multiple>导入</label>"
         "<button type=button id=btnExportAll class=secondary title=\"导出全部为 ZIP\">导出 ZIP</button>"
         "<span class=ed-sep></span>"
         "<label class='show-lab pick-lab'><input type=checkbox id=pickAll>全选</label>"
         "<button type=button id=btnDeleteSel class=danger>删除</button>"
         "<span class=ed-sep></span>"
         "<label class=show-lab><input type=checkbox id=selAll>全选展示</label>"
         "</div>"
         "</div>"
         "</div>"
         "<div id=list class=ed-saved>加载中…</div>"
         "</div></div>");
}

void webAlbumSendScript(WebServer &server) {
  // Keep JS out of Arduino String — album page is ~40KB and used to OOM into a blank page.
  static const char kJs[] = R"ALBUMJS(<script>
const W=400,H=300,FRAME=30000;
const PAL=[[0,0,0],[255,255,255],[246,199,73],[139,32,16]];
const PN=['黑','白','黄','红'];
function $(id){return document.getElementById(id)}
function val(id){return +$(id).value}
let img=null,packed=null,rules=[],mapTo=-1,selRule=-1,cover=true,tool='pan',grain=true,invert=false;
let rulesHist=[],RULES_HIST_MAX=40;
let mixN=4;
const MIX_N_LAB={1:'1×1',2:'1×2',3:'√3',4:'2×2',9:'3×3'};
const MIX_N_ALL=[1,2,3,4,9];
function cloneRulesState(){return {rules:JSON.parse(JSON.stringify(rules)),selRule:selRule,mixN:mixN};}
function pushRulesHist(){
  rulesHist.push(cloneRulesState());
  if(rulesHist.length>RULES_HIST_MAX) rulesHist.shift();
}
function undoRules(){
  if(!rulesHist.length){toast('没有可撤销');return;}
  const s=rulesHist.pop();
  if(s.mixN&&s.mixN!==mixN){
    mixN=s.mixN;
    syncMixNBtns();
    buildMixes();
  }
  rules=s.rules;selRule=s.selRule;
  if(selRule>=rules.length) selRule=-1;
  if(mapTo>=mixList.length) mapTo=-1;
  renderRules();schedule(true);toast('已撤销');
}
let mixList=[],mixLayout=[];
let drag=false,lx=0,ly=0,tmr=null,adjRgb=null,rawLum=null;
let panAxis=null,panOx=0,panOy=0,pan0x=0,pan0y=0;
let baseIdx=null,paint=new Int8Array(W*H);
let blk=0,mid=128,wht=255,outBlk=0,outWht=255,histDrag=-1,editName='',brushCol=0,loupeRight=false,visCol=-1;
let spaceHeld=false,pvDrag=false,pvx=0,pvy=0;
let cropCv=null,cropCtx=null,srcRaf=0,geomTmr=null;
function typingTarget(el){
  if(!el) return false;
  const t=el.tagName;
  return t==='INPUT'||t==='TEXTAREA'||t==='SELECT'||el.isContentEditable;
}
function canPanSrc(){return spaceHeld||tool==='pan';}
function syncCursor(){
  const src=$('src'),pv=$('preview');
  if(canPanSrc()) src.style.cursor=drag?'grabbing':'grab';
  else if(tool==='pick') src.style.cursor='crosshair';
  else src.style.cursor='default';
  if(tool==='pick') pv.style.cursor='crosshair';
  else if(tool==='erase'||tool==='brush') pv.style.cursor='none';
  else pv.style.cursor='default';
}
function nearest(r,g,b){
  let bi=0,bd=1e18;
  for(let i=0;i<4;i++){
    const p=PAL[i],d=(r-p[0])*(r-p[0])+(g-p[1])*(g-p[1])+(b-p[2])*(b-p[2]);
    if(d<bd){bd=d;bi=i;}
  }
  return bi;
}
function mixRgb(cnt){
  const n=cnt[0]+cnt[1]+cnt[2]+cnt[3]||mixN;
  let r=0,g=0,b=0;
  for(let i=0;i<4;i++){r+=PAL[i][0]*cnt[i];g+=PAL[i][1]*cnt[i];b+=PAL[i][2]*cnt[i];}
  return [Math.round(r/n),Math.round(g/n),Math.round(b/n)];
}
function gammaFromMid(){
  const t=Math.min(0.99,Math.max(0.01,(mid-blk)/Math.max(1,wht-blk)));
  return Math.log(0.5)/Math.log(t);
}
function midFromGamma(g){
  g=Math.max(0.2,Math.min(3,g));
  const t=Math.pow(0.5,1/g);
  return Math.round(blk+t*Math.max(1,wht-blk));
}
function syncGmaFromMid(){
  const el=$('gma');
  if(!el) return;
  el.value=gammaFromMid().toFixed(2);
}
function syncMidFromGma(){
  mid=Math.max(blk+1,Math.min(wht-1,midFromGamma(val('gma'))));
}
function setEm(){
  $('zoomV').textContent=val('zoom')+'%';
  $('brushV').textContent=val('brush');
  $('brtV').textContent=val('brt')+'%';
  $('conV').textContent=val('con')+'%';
  $('gmaV').textContent=(+val('gma')).toFixed(2);
  $('satV').textContent=val('sat')+'%';
  $('hueV').textContent=val('hue')+'°';
  $('tmpV').textContent=(val('tmp')>0?'+':'')+val('tmp');
  $('tintV').textContent=(val('tint')>0?'+':'')+val('tint');
  $('yelV').textContent=val('yel')+'%';
  $('redV').textContent=val('red')+'%';
  $('chrV').textContent=val('chr');
  $('dithV').textContent=val('dith')+'%';
  $('tolV').textContent=val('tol');
  $('outBlkV').textContent=outBlk;
  $('outWhtV').textContent=outWht;
  $('lvBlk').textContent='黑 '+blk;
  $('lvWht').textContent='白 '+wht;
  $('lvMid').textContent='γ '+(+val('gma')).toFixed(2);
  fillLevelInputs();
}
function fillLevelInputs(){
  $('inBlk').value=blk;$('inBlkN').value=blk;
  $('inMid').value=mid;$('inMidN').value=mid;
  $('inWht').value=wht;$('inWhtN').value=wht;
  $('outBlk').value=outBlk;$('outWht').value=outWht;
}
function toast(msg,err){
  const el=$('toast');
  if(!el) return;
  el.textContent=msg;
  el.className='ed-toast'+(err?' err':'')+' show';
  clearTimeout(el._t);
  el._t=setTimeout(function(){el.classList.remove('show');},2800);
}
function setTool(t){
  tool=t;
  $('btnPan').classList.toggle('on',t==='pan');
  $('btnErase').classList.toggle('on',t==='erase');
  $('btnPick').classList.toggle('on',t==='pick');
  $('btnBrush').classList.toggle('on',t==='brush');
  $('brushLab').textContent=t==='erase'?'橡皮':'笔触';
  if(t!=='pick') $('loupe').style.display='none';
  if(t!=='erase'&&t!=='brush') clearCursor();
  syncCursor();
}
function setBrushCol(n){
  brushCol=n;
  for(let i=0;i<4;i++) $('br'+i).classList.toggle('on',i===n);
}
function drawCropTo(ctx){
  ctx.fillStyle='#fff';ctx.fillRect(0,0,W,H);
  if(!img) return;
  const s0=cover?Math.max(W/img.width,H/img.height):Math.min(W/img.width,H/img.height);
  const s=s0*(val('zoom')/100);
  const dw=img.width*s,dh=img.height*s;
  ctx.drawImage(img,(W-dw)/2+val('panx'),(H-dh)/2+val('pany'),dw,dh);
}
function paintSourceView(){
  if(srcRaf) return;
  srcRaf=requestAnimationFrame(function(){
    srcRaf=0;
    drawCropTo($('src').getContext('2d'));
  });
}
function cropRGB(){
  if(!cropCv){
    cropCv=document.createElement('canvas');
    cropCv.width=W;cropCv.height=H;
    cropCtx=cropCv.getContext('2d',{willReadFrequently:true});
  }
  drawCropTo(cropCtx);
  const id=cropCtx.getImageData(0,0,W,H);
  const d=id.data;
  rawLum=new Uint8Array(W*H);
  for(let i=0,p=0;i<d.length;i+=4,p++){
    rawLum[p]=Math.round(0.299*d[i]+0.587*d[i+1]+0.114*d[i+2]);
  }
  return id;
}
function hue2rgb(p,q,t){
  if(t<0)t+=1;if(t>1)t-=1;
  if(t<1/6)return p+(q-p)*6*t;
  if(t<1/2)return q;
  if(t<2/3)return p+(q-p)*(2/3-t)*6;
  return p;
}
function applyPixel(r,g,b){
  const br=val('brt')/100,co=val('con')/100,sa=val('sat')/100;
  const hue=val('hue'),tmp=val('tmp'),tint=val('tint'),chTh=val('chr');
  const gma=Math.max(0.1,val('gma')||gammaFromMid());
  const yb=val('yel')/100,rb=val('red')/100;
  if(invert){r=255-r;g=255-g;b=255-b;}
  r=r*br;g=g*br;b=b*br;
  r=(r-128)*co+128;g=(g-128)*co+128;b=(b-128)*co+128;
  r+=tmp*1.15;b-=tmp*1.15;
  if(tint){r-=tint*0.85;g+=tint*1.1;b-=tint*0.85;}
  if(hue||sa!==1){
    let mx=Math.max(r,g,b),mn=Math.min(r,g,b),d=mx-mn,L=(mx+mn)/2,S=0,H=0;
    if(d>1e-3){
      S=L>127.5?d/(510-mx-mn):d/Math.max(1,mx+mn);
      if(mx===r) H=((g-b)/d+(g<b?6:0))/6;
      else if(mx===g) H=((b-r)/d+2)/6;
      else H=((r-g)/d+4)/6;
    }
    H=((H+hue/360)%1+1)%1;
    S=Math.max(0,Math.min(1,S*sa));
    const l=L/255;
    if(S<1e-4){r=g=b=L;}
    else{
      const q=l<0.5?l*(1+S):l+S-l*S,p=2*l-q;
      r=hue2rgb(p,q,H+1/3)*255;g=hue2rgb(p,q,H)*255;b=hue2rgb(p,q,H-1/3)*255;
    }
  }
  const span=Math.max(1,wht-blk);
  r=255*Math.pow(Math.max(0,Math.min(255,(r-blk)*255/span))/255,1/gma);
  g=255*Math.pow(Math.max(0,Math.min(255,(g-blk)*255/span))/255,1/gma);
  b=255*Math.pow(Math.max(0,Math.min(255,(b-blk)*255/span))/255,1/gma);
  const os=outWht-outBlk;
  r=outBlk+r*os/255;g=outBlk+g*os/255;b=outBlk+b*os/255;
  const yness=(r+g)/2-b;
  if(yness>0){
    r=Math.min(255,r+yness*(yb-1)*0.35);
    g=Math.min(255,g+yness*(yb-1)*0.35);
    b=Math.max(0,b-yness*(yb-1)*0.25);
  }
  const rness=r-(g+b)/2;
  if(rness>0){
    r=Math.min(255,r+rness*(rb-1)*0.45);
    g=Math.max(0,g-rness*(rb-1)*0.18);
    b=Math.max(0,b-rness*(rb-1)*0.18);
  }
  if(chTh>0){
    const mx=Math.max(r,g,b),mn=Math.min(r,g,b);
    if(mx-mn<chTh*2.2){
      const y=0.299*r+0.587*g+0.114*b;
      r=g=b=y;
    }
  }
  return [r,g,b];
}
function nearestMixIdx(r,g,b){
  let bi=0,bd=1e18;
  for(let i=0;i<mixList.length;i++){
    const p=mixRgb(mixList[i]);
    const d=(r-p[0])*(r-p[0])+(g-p[1])*(g-p[1])+(b-p[2])*(b-p[2]);
    if(d<bd){bd=d;bi=i;}
  }
  return bi;
}
function pureInkOf(cnt){
  for(let i=0;i<4;i++) if(cnt[i]===mixN) return i;
  return -1;
}
function syncMixNBtns(){
  MIX_N_ALL.forEach(function(n){
    const el=$('mixN'+n);
    if(el) el.classList.toggle('on', n===mixN);
  });
}
function updateMixHint(){
  const el=$('mapHint');
  if(!el) return;
  el.textContent='色盘正方形（左上黑 / 右上白 / 左下黄 / 右下红），混合 '+MIX_N_LAB[mixN]+' · '+mixList.length+' 原始色。初始按最近原始色匹配；点原始色后再在左图取色绑定，照片色排在色块周边；同一照片色再次绑定会取消先前绑定。';
}
// 规则：照片色 → 原始色（mix 下标）；无规则则返回 -1，由 nearestMixIdx 决定
function mapPixel(r,g,b){
  for(let i=0;i<rules.length;i++){
    const u=rules[i],tol=u.tol*u.tol*3;
    const d=(r-u.r)*(r-u.r)+(g-u.g)*(g-u.g)+(b-u.b)*(b-u.b);
    if(d<=tol) return u.to;
  }
  return -1;
}
function drawHist(){
  const cv=$('hist'),ctx=cv.getContext('2d');
  const w=cv.width,h=cv.height,rail=28,hh=h-rail;
  ctx.fillStyle='#fff';ctx.fillRect(0,0,w,hh);
  const bins=new Uint32Array(256);
  if(rawLum){for(let i=0;i<rawLum.length;i++) bins[rawLum[i]]++;}
  let mx=1;
  for(let i=0;i<256;i++) if(bins[i]>mx) mx=bins[i];
  const bw=w/256;
  ctx.fillStyle='#111';
  for(let i=0;i<256;i++){
    const bh=bins[i]/mx*(hh-2);
    if(bh<=0) continue;
    ctx.fillRect(Math.floor(i*bw),hh-bh,Math.max(1,Math.ceil(bw)-1),bh);
  }
  ctx.fillStyle='rgba(0,0,0,.12)';
  ctx.fillRect(0,0,blk*w/255,hh);
  ctx.fillRect(wht*w/255,0,w-wht*w/255,hh);
  ctx.strokeStyle='#888';ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(mid*w/255+0.5,0);ctx.lineTo(mid*w/255+0.5,hh);ctx.stroke();
  const grd=ctx.createLinearGradient(0,0,w,0);
  grd.addColorStop(0,'#000');grd.addColorStop(1,'#fff');
  ctx.fillStyle=grd;ctx.fillRect(0,hh,w,rail);
  ctx.strokeStyle='#111';ctx.strokeRect(0.5,hh+0.5,w-1,rail-1);
  function tri(x,fill,stroke){
    const px=Math.max(10,Math.min(w-10,x*w/255));
    ctx.beginPath();
    ctx.moveTo(px,hh+1);
    ctx.lineTo(px-10,h-2);
    ctx.lineTo(px+10,h-2);
    ctx.closePath();
    ctx.fillStyle=fill;ctx.fill();
    ctx.lineWidth=1;ctx.strokeStyle=stroke;ctx.stroke();
  }
  tri(blk,'#111','#111');
  tri(mid,'#9a9a9a','#111');
  tri(wht,'#fff','#111');
}
function drawInkHist(idx){
  const cv=$('inkHist');
  if(!cv) return;
  const dpr=Math.max(1,window.devicePixelRatio||1);
  const cssW=Math.max(1,cv.clientWidth||320);
  const cssH=160;
  if(cv.width!==Math.round(cssW*dpr)||cv.height!==Math.round(cssH*dpr)){
    cv.width=Math.round(cssW*dpr);
    cv.height=Math.round(cssH*dpr);
  }
  const ctx=cv.getContext('2d');
  ctx.setTransform(dpr,0,0,dpr,0,0);
  const padY=4;
  const barH=cssH-padY*2;
  const colW=cssW/4;
  const bw=Math.min(22,Math.max(10,colW*0.42));
  ctx.clearRect(0,0,cssW,cssH);
  ctx.fillStyle='#fff';ctx.fillRect(0,0,cssW,cssH);
  const counts=[0,0,0,0];
  const src=idx||(baseIdx?composedIdx():null);
  if(src) for(let i=0;i<src.length;i++) counts[src[i]&3]++;
  const total=Math.max(1,src?src.length:1);
  let mx=1;
  for(let i=0;i<4;i++) if(counts[i]>mx) mx=counts[i];
  for(let i=0;i<4;i++){
    const x=colW*i+(colW-bw)/2;
    const bh=src?counts[i]/mx*barH:0;
    const y=padY+barH-bh;
    const p=PAL[i];
    ctx.fillStyle='rgb('+p[0]+','+p[1]+','+p[2]+')';
    if(bh>0) ctx.fillRect(x,y,bw,bh);
    if(i===1){
      ctx.strokeStyle='#888';
      ctx.strokeRect(x+0.5,y+0.5,bw-1,Math.max(0,bh-1));
    }
    const el=$('ink'+i);
    if(el){
      const pct=src?Math.round(counts[i]*1000/total)/10:0;
      el.textContent=PN[i]+' '+(src?pct+'%':'—');
    }
  }
}
function blitIdx(idx){
  const pv=$('preview').getContext('2d');
  const pid=pv.createImageData(W,H);
  for(let i=0;i<W*H;i++){
    const o=i*4;
    if(visCol>=0&&idx[i]!==visCol){
      pid.data[o]=232;pid.data[o+1]=230;pid.data[o+2]=224;pid.data[o+3]=255;
    }else{
      const p=PAL[idx[i]];
      pid.data[o]=p[0];pid.data[o+1]=p[1];pid.data[o+2]=p[2];pid.data[o+3]=255;
    }
  }
  pv.putImageData(pid,0,0);
}
function packIdx(idx){
  const out=new Uint8Array(FRAME);
  for(let y=0;y<H;y++){
    for(let x=0;x<W;x+=4){
      let byte=0;
      for(let k=0;k<4;k++){
        const px=x+k;
        const v=px<W?idx[y*W+px]:1;
        byte|=(v&3)<<((3-k)*2);
      }
      out[y*100+(x>>2)]=byte;
    }
  }
  return out;
}
function composedIdx(){
  const idx=new Uint8Array(W*H);
  for(let i=0;i<W*H;i++){
    const p=paint[i];
    idx[i]=p>=0?p:(baseIdx?baseIdx[i]:1);
  }
  return idx;
}
function refreshFinal(){
  const idx=composedIdx();
  blitIdx(idx);
  packed=packIdx(idx);
  drawInkHist(idx);
  return idx;
}
function drawSource(){
  const src=cropRGB();
  $('src').getContext('2d').putImageData(src,0,0);
  return src;
}
function pack(dither){
  const src=drawSource();
  const d=src.data;
  adjRgb=new Uint8ClampedArray(W*H*3);
  const mixIdx=new Uint16Array(W*H);
  const qbuf=new Float32Array(W*H*3);
  for(let i=0,j=0,p=0;i<d.length;i+=4,j+=3,p++){
    adjRgb[j]=d[i];adjRgb[j+1]=d[i+1];adjRgb[j+2]=d[i+2];
    const rgb=applyPixel(d[i],d[i+1],d[i+2]);
    let mi=mapPixel(d[i],d[i+1],d[i+2]);
    if(mi<0||mi>=mixList.length) mi=nearestMixIdx(rgb[0],rgb[1],rgb[2]);
    mixIdx[p]=mi;
    const mr=mixRgb(mixList[mi]);
    qbuf[j]=mr[0];qbuf[j+1]=mr[1];qbuf[j+2]=mr[2];
  }
  const idx=new Uint8Array(W*H);
  for(let y=0;y<H;y++){
    for(let x=0;x<W;x++){
      const p=y*W+x,o=p*3;
      const cnt=mixList[mixIdx[p]];
      const ink=pureInkOf(cnt);
      if(ink>=0){
        idx[p]=ink;
        continue;
      }
      let r=qbuf[o],g=qbuf[o+1],b=qbuf[o+2];
      const i=nearest(r,g,b);
      idx[p]=i;
      if(!dither||!grain) continue;
      const pal=PAL[i];
      const fac=val('dith')/100;
      const er=(r-pal[0])*fac,eg=(g-pal[1])*fac,eb=(b-pal[2])*fac;
      const spread=function(dx,dy,fac){
        const xx=x+dx,yy=y+dy;
        if(xx<0||xx>=W||yy<0||yy>=H)return;
        if(pureInkOf(mixList[mixIdx[yy*W+xx]])>=0)return;
        const t=(yy*W+xx)*3;
        qbuf[t]+=er*fac;qbuf[t+1]+=eg*fac;qbuf[t+2]+=eb*fac;
      };
      spread(1,0,7/16);spread(-1,1,3/16);spread(0,1,5/16);spread(1,1,1/16);
    }
  }
  baseIdx=idx;
  drawHist();
  refreshFinal();
  return packed;
}
function run(dither){
  setEm();
  if(!img){$('convInfo').textContent='先选一张图';return;}
  const useGrain=grain&&!!dither;
  pack(useGrain);
  $('btnUpload').disabled=false;
  $('btnSaveAs').disabled=false;
  $('btnCast').disabled=false;
  $('btnUpload').textContent=editName?'覆盖':'上传';
  $('convInfo').textContent=(grain?'颗粒 ':'色块 ')+(editName?editName:'新照片');
}
function schedule(final){
  clearTimeout(tmr);
  clearTimeout(geomTmr);
  // 拖动滑块时也按当前模式预览（颗粒=抖动，色块=最近色）
  tmr=setTimeout(function(){run(grain);}, (final||grain)?120:40);
}
// 平移/缩放：左侧立刻重绘；右侧轻量防抖，松手再按模式出最终预览
function scheduleGeom(final){
  clearTimeout(tmr);
  clearTimeout(geomTmr);
  paintSourceView();
  if(final){
    geomTmr=setTimeout(function(){run(grain);}, 16);
    return;
  }
  geomTmr=setTimeout(function(){run(false);}, 90);
}
function mixLabel(cnt){
  const parts=[];
  for(let i=0;i<4;i++) if(cnt[i]) parts.push(PN[i]+'×'+cnt[i]);
  return parts.join(' ')||'原始色';
}
function sameRgb(u,r,g,b){return u.r===r&&u.g===g&&u.b===b;}
function origName(mi){
  if(mi<0||mi>=mixList.length) return '原始色';
  const ink=pureInkOf(mixList[mi]);
  if(ink>=0) return PN[ink];
  return mixLabel(mixList[mi]);
}
function mixPos(cnt){
  // 正方形：左上黑、右上白、左下黄、右下红
  const n=mixN||4;
  return {x:(cnt[1]+cnt[3])/n, y:(cnt[2]+cnt[3])/n};
}
function posStyle(x,y,ox,oy){
  ox=ox||0;oy=oy||0;
  const m=6;
  return 'left:calc('+(m+x*(100-2*m))+'% + '+ox+'px);top:calc('+(m+y*(100-2*m))+'% + '+oy+'px)';
}
function layoutStyle(mi,ox,oy){
  const L=mixLayout[mi];
  if(!L) return posStyle(0,0,ox,oy);
  return posStyle(L.x,L.y,L.ox+(ox||0),L.oy+(oy||0));
}
// 绑定照片色均匀排在原始色块周边
function chipOrbit(mi,k,n){
  const pure=pureInkOf(mixList[mi])>=0;
  const R=pure?22:18;
  const ang=-Math.PI/2+(Math.PI*2)*k/Math.max(1,n);
  return {ox:Math.cos(ang)*R, oy:Math.sin(ang)*R};
}
function swatchEl(mi){
  return document.querySelector('#cmapPlot [data-m="'+mi+'"]');
}
function paintMixes(){
  document.querySelectorAll('#cmapPlot [data-m]').forEach(function(btn){
    const mi=+btn.dataset.m;
    const hit=rules.findIndex(function(u){return u.to===mi;});
    btn.classList.toggle('on', hit>=0);
    btn.classList.toggle('arm', mi===mapTo);
    btn.classList.toggle('sel', selRule>=0&&rules[selRule]&&rules[selRule].to===mi);
  });
  document.querySelectorAll('#cmapPlot [data-c]').forEach(function(btn){
    const i=+btn.dataset.c;
    btn.classList.toggle('on', true);
    btn.classList.toggle('sel', i===selRule);
  });
}
function syncTolHeight(){
  const pop=$('tolPop'),wrap=$('wireWrap');
  if(!pop||!wrap) return;
  pop.style.height=wrap.offsetHeight+'px';
}
function syncTolPop(){
  const pop=$('tolPop');
  if(!pop) return;
  if(selRule>=0&&rules[selRule]){
    const u=rules[selRule];
    $('tol').value=u.tol;
    $('tolLab').textContent=origName(u.to)+' ← 照片';
  }else if(mapTo>=0){
    $('tolLab').textContent=origName(mapTo);
  }else{
    $('tolLab').textContent='容差';
  }
  setEm();
  requestAnimationFrame(function(){syncTolHeight();drawWires();});
}
function drawWires(){
  const wrap=$('wireWrap'),svg=$('wires');
  if(!wrap||!svg) return;
  syncTolHeight();
  const wr=wrap.getBoundingClientRect();
  svg.setAttribute('width',Math.max(1,wr.width));
  svg.setAttribute('height',Math.max(1,wr.height));
  // 绑定点已贴在色块周边，不再画长连线
  svg.innerHTML='';
}
function renderRules(){
  const plot=$('cmapPlot');
  if(!plot) return;
  document.querySelectorAll('#cmapPlot [data-c]').forEach(function(el){el.remove();});
  const byTo={};
  rules.forEach(function(u,i){
    (byTo[u.to]=byTo[u.to]||[]).push(i);
  });
  Object.keys(byTo).forEach(function(key){
    const list=byTo[key],mi=+key,n=list.length;
    list.forEach(function(ri,k){
      const u=rules[ri];
      const btn=document.createElement('button');
      btn.type='button';
      btn.className='sw-drop';
      btn.dataset.c=String(ri);
      const rgb=[Math.round(u.r),Math.round(u.g),Math.round(u.b)];
      btn.title='照片 '+rgb.join(',')+' ±'+u.tol+' → '+origName(mi);
      const off=chipOrbit(mi,k,n);
      const ring=mixRgb(mixList[mi]||[0,0,0,mixN]);
      btn.style.cssText=layoutStyle(mi,off.ox,off.oy)+';background:rgb('+rgb[0]+','+rgb[1]+','+rgb[2]+');border-color:rgb('+ring[0]+','+ring[1]+','+ring[2]+')';
      plot.appendChild(btn);
    });
  });
  paintMixes();
  syncTolPop();
  requestAnimationFrame(drawWires);
}
function selectRule(i,force){
  selRule=(!force && selRule===i)?-1:i;
  if(selRule>=0 && rules[selRule]) mapTo=rules[selRule].to;
  renderRules();
}
function colorDist2(r,g,b,u){
  return (r-u.r)*(r-u.r)+(g-u.g)*(g-u.g)+(b-u.b)*(b-u.b);
}
function addOrSelectRule(r,g,b){
  if(mapTo<0||mapTo>=mixList.length){toast('先点色盘上的原始颜色');return;}
  r=Math.round(r);g=Math.round(g);b=Math.round(b);
  const tol=val('tol'),tol2=tol*tol*3;
  // 已绑到同一原始色且颜色相同：只选中
  const same=rules.findIndex(function(u){return u.to===mapTo&&sameRgb(u,r,g,b);});
  if(same>=0){selRule=same;renderRules();return;}
  pushRulesHist();
  // 后绑覆盖：相同或容差内相近的照片色取消先前绑定
  for(let i=rules.length-1;i>=0;i--){
    const u=rules[i];
    const lim=Math.max(tol2,u.tol*u.tol*3);
    if(colorDist2(r,g,b,u)<=lim) rules.splice(i,1);
  }
  rules.push({r:r,g:g,b:b,tol:tol,to:mapTo});
  selRule=rules.length-1;
  renderRules();
  schedule(true);
}
function setMapTo(n){
  mapTo=n;
  selRule=-1;
  paintMixes();
  syncTolPop();
  if(n>=0&&n<mixList.length) setTool('pick');
}
function setMixN(n){
  n=+n;
  if(MIX_N_ALL.indexOf(n)<0) return;
  if(n===mixN) return;
  const prevTargets=rules.map(function(u){
    if(u.to>=0&&u.to<mixList.length) return mixRgb(mixList[u.to]);
    return [u.r,u.g,u.b];
  });
  pushRulesHist();
  mixN=n;
  mapTo=-1;selRule=-1;
  syncMixNBtns();
  buildMixes();
  rules.forEach(function(u,i){
    const t=prevTargets[i];
    u.to=nearestMixIdx(t[0],t[1],t[2]);
  });
  renderRules();
  schedule(true);
  toast('混合 '+MIX_N_LAB[mixN]+' · '+mixList.length+' 原始色');
}
function buildMixes(){
  mixList=[];
  for(let a=0;a<=mixN;a++)
    for(let b=0;b<=mixN-a;b++)
      for(let c=0;c<=mixN-a-b;c++)
        mixList.push([a,b,c,mixN-a-b-c]);
  mixLayout=new Array(mixList.length);
  const dense=mixN>=9;
  const wrap=$('wireWrap');
  if(wrap) wrap.classList.toggle('mix-dense',dense);
  const groups={};
  mixList.forEach(function(cnt,i){
    const p=mixPos(cnt);
    const key=Math.round(p.x*mixN)+','+Math.round(p.y*mixN);
    (groups[key]=groups[key]||[]).push({i:i,cnt:cnt,p:p});
  });
  Object.keys(groups).forEach(function(key){
    const g=groups[key];
    g.forEach(function(it,gi){
      let ox=0,oy=0;
      if(g.length>1){
        const ang=gi*(Math.PI*2)/g.length-Math.PI/2;
        const rad=dense?(g.length<=2?8:10):(g.length<=2?14:16);
        ox=Math.cos(ang)*rad;
        oy=Math.sin(ang)*rad;
      }
      mixLayout[it.i]={x:it.p.x,y:it.p.y,ox:ox,oy:oy};
    });
  });
  const pureCls=['pal-k','pal-w','pal-y','pal-r'];
  let html='';
  mixList.forEach(function(cnt,i){
    const rgb=mixRgb(cnt);
    const sty=layoutStyle(i,0,0);
    const ink=pureInkOf(cnt);
    if(ink>=0){
      html+='<button type=button class="sw-pure '+pureCls[ink]+'" data-m="'+i+'" style="'+sty+'" title="'+PN[ink]+'" aria-label="'+PN[ink]+'"></button>';
    }else{
      html+='<button type=button class=sw-mix data-m="'+i+'" style="'+sty+';background:rgb('+rgb[0]+','+rgb[1]+','+rgb[2]+')" title="'+mixLabel(cnt)+'"></button>';
    }
  });
  $('cmapPlot').innerHTML=html;
  if(!$('wireWrap')._cmapClick){
    $('wireWrap')._cmapClick=true;
    $('wireWrap').addEventListener('click',function(e){
      const mix=e.target.closest('[data-m]');
      if(mix){setMapTo(+mix.dataset.m);return;}
      const chip=e.target.closest('[data-c]');
      if(chip) selectRule(+chip.dataset.c,true);
    });
  }
  updateMixHint();
  paintMixes();
}
function setVisCol(n){
  visCol=(visCol===n)?-1:n;
  for(let i=0;i<4;i++) $('flt'+i).classList.toggle('on', visCol===i);
  if(baseIdx) refreshFinal();
}
function canvasXY(cv,e){
  const r=cv.getBoundingClientRect();
  const x=Math.max(0,Math.min(W-1,Math.floor((e.clientX-r.left)*W/r.width)));
  const y=Math.max(0,Math.min(H-1,Math.floor((e.clientY-r.top)*H/r.height)));
  return [x,y, (e.clientX-r.left)/r.width, (e.clientY-r.top)/r.height];
}
function pointerInCanvas(cv,e){
  const r=cv.getBoundingClientRect();
  return e.clientX>=r.left&&e.clientX<r.right&&e.clientY>=r.top&&e.clientY<r.bottom;
}
function stampPaint(cx,cy,r,col){
  const rr=r*r;
  for(let y=cy-r;y<=cy+r;y++){
    for(let x=cx-r;x<=cx+r;x++){
      if(x<0||y<0||x>=W||y>=H) continue;
      if((x-cx)*(x-cx)+(y-cy)*(y-cy)<=rr) paint[y*W+x]=col;
    }
  }
}
function placeLoupe(nx,ny){
  const el=$('loupe');
  const stage=el.parentElement.getBoundingClientRect();
  const coverX=(96+16)/Math.max(1,stage.width);
  const coverY=(96+16)/Math.max(1,stage.height);
  const hitL=nx<coverX && ny<coverY;
  const hitR=nx>1-coverX && ny<coverY;
  if(hitL && !hitR) loupeRight=true;
  else if(hitR && !hitL) loupeRight=false;
  el.style.display='block';
  el.style.top='8px';el.style.bottom='auto';
  el.style.left=loupeRight?'auto':'8px';
  el.style.right=loupeRight?'8px':'auto';
}
function drawLoupe(px,py){
  if(!adjRgb) return;
  const cv=$('loupe'),ctx=cv.getContext('2d');
  const mag=8,rad=6;
  ctx.imageSmoothingEnabled=false;
  ctx.fillStyle='#111';ctx.fillRect(0,0,96,96);
  for(let dy=-rad;dy<=rad;dy++){
    for(let dx=-rad;dx<=rad;dx++){
      const x=px+dx,y=py+dy;
      let r=0,g=0,b=0;
      if(x>=0&&y>=0&&x<W&&y<H){
        const j=(y*W+x)*3;
        r=adjRgb[j];g=adjRgb[j+1];b=adjRgb[j+2];
      }
      ctx.fillStyle='rgb('+r+','+g+','+b+')';
      ctx.fillRect((dx+rad)*mag,(dy+rad)*mag,mag,mag);
    }
  }
  ctx.strokeStyle='#22c55e';ctx.lineWidth=2;
  ctx.strokeRect(rad*mag+0.5,rad*mag+0.5,mag-1,mag-1);
  const j=(py*W+px)*3;
  ctx.fillStyle='#fff';ctx.font='10px sans-serif';
  ctx.fillText(Math.round(adjRgb[j])+' '+Math.round(adjRgb[j+1])+' '+Math.round(adjRgb[j+2]),4,94);
}
function clearCursor(){
  const ctx=$('brushCur').getContext('2d');
  ctx.clearRect(0,0,W,H);
}
function drawBrushCursor(px,py){
  const ctx=$('brushCur').getContext('2d');
  ctx.clearRect(0,0,W,H);
  const r=Math.max(0.5,val('brush'));
  ctx.beginPath();ctx.arc(px+0.5,py+0.5,r,0,Math.PI*2);
  ctx.strokeStyle='#111';ctx.lineWidth=1;ctx.stroke();
  ctx.beginPath();ctx.arc(px+0.5,py+0.5,r,0,Math.PI*2);
  ctx.strokeStyle='#fff';ctx.setLineDash([2,2]);ctx.stroke();ctx.setLineDash([]);
}
const srcCv=$('src');
srcCv.onpointerdown=function(e){
  srcCv.setPointerCapture(e.pointerId);
  const xy=canvasXY(srcCv,e);
  if(canPanSrc()){
    if(!img) return;
    drag=true;lx=e.clientX;ly=e.clientY;
    panOx=e.clientX;panOy=e.clientY;
    pan0x=val('panx');pan0y=val('pany');
    panAxis=null;
    syncCursor();return;
  }
  if(tool==='pick'){
    if(!adjRgb) return;
    const j=(xy[1]*W+xy[0])*3;
    addOrSelectRule(adjRgb[j],adjRgb[j+1],adjRgb[j+2]);return;
  }
};
srcCv.onpointermove=function(e){
  const xy=canvasXY(srcCv,e);
  if(tool==='pick'&&!canPanSrc()){placeLoupe(xy[2],xy[3]);drawLoupe(xy[0],xy[1]);return;}
  if(!drag||!img||!canPanSrc()) return;
  const r=srcCv.getBoundingClientRect();
  if(e.shiftKey){
    const cdx=e.clientX-panOx,cdy=e.clientY-panOy;
    if(!panAxis&&(Math.abs(cdx)>4||Math.abs(cdy)>4))
      panAxis=Math.abs(cdx)>=Math.abs(cdy)?'x':'y';
    if(panAxis==='x'){
      $('panx').value=pan0x+cdx*W/r.width;
      $('pany').value=pan0y;
    }else if(panAxis==='y'){
      $('panx').value=pan0x;
      $('pany').value=pan0y+cdy*H/r.height;
    }
  }else{
    if(panAxis){
      panAxis=null;
      panOx=e.clientX;panOy=e.clientY;
      pan0x=val('panx');pan0y=val('pany');
    }
    $('panx').value=val('panx')+(e.clientX-lx)*W/r.width;
    $('pany').value=val('pany')+(e.clientY-ly)*H/r.height;
    panOx=e.clientX;panOy=e.clientY;
    pan0x=val('panx');pan0y=val('pany');
  }
  lx=e.clientX;ly=e.clientY;scheduleGeom(false);
};
srcCv.onpointerup=function(){if(drag){drag=false;panAxis=null;syncCursor();scheduleGeom(true);}};
srcCv.onpointercancel=function(){if(drag){drag=false;panAxis=null;syncCursor();scheduleGeom(true);}else syncCursor();};
srcCv.onpointerleave=function(){if(tool==='pick'&&!spaceHeld) $('loupe').style.display='none';};
const pvCv=$('preview');
const pvBox=$('pvInner').parentElement;
function paintAt(e){
  const xy=canvasXY(pvCv,e);
  stampPaint(xy[0],xy[1],val('brush'),tool==='erase'?1:brushCol);
  refreshFinal();
}
function endBrushStroke(e){
  if(!drag||(tool!=='erase'&&tool!=='brush')) return;
  drag=false;
  if(e&&e.pointerId!=null){
    try{pvCv.releasePointerCapture(e.pointerId);}catch(_){}
  }
  clearCursor();
  syncCursor();
}
pvCv.onpointerdown=function(e){
  if(tool==='pick'){
    if(!adjRgb) return;
    const xy=canvasXY(pvCv,e);
    const j=(xy[1]*W+xy[0])*3;
    addOrSelectRule(adjRgb[j],adjRgb[j+1],adjRgb[j+2]);
    return;
  }
  if(tool!=='erase'&&tool!=='brush') return;
  pvCv.setPointerCapture(e.pointerId);drag=true;paintAt(e);
};
pvCv.onpointermove=function(e){
  if(pvDrag){
    pvBox.scrollLeft-=(e.clientX-pvx);
    pvBox.scrollTop-=(e.clientY-pvy);
    pvx=e.clientX;pvy=e.clientY;return;
  }
  if(drag&&(tool==='erase'||tool==='brush')&&!pointerInCanvas(pvCv,e)){
    endBrushStroke(e);
    return;
  }
  const xy=canvasXY(pvCv,e);
  if(tool==='erase'||tool==='brush') drawBrushCursor(xy[0],xy[1]);
  if(drag&&(tool==='erase'||tool==='brush')) paintAt(e);
};
pvCv.onpointerup=function(e){pvDrag=false;endBrushStroke(e);drag=false;syncCursor();};
pvCv.onpointercancel=function(e){pvDrag=false;endBrushStroke(e);drag=false;syncCursor();};
pvCv.onpointerleave=function(e){clearCursor();endBrushStroke(e);};
function forwardWheel(e){
  e.preventDefault();
  e.stopPropagation();
  const dy=e.deltaMode===1?e.deltaY*16:(e.deltaMode===2?e.deltaY*innerHeight:e.deltaY);
  const dx=e.deltaMode===1?e.deltaX*16:(e.deltaMode===2?e.deltaX*innerWidth:e.deltaX);
  scrollBy(dx,dy);
}
['srcStage','pvStage'].forEach(function(id){
  const el=$(id);
  if(!el) return;
  el.addEventListener('wheel',forwardWheel,{passive:false,capture:true});
});
window.addEventListener('keydown',function(e){
  if(e.code==='Space'||e.key===' '){
    if(typingTarget(e.target)) return;
    e.preventDefault();
    if(!spaceHeld){spaceHeld=true;syncCursor();}
    return;
  }
  if((e.ctrlKey||e.metaKey)&&(e.key==='z'||e.key==='Z')&&!e.shiftKey){
    if(typingTarget(e.target)) return;
    e.preventDefault();
    undoRules();
    return;
  }
  if((e.key==='Backspace'||e.key==='Delete')&&selRule>=0){
    if(typingTarget(e.target)) return;
    e.preventDefault();
    pushRulesHist();
    rules.splice(selRule,1);
    selRule=-1;
    renderRules();
    schedule(true);
  }
});
window.addEventListener('keyup',function(e){
  if(e.code!=='Space'&&e.key!==' ') return;
  spaceHeld=false;
  if(drag){drag=false;scheduleGeom(true);}
  pvDrag=false;
  syncCursor();
});
window.addEventListener('blur',function(){spaceHeld=false;pvDrag=false;syncCursor();});
function histX(e){
  const r=$('hist').getBoundingClientRect();
  return Math.max(0,Math.min(255,Math.round((e.clientX-r.left)*255/r.width)));
}
function endHistDrag(e){
  if(histDrag<0) return;
  histDrag=-1;
  if(e&&e.pointerId!=null){
    try{$('hist').releasePointerCapture(e.pointerId);}catch(err){}
  }
  schedule(true);
}
function histPointerOutside(e){
  const r=$('hist').getBoundingClientRect();
  return e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom;
}
$('hist').onpointerdown=function(e){
  $('hist').setPointerCapture(e.pointerId);
  const x=histX(e);
  const ds=[Math.abs(x-blk),Math.abs(x-mid),Math.abs(x-wht)];
  histDrag=0;
  if(ds[1]<ds[0]) histDrag=1;
  if(ds[2]<ds[histDrag]) histDrag=2;
};
$('hist').onpointermove=function(e){
  if(histDrag<0) return;
  // 拖出色阶区域：取消按下
  if(histPointerOutside(e)){endHistDrag(e);return;}
  let x=histX(e);
  if(histDrag===0){blk=Math.min(x,mid-1); if(blk<0) blk=0;}
  if(histDrag===1){mid=Math.max(blk+1,Math.min(wht-1,x));}
  if(histDrag===2){wht=Math.max(x,mid+1); if(wht>255) wht=255;}
  syncGmaFromMid();
  setEm();drawHist();schedule(false);
};
$('hist').onpointerup=function(e){endHistDrag(e);};
$('hist').onpointercancel=function(e){endHistDrag(e);};
$('hist').onpointerleave=function(e){if(histDrag>=0) endHistDrag(e);};
$('hist').onkeydown=function(e){
  const d=(e.key==='ArrowRight'||e.key==='ArrowUp')?1:((e.key==='ArrowLeft'||e.key==='ArrowDown')?-1:0);
  if(!d) return;
  e.preventDefault();
  const t=e.shiftKey?2:(e.altKey?0:1);
  if(t===0) blk=Math.max(0,Math.min(mid-1,blk+d));
  if(t===1) mid=Math.max(blk+1,Math.min(wht-1,mid+d));
  if(t===2) wht=Math.max(mid+1,Math.min(255,wht+d));
  syncGmaFromMid();
  setEm();drawHist();schedule(false);
};
$('hist').onkeyup=function(e){
  if(e.key.indexOf('Arrow')===0) schedule(true);
};
function bindLevel(rangeId,numId,apply){
  function go(v,live){apply(+v);setEm();drawHist();schedule(live);}
  $(rangeId).oninput=function(){go(this.value,false);};
  $(rangeId).onchange=function(){go(this.value,true);};
  $(numId).oninput=function(){go(this.value,false);};
  $(numId).onchange=function(){go(this.value,true);};
}
bindLevel('inBlk','inBlkN',function(v){blk=Math.max(0,Math.min(mid-1,v|0));syncGmaFromMid();});
bindLevel('inMid','inMidN',function(v){mid=Math.max(blk+1,Math.min(wht-1,v|0));syncGmaFromMid();});
bindLevel('inWht','inWhtN',function(v){wht=Math.max(mid+1,Math.min(255,v|0));syncGmaFromMid();});
['outBlk','outWht'].forEach(function(id){
  $(id).oninput=function(){
    outBlk=Math.max(0,Math.min(254,val('outBlk')));
    outWht=Math.max(outBlk+1,Math.min(255,val('outWht')));
    setEm();schedule(false);
  };
  $(id).onchange=function(){schedule(true);};
});
['zoom','yel','red','brush','brt','con','sat','hue','tmp','tint','chr','dith'].forEach(function(id){
  $(id).oninput=function(){
    setEm();
    if(id==='brush') return;
    if(id==='zoom'){scheduleGeom(false);return;}
    schedule(false);
  };
  $(id).onchange=function(){
    if(id==='brush') return;
    if(id==='zoom'){scheduleGeom(true);return;}
    schedule(true);
  };
});
$('gma').oninput=function(){
  syncMidFromGma();
  setEm();drawHist();schedule(false);
};
$('gma').onchange=function(){
  syncMidFromGma();
  setEm();drawHist();schedule(true);
};
$('tol').oninput=function(){
  setEm();
  if(selRule>=0 && rules[selRule]){
    rules[selRule].tol=val('tol');
    renderRules();
    schedule(false);
  }
};
$('tol').onchange=function(){
  if(selRule>=0 && rules[selRule]){
    rules[selRule].tol=val('tol');
    renderRules();
    schedule(true);
  }
};
const SLIDER_DEF={
  zoom:100,brush:6,brt:100,con:100,gma:1,sat:100,hue:0,tmp:0,tint:0,
  yel:100,red:100,chr:8,dith:100,tol:36,
  inBlk:0,inMid:128,inWht:255,outBlk:0,outWht:255
};
function applySliderDefault(id){
  const d=SLIDER_DEF[id];
  if(d==null||!$(id)) return;
  $(id).value=d;
  if(id==='inBlk'){blk=Math.max(0,Math.min(mid-1,d));syncGmaFromMid();fillLevelInputs();}
  else if(id==='inMid'){mid=Math.max(blk+1,Math.min(wht-1,d));syncGmaFromMid();fillLevelInputs();}
  else if(id==='inWht'){wht=Math.max(mid+1,Math.min(255,d));syncGmaFromMid();fillLevelInputs();}
  else if(id==='gma'){syncMidFromGma();fillLevelInputs();}
  else if(id==='outBlk'){outBlk=d;if(outWht<=outBlk) outWht=Math.min(255,outBlk+1);fillLevelInputs();}
  else if(id==='outWht'){outWht=d;if(outWht<=outBlk) outBlk=Math.max(0,outWht-1);fillLevelInputs();}
  else if(id==='tol'&&selRule>=0&&rules[selRule]){rules[selRule].tol=d;renderRules();}
  setEm();
  if(id==='brush') return;
  if(id==='zoom'){scheduleGeom(true);return;}
  if(id==='inBlk'||id==='inMid'||id==='inWht'||id==='gma') drawHist();
  schedule(true);
}
Object.keys(SLIDER_DEF).forEach(function(id){
  const el=$(id);
  if(!el) return;
  el.title=(el.title?el.title+' · ':'')+'双击复位';
  el.addEventListener('dblclick',function(e){
    e.preventDefault();
    applySliderDefault(id);
  });
});
function setGrain(on,skip){
  grain=!!on;
  $('modeGrain').classList.toggle('on',grain);
  $('modeSolid').classList.toggle('on',!grain);
  $('dithRow').style.display=grain?'':'none';
  if(!skip&&img) schedule(true);
}
$('modeGrain').onclick=function(){setGrain(true);};
$('modeSolid').onclick=function(){setGrain(false);};
$('btnPan').onclick=function(){setTool('pan');};
$('btnErase').onclick=function(){setTool('erase');};
$('btnBrush').onclick=function(){setTool('brush');};
$('btnPick').onclick=function(){setTool(tool==='pick'?'pan':'pick');};
$('btnFit').onclick=function(){cover=true;schedule(true);};
$('btnContain').onclick=function(){cover=false;schedule(true);};
function setInvert(on,skip){
  invert=!!on;
  $('btnInvert').classList.toggle('on',invert);
  if(!skip&&img) schedule(true);
}
$('btnInvert').onclick=function(){setInvert(!invert);};
function resetView(){
  $('zoom').value=100;$('panx').value=0;$('pany').value=0;cover=true;
  paint=new Int8Array(W*H);paint.fill(-1);
}
function resetSide(){
  blk=0;mid=128;wht=255;outBlk=0;outWht=255;
  $('brt').value=100;$('con').value=100;$('gma').value=1;$('sat').value=100;
  $('hue').value=0;$('tmp').value=0;$('tint').value=0;
  $('yel').value=100;$('red').value=100;$('chr').value=8;$('dith').value=100;$('tol').value=36;
  setInvert(false,true);
  rules=[];selRule=-1;setMapTo(-1);renderRules();setGrain(true,true);fillLevelInputs();
}
$('btnReset').onclick=function(){resetView();schedule(true);};
$('btnSideReset').onclick=function(){resetSide();schedule(true);};
$('btnClearMaps').onclick=function(){if(!rules.length)return;pushRulesHist();rules=[];selRule=-1;renderRules();schedule(true);};
MIX_N_ALL.forEach(function(n){
  const el=$('mixN'+n);
  if(el) el.onclick=function(){setMixN(n);};
});
for(let i=0;i<4;i++) $('br'+i).onclick=function(){setBrushCol(+this.id.slice(2));setTool('brush');};
for(let i=0;i<4;i++) $('flt'+i).onclick=function(){setVisCol(+this.id.slice(3));};
function loadImage(im,name,isSaved){
  img=im;editName=isSaved?name:'';
  resetView();resetSide();
  const fn=$('fileName');
  fn.classList.remove('empty');
  fn.textContent=name+(im.width?('  '+im.width+'×'+im.height):'');
  run(true);
}
// 无损载入设备帧：不经 PNG/抖动，像素与板上一致
function loadFrom2bp(buf,name,isSaved){
  if(buf.length!==FRAME) throw new Error('尺寸不对（需要 '+FRAME+' 字节 .2bp）');
  const idx=decode2bp(buf);
  baseIdx=idx;
  paint=new Int8Array(W*H);
  for(let i=0;i<paint.length;i++) paint[i]=-1;
  packed=buf instanceof Uint8Array?new Uint8Array(buf):new Uint8Array(buf);
  editName=isSaved?name:'';
  resetView();resetSide();
  const c=idxToCanvas(idx);
  const im=new Image();
  im.onload=function(){
    img=im;
    $('src').getContext('2d').drawImage(im,0,0);
    blitIdx(composedIdx());
    drawInkHist(composedIdx());
    const fn=$('fileName');
    fn.classList.remove('empty');
    fn.textContent=(name||'frame.2bp')+'  400×300 · 无损';
    $('btnUpload').disabled=false;
    $('btnSaveAs').disabled=false;
    $('btnCast').disabled=false;
    $('btnUpload').textContent=editName?'覆盖':'上传';
    $('convInfo').textContent='已载入 .2bp'+(editName?(' '+editName):'')+'（未重新抖动）';
    toast('已载入 .2bp');
  };
  im.src=c.toDataURL('image/png');
}
function readFileBuf(f){
  return new Promise(function(res,rej){
    const rd=new FileReader();
    rd.onload=function(){res(rd.result);};
    rd.onerror=function(){rej(new Error('读取失败'));};
    rd.readAsArrayBuffer(f);
  });
}
function loadImageFromFile(f){
  return new Promise(function(res,rej){
    const url=URL.createObjectURL(f);
    const im=new Image();
    im.onload=function(){URL.revokeObjectURL(url);res(im);};
    im.onerror=function(){URL.revokeObjectURL(url);rej(new Error('图片加载失败'));};
    im.src=url;
  });
}
function is2bpFile(f){
  return /\.2bp$/i.test(f.name)||f.type==='application/octet-stream'||f.size===FRAME;
}
async function uploadBytes(bytes,hint){
  const fd=new FormData();
  fd.append('file',new Blob([bytes],{type:'application/octet-stream'}),hint||'photo.2bp');
  const r=await fetch('/album/upload',{method:'POST',body:fd});
  const t=await r.text();
  if(!r.ok) throw new Error(t||String(r.status));
  return t;
}
async function openOneFile(f){
  if(is2bpFile(f)){
    const buf=new Uint8Array(await readFileBuf(f));
    loadFrom2bp(buf,f.name,/\.2bp$/i.test(f.name));
    return;
  }
  const im=await loadImageFromFile(f);
  loadImage(im,f.name,false);
}
async function batchImport(files){
  let ok=0,fail=0,lastSaved='',stopped=false;
  // 保留侧栏色阶；批量时不用上一张的取色绑定
  rules=[];selRule=-1;setMapTo(-1);renderRules();
  $('btnUpload').disabled=true;
  $('btnSaveAs').disabled=true;
  $('btnCast').disabled=true;
  for(let i=0;i<files.length;i++){
    const f=files[i];
    const tag=(i+1)+'/'+files.length;
    $('fileName').classList.remove('empty');
    $('fileName').textContent='批量 '+tag+' · '+f.name;
    $('convInfo').textContent='批量导入 '+tag+'…';
    toast('批量导入 '+tag+'…');
    try{
      let saved='';
      if(is2bpFile(f)){
        const buf=new Uint8Array(await readFileBuf(f));
        if(buf.length!==FRAME) throw new Error('尺寸不对');
        saved=await uploadBytes(buf,/\.2bp$/i.test(f.name)?f.name:'photo.2bp');
        if(i===files.length-1) loadFrom2bp(buf,saved,true);
      }else{
        const im=await loadImageFromFile(f);
        img=im;editName='';
        resetView();
        pack(!!grain);
        saved=await uploadBytes(packed,'photo.2bp');
        if(i===files.length-1){
          editName=saved;
          const fn=$('fileName');
          fn.classList.remove('empty');
          fn.textContent=saved+' · '+f.name+(im.width?('  '+im.width+'×'+im.height):'');
          $('btnUpload').textContent='覆盖';
          $('convInfo').textContent=(grain?'颗粒 ':'色块 ')+saved;
        }
      }
      lastSaved=saved;
      ok++;
    }catch(e){
      fail++;
      const msg=String(e);
      toast('失败 '+f.name+'：'+msg,true);
      // 存储满等硬错误时停止后续，避免连续失败
      if(/full|space|fail|失败|no space|ENOSPC/i.test(msg)){stopped=true;break;}
    }
  }
  if(lastSaved&&!editName) editName=lastSaved;
  $('btnUpload').disabled=!packed;
  $('btnSaveAs').disabled=!packed;
  $('btnCast').disabled=!packed;
  if(packed) $('btnUpload').textContent=editName?'覆盖':'上传';
  await refreshList();
  const more=stopped?(' · 已中止，余下 '+(files.length-ok-fail)+' 未传'):'';
  const msg='批量完成：成功 '+ok+(fail?(' · 失败 '+fail):'')+more;
  $('convInfo').textContent=msg;
  toast(msg,!!fail);
}
$('pick').onchange=async function(){
  const files=[].slice.call(this.files||[]);
  this.value='';
  if(!files.length) return;
  try{
    if(files.length===1) await openOneFile(files[0]);
    else await batchImport(files);
  }catch(e){
    $('fileName').textContent=String(e);
    toast(String(e),true);
  }
};
$('pickBatch').onchange=async function(){
  const files=[].slice.call(this.files||[]);
  this.value='';
  if(!files.length) return;
  try{await batchImport(files);}
  catch(e){toast(String(e),true);}
};
function decode2bp(u8){
  const idx=new Uint8Array(W*H);
  for(let y=0;y<H;y++){
    for(let x=0;x<W;x+=4){
      const byte=u8[y*100+(x>>2)];
      for(let k=0;k<4;k++){
        if(x+k<W) idx[y*W+x+k]=(byte>>((3-k)*2))&3;
      }
    }
  }
  return idx;
}
function idxToCanvas(idx){
  const c=document.createElement('canvas');
  c.width=W;c.height=H;
  const ctx=c.getContext('2d');
  const id=ctx.createImageData(W,H);
  for(let i=0;i<W*H;i++){
    const p=PAL[idx[i]],o=i*4;
    id.data[o]=p[0];id.data[o+1]=p[1];id.data[o+2]=p[2];id.data[o+3]=255;
  }
  ctx.putImageData(id,0,0);
  return c;
}
function idxToImage(idx,cb){
  const im=new Image();
  im.onload=function(){cb(im);};
  im.src=idxToCanvas(idx).toDataURL('image/png');
}
function saveBlob(blob,filename){
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  a.download=filename;a.click();
  setTimeout(function(){URL.revokeObjectURL(a.href);},1500);
}
async function downloadSaved(name){
  const resp=await fetch('/album/file?name='+encodeURIComponent(name));
  const buf=new Uint8Array(await resp.arrayBuffer());
  if(buf.length!==FRAME) throw new Error('尺寸不对');
  // 导出原始 .2bp（四色索引帧），避免 PNG 往返变色
  const fname=/\.2bp$/i.test(name)?name:(name+'.2bp');
  saveBlob(new Blob([buf],{type:'application/octet-stream'}),fname);
}
function crc32(buf){
  let c=~0;
  for(let i=0;i<buf.length;i++){
    c^=buf[i];
    for(let k=0;k<8;k++) c=(c>>>1)^(0xedb88320&-(c&1));
  }
  return ~c>>>0;
}
async function deflateRaw(u8){
  if(typeof CompressionStream==='undefined') return null;
  try{
    const cs=new CompressionStream('deflate-raw');
    const w=cs.writable.getWriter();
    await w.write(u8); await w.close();
    const r=cs.readable.getReader();
    const parts=[];
    for(;;){const x=await r.read(); if(x.done) break; parts.push(x.value);}
    let n=0; for(let i=0;i<parts.length;i++) n+=parts[i].length;
    const out=new Uint8Array(n); let o=0;
    for(let i=0;i<parts.length;i++){out.set(parts[i],o); o+=parts[i].length;}
    return out;
  }catch(e){return null;}
}
async function makeZip(files){
  const enc=new TextEncoder();
  const locals=[];
  let offset=0;
  for(let i=0;i<files.length;i++){
    const f=files[i];
    const name=enc.encode(f.name);
    const data=f.data;
    const crc=crc32(data);
    let cdata=await deflateRaw(data);
    let method=8;
    if(!cdata||cdata.length>=data.length){cdata=data; method=0;}
    const lh=new ArrayBuffer(30);
    const v=new DataView(lh);
    v.setUint32(0,0x04034b50,true);
    v.setUint16(4,20,true);
    v.setUint16(6,0,true);
    v.setUint16(8,method,true);
    v.setUint16(10,0,true);
    v.setUint16(12,0,true);
    v.setUint32(14,crc,true);
    v.setUint32(18,cdata.length,true);
    v.setUint32(22,data.length,true);
    v.setUint16(26,name.length,true);
    v.setUint16(28,0,true);
    locals.push({lh:new Uint8Array(lh),name:name,cdata:cdata,method:method,crc:crc,usize:data.length,csize:cdata.length,offset:offset});
    offset+=30+name.length+cdata.length;
  }
  const central=[];
  let cdirSize=0;
  for(let i=0;i<locals.length;i++){
    const p=locals[i];
    const ch=new ArrayBuffer(46);
    const v=new DataView(ch);
    v.setUint32(0,0x02014b50,true);
    v.setUint16(4,20,true);
    v.setUint16(6,20,true);
    v.setUint16(8,0,true);
    v.setUint16(10,p.method,true);
    v.setUint16(12,0,true);
    v.setUint16(14,0,true);
    v.setUint32(16,p.crc,true);
    v.setUint32(20,p.csize,true);
    v.setUint32(24,p.usize,true);
    v.setUint16(28,p.name.length,true);
    v.setUint16(30,0,true);
    v.setUint16(32,0,true);
    v.setUint16(34,0,true);
    v.setUint16(36,0,true);
    v.setUint32(38,0,true);
    v.setUint32(42,p.offset,true);
    central.push(new Uint8Array(ch),p.name);
    cdirSize+=46+p.name.length;
  }
  const end=new ArrayBuffer(22);
  const ev=new DataView(end);
  ev.setUint32(0,0x06054b50,true);
  ev.setUint16(4,0,true);
  ev.setUint16(6,0,true);
  ev.setUint16(8,locals.length,true);
  ev.setUint16(10,locals.length,true);
  ev.setUint32(12,cdirSize,true);
  ev.setUint32(16,offset,true);
  ev.setUint16(20,0,true);
  const out=[];
  for(let i=0;i<locals.length;i++){
    const p=locals[i];
    out.push(p.lh,p.name,p.cdata);
  }
  for(let i=0;i<central.length;i++) out.push(central[i]);
  out.push(new Uint8Array(end));
  return new Blob(out,{type:'application/zip'});
}
async function batchExportAll(){
  const r=await fetch('/album/list');
  const j=await r.json();
  const names=(j.files||[]).map(function(it){return typeof it==='string'?it:it.name;}).filter(Boolean);
  if(!names.length){toast('没有可导出的照片',true);return;}
  const btn=$('btnExportAll');
  if(btn) btn.disabled=true;
  try{
    const files=[];
    for(let i=0;i<names.length;i++){
      toast('打包 '+(i+1)+'/'+names.length+'…');
      const resp=await fetch('/album/file?name='+encodeURIComponent(names[i]));
      const buf=new Uint8Array(await resp.arrayBuffer());
      if(buf.length!==FRAME) throw new Error(names[i]+' 尺寸不对');
      const fname=/\.2bp$/i.test(names[i])?names[i]:(names[i]+'.2bp');
      files.push({name:fname,data:buf});
    }
    toast('压缩 ZIP…');
    const zip=await makeZip(files);
    const ts=new Date();
    const p=function(n){return (n<10?'0':'')+n;};
    const zipName='album_'+ts.getFullYear()+p(ts.getMonth()+1)+p(ts.getDate())
      +'_'+p(ts.getHours())+p(ts.getMinutes())+p(ts.getSeconds())+'.zip';
    saveBlob(zip,zipName);
    toast('已导出 '+zipName+'（'+names.length+' 张）');
  }catch(e){
    toast('导出失败：'+e,true);
  }finally{
    if(btn) btn.disabled=false;
  }
}
async function loadSaved(name){
  const resp=await fetch('/album/file?name='+encodeURIComponent(name));
  const buf=new Uint8Array(await resp.arrayBuffer());
  loadFrom2bp(buf,name,true);
}
async function sendPacked(overwrite){
  if(!packed) return;
  const q=(overwrite&&editName)?'?name='+encodeURIComponent(editName):'';
  const fd=new FormData();
  fd.append('file',new Blob([packed],{type:'application/octet-stream'}),'photo.2bp');
  $('convInfo').textContent='正在上传…';
  toast('正在保存…');
  $('btnUpload').disabled=true;
  $('btnSaveAs').disabled=true;
  $('btnCast').disabled=true;
  try{
    const r=await fetch('/album/upload'+q,{method:'POST',body:fd});
    const t=await r.text();
    if(!r.ok) throw new Error(t||r.status);
    if(!overwrite) editName=t;
    const msg='已保存 '+t;
    $('convInfo').textContent=msg;
    $('btnUpload').textContent=editName?'覆盖':'上传';
    toast(msg);
    refreshList();
  }finally{
    $('btnUpload').disabled=false;
    $('btnSaveAs').disabled=false;
    $('btnCast').disabled=false;
  }
}
let castCoolUntil=0;
function castCoolLeft(){return Math.max(0,castCoolUntil-Date.now());}
async function waitCastCool(){
  let left=castCoolLeft();
  while(left>0){
    $('convInfo').textContent='墨水屏刷新中… '+Math.ceil(left/1000)+'s';
    await new Promise(function(r){setTimeout(r,400);});
    left=castCoolLeft();
  }
}
async function castPacked(){
  if(!packed||packed.length!==FRAME) throw new Error('请先抖动或载入完整画面');
  if(castCoolLeft()>0) throw new Error('墨水屏刷新中，请稍候');
  // 拷贝一份，避免 TypedArray 在上传过程中被后续操作改写
  const bytes=new Uint8Array(packed);
  const fd=new FormData();
  fd.append('file',new Blob([bytes],{type:'application/octet-stream'}),'preview.2bp');
  $('convInfo').textContent='正在投屏…';
  toast('正在投屏到墨水屏…');
  $('btnCast').disabled=true;
  try{
    const r=await fetch('/album/display-frame',{method:'POST',body:fd});
    const t=await r.text();
    if(!r.ok) throw new Error(t||r.status);
    castCoolUntil=Date.now()+18000;
    $('convInfo').textContent='已投屏，墨水屏刷新中…';
    toast('已投屏，请等刷新完成');
    await waitCastCool();
    $('convInfo').textContent='投屏完成';
  }finally{
    $('btnCast').disabled=!packed;
  }
}
async function castSaved(name){
  if(castCoolLeft()>0) throw new Error('墨水屏刷新中，请稍候');
  const r=await fetch('/album/display?name='+encodeURIComponent(name),{method:'POST'});
  const t=await r.text();
  if(!r.ok) throw new Error(t||r.status);
  castCoolUntil=Date.now()+18000;
  toast('已投屏 '+name+'，请等刷新完成');
  await waitCastCool();
}
$('btnConvert').onclick=function(){run(true);};
$('btnUpload').onclick=async function(){
  try{await sendPacked(!!editName);}catch(e){$('convInfo').textContent='上传失败：'+e;toast('上传失败：'+e,true);}
};
$('btnSaveAs').onclick=async function(){
  try{await sendPacked(false);}catch(e){$('convInfo').textContent='上传失败：'+e;toast('上传失败：'+e,true);}
};
$('btnCast').onclick=async function(){
  try{await castPacked();}catch(e){$('convInfo').textContent='投屏失败：'+e;toast('投屏失败：'+e,true);}
};
async function refreshList(){
  const el=$('list');
  try{
    const r=await fetch('/album/list');
    const j=await r.json();
    const hdr=$('showHeader');
    if(hdr&&typeof j.show_header==='boolean') hdr.checked=j.show_header;
    if(!j.files||!j.files.length){
      el.innerHTML='<p class=meta>(还没有照片)</p>';
      syncSelAll();syncPickAll();
      return;
    }
    el.innerHTML=j.files.map(function(it){
      const n=typeof it==='string'?it:it.name;
      const show=typeof it==='string'?true:!!it.show;
      const on=n===editName?' on':'';
      const dim=show?'':' off';
      return '<div class="thumb'+on+dim+'" data-card="'+n+'">'
        +'<canvas width=400 height=300 data-name="'+n+'"></canvas>'
        +'<div class="row meta-row pick-row">'
        +'<label class="show-lab pick-lab"><input type=checkbox data-pick="'+n+'">选</label>'
        +'<label class=show-lab><input type=checkbox data-show="'+n+'"'+(show?' checked':'')+'>展示</label>'
        +'<code>'+n+'</code></div>'
        +'<div class="row acts">'
        +'<button type=button data-cast="'+n+'" title="投放到墨水屏">投屏</button>'
        +'<button type=button class=secondary data-edit="'+n+'">编辑</button>'
        +'<button type=button class=secondary data-dl="'+n+'" title="下载无损 .2bp">下载</button>'
        +'<button type=button class=danger data-del="'+n+'">删</button></div></div>';
    }).join('');
    el.querySelectorAll('[data-cast]').forEach(function(btn){
      btn.onclick=function(ev){
        ev.stopPropagation();
        const name=btn.dataset.cast;
        btn.disabled=true;
        castSaved(name).catch(function(e){toast('投屏失败：'+e,true);})
          .finally(function(){btn.disabled=false;});
      };
    });
    el.querySelectorAll('[data-dl]').forEach(function(btn){
      btn.onclick=function(ev){
        ev.stopPropagation();
        downloadSaved(btn.dataset.dl).then(function(){toast('已下载 .2bp');})
          .catch(function(e){$('convInfo').textContent=e;toast(String(e),true);});
      };
    });
    el.querySelectorAll('[data-del]').forEach(function(btn){
      btn.onclick=async function(ev){
        ev.stopPropagation();
        const name=btn.dataset.del;
        if(!confirm('删除 '+name+' ?')) return;
        try{
          const r=await fetch('/album/delete?name='+encodeURIComponent(name),{method:'POST'});
          if(!r.ok) throw new Error(await r.text());
          if(editName===name) editName='';
          const card=btn.closest('.thumb');
          if(card) card.remove();
          if(!el.querySelector('.thumb')) el.innerHTML='<p class=meta>(还没有照片)</p>';
          toast('已删除 '+name);
          syncSelAll();syncPickAll();
        }catch(e){toast('删除失败：'+e,true);}
      };
    });
    el.querySelectorAll('[data-pick]').forEach(function(cb){
      cb.onchange=function(ev){
        ev.stopPropagation();
        const card=cb.closest('.thumb');
        if(card) card.classList.toggle('picked',cb.checked);
        syncPickAll();
      };
      cb.onclick=function(ev){ev.stopPropagation();};
    });
    el.querySelectorAll('[data-show]').forEach(function(cb){
      cb.onchange=async function(ev){
        ev.stopPropagation();
        const name=cb.dataset.show;
        const show=cb.checked?1:0;
        await fetch('/album/show?name='+encodeURIComponent(name)+'&show='+show,{method:'POST'});
        cb.closest('.thumb').classList.toggle('off',!cb.checked);
        toast(show?'已设为展示':'已取消展示');
        syncSelAll();
      };
      cb.onclick=function(ev){ev.stopPropagation();};
    });
    el.querySelectorAll('[data-edit],canvas[data-name]').forEach(function(el2){
      el2.onclick=function(){
        const n=el2.dataset.edit||el2.dataset.name;
        loadSaved(n).catch(function(e){$('convInfo').textContent=e;});
      };
    });
    el.querySelectorAll('canvas[data-name]').forEach(async function(cv){
      try{
        const resp=await fetch('/album/file?name='+encodeURIComponent(cv.dataset.name));
        const buf=new Uint8Array(await resp.arrayBuffer());
        if(buf.length!==FRAME) return;
        idxToImage(decode2bp(buf),function(im){cv.getContext('2d').drawImage(im,0,0);});
      }catch(e){}
    });
    syncSelAll();syncPickAll();
  }catch(e){el.textContent='列表失败：'+e;}
}
function syncSelAll(){
  const el=$('selAll');
  if(!el) return;
  const cbs=[].slice.call(document.querySelectorAll('#list [data-show]'));
  if(!cbs.length){el.checked=false;el.disabled=true;el.indeterminate=false;return;}
  el.disabled=false;
  const n=cbs.filter(function(c){return c.checked;}).length;
  el.checked=n===cbs.length;
  el.indeterminate=n>0&&n<cbs.length;
}
function syncPickAll(){
  const el=$('pickAll');
  if(!el) return;
  const cbs=[].slice.call(document.querySelectorAll('#list [data-pick]'));
  if(!cbs.length){el.checked=false;el.disabled=true;el.indeterminate=false;return;}
  el.disabled=false;
  const n=cbs.filter(function(c){return c.checked;}).length;
  el.checked=n===cbs.length;
  el.indeterminate=n>0&&n<cbs.length;
}
function pickedNames(){
  return [].slice.call(document.querySelectorAll('#list [data-pick]:checked'))
    .map(function(cb){return cb.dataset.pick;})
    .filter(Boolean);
}
const panHold=document.createElement('input');
panHold.type='hidden';panHold.id='panx';panHold.value='0';
const panHoldY=document.createElement('input');
panHoldY.type='hidden';panHoldY.id='pany';panHoldY.value='0';
document.body.appendChild(panHold);document.body.appendChild(panHoldY);
(function(){
  const tolEl=$('tol');
  if(!tolEl) return;
  let tolPushed=false;
  tolEl.addEventListener('pointerdown',function(){
    if(selRule>=0&&rules[selRule]){pushRulesHist();tolPushed=true;}
  });
  tolEl.addEventListener('pointerup',function(){tolPushed=false;});
  tolEl.addEventListener('change',function(){
    if(!tolPushed&&selRule>=0&&rules[selRule]) pushRulesHist();
    tolPushed=false;
  });
})();
$('showHeader').onchange=async function(){
  const show=this.checked?1:0;
  await fetch('/album/header?show='+show,{method:'POST'});
  toast(show?'已显示相册标题栏':'已隐藏相册标题栏');
};
$('albumSched').onsubmit=async function(e){
  e.preventDefault();
  const fd=new FormData(this);
  const qs=new URLSearchParams(fd).toString();
  try{
    const r=await fetch('/album/schedule?'+qs,{method:'POST'});
    if(!r.ok) throw new Error(await r.text());
    const en=fd.has('refresh_en');
    const d=+(fd.get('refresh_d')||0),h=+(fd.get('refresh_h')||0),m=+(fd.get('refresh_m')||0);
    toast(en?('定时翻页已保存 · '+d+'天'+h+'时'+m+'分'):'定时翻页已关闭');
  }catch(err){
    toast('保存失败：'+err,true);
  }
};
$('btnExportAll').onclick=function(){
  batchExportAll().catch(function(e){toast(String(e),true);});
};
async function batchDeleteSelected(){
  const names=pickedNames();
  if(!names.length){toast('请先勾选要删除的照片',true);return;}
  if(!confirm('确定删除选中的 '+names.length+' 张？此操作不可恢复。')) return;
  toast('正在删除选中…');
  let ok=0,fail=0;
  for(let i=0;i<names.length;i++){
    const name=names[i];
    try{
      const r=await fetch('/album/delete?name='+encodeURIComponent(name),{method:'POST'});
      if(!r.ok) throw new Error(await r.text());
      if(editName===name) editName='';
      ok++;
    }catch(e){
      fail++;
      toast('失败 '+name+'：'+e,true);
    }
  }
  await refreshList();
  const msg='已删除 '+ok+' 张'+(fail?(' · 失败 '+fail):'');
  toast(msg,!!fail);
}
$('btnDeleteSel').onclick=function(){
  batchDeleteSelected().catch(function(e){toast(String(e),true);});
};
$('pickAll').onchange=function(){
  const on=this.checked;
  this.indeterminate=false;
  [].slice.call(document.querySelectorAll('#list [data-pick]')).forEach(function(cb){
    cb.checked=on;
    const card=cb.closest('.thumb');
    if(card) card.classList.toggle('picked',on);
  });
};
$('selAll').onchange=async function(){
  const show=this.checked?1:0;
  this.indeterminate=false;
  const cbs=[].slice.call(document.querySelectorAll('#list [data-show]'));
  cbs.forEach(function(cb){
    cb.checked=!!show;
    const card=cb.closest('.thumb');
    if(card) card.classList.toggle('off',!show);
  });
  toast(show?'正在全选展示…':'正在全部取消展示…');
  try{
    const r=await fetch('/album/show-all?show='+show,{method:'POST'});
    if(!r.ok) throw new Error(await r.text());
    toast(show?'已全部展示':'已全部取消展示');
  }catch(e){
    toast(String(e),true);
    refreshList();
  }
};
paint.fill(-1);
buildMixes();
window.addEventListener('resize',function(){requestAnimationFrame(drawWires);});
setTool('pan');setBrushCol(0);setEm();renderRules();drawHist();drawInkHist();refreshList();
</script>)ALBUMJS";
  server.sendContent(kJs, sizeof(kJs) - 1);
}



static void handleAlbumList() {
  AlbumList list;
  albumList(list);
  JsonDocument doc;
  JsonArray arr = doc["files"].to<JsonArray>();
  for (int i = 0; i < list.count; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = list.names[i];
    o["show"] = list.shown[i];
  }
    doc["count"] = list.count;
  doc["max"] = 0;  // 0 = 不限张数（受存储空间限制）
  doc["show_header"] = getAppConfig().album_show_header;
  String out;
  serializeJson(doc, out);
  g_srv->send(200, "application/json", out);
}

static void handleAlbumDelete() {
  const String name = g_srv->arg("name");
  if (!albumDelete(name.c_str())) {
    g_srv->send(400, "text/plain", "delete failed");
    return;
  }
  g_srv->send(200, "text/plain", "ok");
}

static void handleAlbumDeleteAll() {
  const int n = albumDeleteAll();
  if (n < 0) {
    g_srv->send(400, "text/plain", "delete-all failed");
    return;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "已删除 %d 张", n);
  g_srv->send(200, "text/plain", buf);
}

static void handleAlbumShow() {
  const String name = g_srv->arg("name");
  const bool show = g_srv->arg("show") != "0";
  if (!albumSetShown(name.c_str(), show)) {
    g_srv->send(400, "text/plain", "show failed");
    return;
  }
  g_srv->send(200, "text/plain", "ok");
}

static void handleAlbumShowAll() {
  const bool show = g_srv->arg("show") != "0";
  if (!albumSetShownAll(show)) {
    g_srv->send(400, "text/plain", "show-all failed");
    return;
  }
  g_srv->send(200, "text/plain", "ok");
}

static void handleAlbumHeader() {
  saveAlbumShowHeader(g_srv->arg("show") != "0");
  g_srv->send(200, "text/plain", "ok");
}

static void handleAlbumSchedule() {
  const bool en = g_srv->hasArg("refresh_en");
  const uint32_t d = (uint32_t)g_srv->arg("refresh_d").toInt();
  const uint32_t h = (uint32_t)g_srv->arg("refresh_h").toInt();
  const uint32_t m = (uint32_t)g_srv->arg("refresh_m").toInt();
  const uint32_t sec = weatherRefreshSecFromDhm(d, h, m);
  saveAlbumRefreshSec(sec);
  saveAlbumRefreshEn(en);
  g_srv->send(200, "text/plain", "ok");
}

static void handleAlbumUploadFile() {
  HTTPUpload &up = g_srv->upload();
  if (up.status == UPLOAD_FILE_START) {
    g_up_ok = false;
    g_up_name[0] = '\0';
    if (!albumEnsureDir()) return;
    const String want = g_srv->arg("name");
    if (want.length() && albumNameOk(want.c_str())) {
      strncpy(g_up_name, want.c_str(), sizeof(g_up_name) - 1);
      g_up_name[sizeof(g_up_name) - 1] = '\0';
    } else if (!albumNextFileName(g_up_name, sizeof(g_up_name))) {
      Serial.println("[Album] upload reject (full or mkdir)");
      return;
    }
    char path[40];
    snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, g_up_name);
    g_up_file = LittleFS.open(path, "w");
    g_up_ok = (bool)g_up_file;
    Serial.printf("[Album] upload -> %s ok=%d\n", path, (int)g_up_ok);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (g_up_ok && g_up_file) {
      if (g_up_file.size() + up.currentSize > (size_t)ALBUM_FRAME_BYTES) {
        g_up_ok = false;
      } else {
        g_up_file.write(up.buf, up.currentSize);
      }
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (g_up_file) g_up_file.close();
    char path[40];
    snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, g_up_name);
    File f = LittleFS.open(path, "r");
    const size_t sz = f ? f.size() : 0;
    if (f) f.close();
    if (!g_up_ok || sz != (size_t)ALBUM_FRAME_BYTES) {
      if (LittleFS.exists(path)) LittleFS.remove(path);
      g_up_ok = false;
      Serial.printf("[Album] upload bad size=%u\n", (unsigned)sz);
    } else {
      Serial.printf("[Album] upload done %s\n", g_up_name);
    }
  }
}

static void handleAlbumUploadDone() {
  if (g_up_ok && g_up_name[0]) {
    g_srv->send(200, "text/plain", g_up_name);
  } else {
    g_srv->send(400, "text/plain", "upload failed (need exactly 30000 bytes)");
  }
}

static void handleAlbumFileGet() {
  const String name = g_srv->arg("name");
  if (!albumNameOk(name.c_str())) {
    g_srv->send(400, "text/plain", "bad name");
    return;
  }
  char path[40];
  snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, name.c_str());
  File f = LittleFS.open(path, "r");
  if (!f) {
    g_srv->send(404, "text/plain", "missing");
    return;
  }
  g_srv->streamFile(f, "application/octet-stream");
  f.close();
}

// POST /album/display?name=001.2bp — 无 upload 回调（空 body 挂 upload 会空指针崩）
static void handleAlbumDisplayByName() {
  if (albumCastBlocked()) {
    g_srv->send(503, "text/plain", "墨水屏刷新中，请稍候再投屏");
    return;
  }
  const String name = g_srv->arg("name");
  if (!albumRequestDisplayName(name.c_str())) {
    g_srv->send(400, "text/plain", "找不到该照片");
    return;
  }
  g_srv->send(200, "text/plain", "ok");
}

// POST /album/display-frame + multipart .2bp — 投屏当前编辑帧
// 先写入 tmp，完整 30000 字节后再替换 preview，避免半截/空包毁掉上一帧。
static void handleAlbumDisplayFile() {
  HTTPUpload &up = g_srv->upload();
  if (up.status == UPLOAD_FILE_START) {
    g_cast_ok = false;
    g_cast_bytes = 0;
    if (g_cast_file) g_cast_file.close();
    if (LittleFS.exists(ALBUM_PREVIEW_TMP)) LittleFS.remove(ALBUM_PREVIEW_TMP);
    g_cast_file = LittleFS.open(ALBUM_PREVIEW_TMP, "w");
    g_cast_ok = (bool)g_cast_file;
    Serial.printf("[Album] cast upload -> %s ok=%d\n", ALBUM_PREVIEW_TMP, (int)g_cast_ok);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (g_cast_ok && g_cast_file) {
      if (g_cast_bytes + up.currentSize > (size_t)ALBUM_FRAME_BYTES) {
        g_cast_ok = false;
      } else {
        const size_t n = g_cast_file.write(up.buf, up.currentSize);
        if (n != up.currentSize) {
          g_cast_ok = false;
        } else {
          g_cast_bytes += n;
        }
      }
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (g_cast_file) g_cast_file.close();
    if (!g_cast_ok || g_cast_bytes != (size_t)ALBUM_FRAME_BYTES) {
      if (LittleFS.exists(ALBUM_PREVIEW_TMP)) LittleFS.remove(ALBUM_PREVIEW_TMP);
      g_cast_ok = false;
      Serial.printf("[Album] cast upload bad size=%u (want %d)\n", (unsigned)g_cast_bytes,
                    ALBUM_FRAME_BYTES);
    } else {
      if (LittleFS.exists(ALBUM_PREVIEW_PATH)) LittleFS.remove(ALBUM_PREVIEW_PATH);
      if (!LittleFS.rename(ALBUM_PREVIEW_TMP, ALBUM_PREVIEW_PATH)) {
        // rename 失败则尝试读 tmp 再写正式路径
        File src = LittleFS.open(ALBUM_PREVIEW_TMP, "r");
        File dst = LittleFS.open(ALBUM_PREVIEW_PATH, "w");
        bool copied = false;
        if (src && dst) {
          uint8_t buf[512];
          copied = true;
          while (src.available()) {
            const int n = src.read(buf, sizeof(buf));
            if (n <= 0 || (int)dst.write(buf, n) != n) {
              copied = false;
              break;
            }
          }
        }
        if (src) src.close();
        if (dst) dst.close();
        LittleFS.remove(ALBUM_PREVIEW_TMP);
        g_cast_ok = copied;
      }
      if (g_cast_ok) Serial.println("[Album] cast upload done");
      else Serial.println("[Album] cast upload rename/copy failed");
    }
  }
}

static void handleAlbumDisplayFrameDone() {
  if (albumCastBlocked()) {
    g_srv->send(503, "text/plain", "墨水屏刷新中，请稍候再投屏");
    return;
  }
  if (!g_cast_ok || !albumRequestDisplayPreview()) {
    // 400×300 四色帧 = 30000 字节；空包/半截多半是刷屏期间又点了一次
    g_srv->send(400, "text/plain", "投屏数据不完整，请等刷新结束后再试");
    return;
  }
  g_srv->send(200, "text/plain", "ok");
}

void webAlbumRegisterRoutes(WebServer &server) {
  g_srv = &server;
  server.on("/album/list", HTTP_GET, handleAlbumList);
  server.on("/album/delete", HTTP_POST, handleAlbumDelete);
  server.on("/album/delete-all", HTTP_POST, handleAlbumDeleteAll);
  server.on("/album/show", HTTP_POST, handleAlbumShow);
  server.on("/album/show-all", HTTP_POST, handleAlbumShowAll);
  server.on("/album/header", HTTP_POST, handleAlbumHeader);
  server.on("/album/schedule", HTTP_POST, handleAlbumSchedule);
  server.on("/album/file", HTTP_GET, handleAlbumFileGet);
  server.on("/album/upload", HTTP_POST, handleAlbumUploadDone, handleAlbumUploadFile);
  server.on("/album/display", HTTP_POST, handleAlbumDisplayByName);
  server.on("/album/display-frame", HTTP_POST, handleAlbumDisplayFrameDone, handleAlbumDisplayFile);
}
