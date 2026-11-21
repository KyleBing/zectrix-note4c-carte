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

void webAlbumAppendPage(String &b) {
  b += F(
      "<div class=ed-shell>"
      "<div class=ed-work>"
      "<div class=ed-toolbar>"
      "<div class=ed-tool-main>"
      "<label class=file-pick>"
      "<span class=file-cta><input type=file id=pick accept='image/*,.2bp,application/octet-stream'>选取图片 / .2bp</span>"
      "<span class='file-name empty' id=fileName>未选择文件</span>"
      "</label>"
      "<div class=ed-sep></div>"
      "<div class=ed-tool-group title=画面>"
      "<button type=button id=btnPan class=secondary>平移</button>"
      "<button type=button id=btnFit class=secondary>铺满</button>"
      "<button type=button id=btnContain class=secondary>完整</button>"
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
         "<span>缩放</span><input type=range id=zoom min=50 max=300 value=100><em id=zoomV>100%</em>"
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
      "</div>"
      "<div class=ed-map-body>"
      "<div class=ed-cmap id=wireWrap>"
      "<div class='ed-cmap-edge e-t'>黄</div>"
      "<div class='ed-cmap-edge e-b'>红</div>"
      "<div class='ed-cmap-edge e-l'>黑</div>"
      "<div class='ed-cmap-edge e-r'>白</div>"
      "<div class=ed-cmap-plot id=cmapPlot></div>"
      "<svg id=wires class=ed-wires></svg>"
      "</div>"
      "<div id=tolPop class=tol-pop>"
      "<span id=tolLab>容差</span>"
      "<input type=range id=tol class=tol-v min=8 max=120 value=36 orient=vertical>"
      "<em id=tolV>36</em>"
      "</div>"
      "</div>"
      "<p class=hint ed-map-hint>色盘上是全部原始色（4 份墨水组合，共 35 色；左黑 / 右白 / 上黄 / 下红向中间过渡）。初始按最近原始色匹配照片。点某个原始色后再在左图取色，把照片颜色绑定到该原始色；圆点是照片色。点绑定可改容差；Backspace / Delete 删除；Ctrl/⌘+Z 撤销。</p>"
      "<div id=rules class=rules></div>"
      "</div></div>"
      "<aside class=ed-side>"
      "<div class=ed-mode>"
      "<button type=button id=modeGrain class=on>颗粒</button>"
      "<button type=button id=modeSolid>色块</button>"
      "</div>"
      "<div class=ed-side-ops>"
      "<div class=ed-block>"
      "<div class=ink-hist-wrap><canvas id=inkHist width=320 height=48></canvas></div>"
      "<div class=ink-hist-read>"
      "<span id=ink0>黑 —</span><span id=ink1>白 —</span><span id=ink2>黄 —</span><span id=ink3>红 —</span>"
      "</div>"
      "<div class=ed-row><span>亮度</span><input type=range id=brt min=20 max=180 value=100><em id=brtV>100%</em></div>"
      "<div class=ed-row><span>对比</span><input type=range id=con min=20 max=220 value=100><em id=conV>100%</em></div>"
      "<div class=ed-row><span>饱和</span><input type=range id=sat min=0 max=250 value=100><em id=satV>100%</em></div>"
      "<div class=ed-row><span>色相</span><input type=range id=hue min=-180 max=180 value=0><em id=hueV>0°</em></div>"
      "<div class=ed-row><span>色温</span><input type=range id=tmp min=-80 max=80 value=0><em id=tmpV>0</em></div>"
      "<div class=ed-row id=dithRow><span>颗粒</span><input type=range id=dith min=20 max=160 value=100><em id=dithV>100%</em></div>"
      "</div>"
      "<div class=ed-levels>"
      "<div class=ed-side-h>"
      "<h3>色阶</h3>"
      "</div>"
      "<div class=ed-block>"
      "<div class=hist-wrap><canvas id=hist width=1024 height=160 tabindex=0></canvas></div>"
      "<div class=hist-read><span id=lvBlk>黑 0</span><span id=lvMid>γ 1.00</span><span id=lvWht>白 255</span></div>"
      "<div class=ed-row><span>黑场</span><input type=range id=inBlk min=0 max=254 step=1 value=0><input type=number id=inBlkN class=ed-num min=0 max=254 step=1 value=0></div>"
      "<div class=ed-row><span>灰点</span><input type=range id=inMid min=1 max=254 step=1 value=128><input type=number id=inMidN class=ed-num min=1 max=254 step=1 value=128></div>"
      "<div class=ed-row><span>白场</span><input type=range id=inWht min=1 max=255 step=1 value=255><input type=number id=inWhtN class=ed-num min=1 max=255 step=1 value=255></div>"
      "<div>"
      "<div class=ed-row><span>出黑</span><input type=range id=outBlk min=0 max=254 step=1 value=0><em id=outBlkV>0</em></div>"
      "<div class=ed-row><span>出白</span><input type=range id=outWht min=1 max=255 step=1 value=255><em id=outWhtV>255</em></div>"
      "<div class=ed-row><span>黄墨</span><input type=range id=yel min=0 max=220 value=100><em id=yelV>100%</em></div>"
      "<div class=ed-row><span>红墨</span><input type=range id=red min=0 max=220 value=100><em id=redV>100%</em></div>"
      "<div class=ed-row><span>灰阈</span><input type=range id=chr min=0 max=80 value=8><em id=chrV>8</em></div>"
      "</div>"
      "</div>"
      "</div>"
      "</div>"
      "<p class=hint ed-help>左图载入后一直是原图。调色、色块和颜色绑定只改右图。色盘是原始色；初始自动就近匹配，再点原始色去左图取色绑定。色块是不带颗粒的纯色块。空格+拖动平移。</p>"
      "</aside>"
      "</div></div>"
      "<div id=toast class=ed-toast></div>"
      "<div class=ed-album>"
      "<div class=ed-album-h>"
      "<h2>相册</h2>"
      "<p class=hint style=margin:0>最多 ");
  b += String(ALBUM_MAX);
  b += F(" 张 · 取消展示后设备不翻到该图</p>"
         "<label class=show-lab><input type=checkbox id=selAll>全选展示</label>"
         "</div>"
         "<label class=check-row><input type=checkbox id=showHeader");
  if (getAppConfig().album_show_header) b += F(" checked");
  b += F("><span>设备显示标题栏</span></label>"
         "<p class=hdr-hint>墨水屏相册页顶部是否叠「相册」标题条；关掉可多露出一点画面。</p>"
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
let img=null,packed=null,rules=[],mapTo=-1,selRule=-1,cover=true,tool='pan',grain=true;
let rulesHist=[],RULES_HIST_MAX=40;
function cloneRulesState(){return {rules:JSON.parse(JSON.stringify(rules)),selRule:selRule};}
function pushRulesHist(){
  rulesHist.push(cloneRulesState());
  if(rulesHist.length>RULES_HIST_MAX) rulesHist.shift();
}
function undoRules(){
  if(!rulesHist.length){toast('没有可撤销');return;}
  const s=rulesHist.pop();
  rules=s.rules;selRule=s.selRule;
  if(selRule>=rules.length) selRule=-1;
  renderRules();schedule(true);toast('已撤销');
}
let mixList=[];
let drag=false,lx=0,ly=0,tmr=null,adjRgb=null,rawLum=null;
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
  let r=0,g=0,b=0;
  for(let i=0;i<4;i++){r+=PAL[i][0]*cnt[i];g+=PAL[i][1]*cnt[i];b+=PAL[i][2]*cnt[i];}
  return [Math.round(r/4),Math.round(g/4),Math.round(b/4)];
}
function isPureMix(cnt){
  return cnt.some(function(n){return n===4;});
}
function gammaFromMid(){
  const t=Math.min(0.99,Math.max(0.01,(mid-blk)/Math.max(1,wht-blk)));
  return Math.log(0.5)/Math.log(t);
}
function setEm(){
  $('zoomV').textContent=val('zoom')+'%';
  $('brushV').textContent=val('brush');
  $('brtV').textContent=val('brt')+'%';
  $('conV').textContent=val('con')+'%';
  $('satV').textContent=val('sat')+'%';
  $('hueV').textContent=val('hue')+'°';
  $('tmpV').textContent=(val('tmp')>0?'+':'')+val('tmp');
  $('yelV').textContent=val('yel')+'%';
  $('redV').textContent=val('red')+'%';
  $('chrV').textContent=val('chr');
  $('dithV').textContent=val('dith')+'%';
  $('tolV').textContent=val('tol');
  $('outBlkV').textContent=outBlk;
  $('outWhtV').textContent=outWht;
  $('lvBlk').textContent='黑 '+blk;
  $('lvWht').textContent='白 '+wht;
  $('lvMid').textContent='γ '+gammaFromMid().toFixed(2);
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
  const hue=val('hue'),tmp=val('tmp'),chTh=val('chr');
  const gma=Math.max(0.1,gammaFromMid());
  const yb=val('yel')/100,rb=val('red')/100;
  r=r*br;g=g*br;b=b*br;
  r=(r-128)*co+128;g=(g-128)*co+128;b=(b-128)*co+128;
  r+=tmp*1.15;b-=tmp*1.15;
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
  for(let i=0;i<4;i++) if(cnt[i]===4) return i;
  return -1;
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
  const cssH=48;
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
  // 左黑右白、上黄下红，向中心过渡
  return {x:(cnt[1]-cnt[0])/4, y:(cnt[2]-cnt[3])/4};
}
function posStyle(x,y,ox,oy){
  ox=ox||0;oy=oy||0;
  return 'left:calc('+(50+x*46)+'% + '+ox+'px);top:calc('+(50-y*46)+'% + '+oy+'px)';
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
  let html='';
  rules.forEach(function(u,i){
    const orig=swatchEl(u.to);
    const chip=document.querySelector('#cmapPlot [data-c="'+i+'"]');
    if(!orig||!chip) return;
    const a=orig.getBoundingClientRect(),b=chip.getBoundingClientRect();
    const x1=a.left+a.width/2-wr.left,y1=a.top+a.height/2-wr.top;
    const x2=b.left+b.width/2-wr.left,y2=b.top+b.height/2-wr.top;
    const p=mixRgb(mixList[u.to]);
    const col='rgb('+p[0]+','+p[1]+','+p[2]+')';
    const w=i===selRule?3.6:2;
    html+='<line x1="'+x1+'" y1="'+y1+'" x2="'+x2+'" y2="'+y2+'" stroke="transparent" stroke-width="14" data-line="'+i+'"/>';
    html+='<line x1="'+x1+'" y1="'+y1+'" x2="'+x2+'" y2="'+y2+'" stroke="'+col+'" stroke-width="'+w+'" data-line="'+i+'"/>';
  });
  svg.innerHTML=html;
  svg.querySelectorAll('[data-line]').forEach(function(ln){
    ln.style.pointerEvents='stroke';
    ln.onclick=function(e){
      e.stopPropagation();
      e.preventDefault();
      selectRule(+ln.getAttribute('data-line'),true);
    };
  });
}
function renderRules(){
  const plot=$('cmapPlot');
  if(!plot) return;
  document.querySelectorAll('#cmapPlot [data-c]').forEach(function(el){el.remove();});
  const per={};
  rules.forEach(function(u,i){
    const btn=document.createElement('button');
    btn.type='button';
    btn.className='sw-drop';
    btn.dataset.c=String(i);
    const rgb=[Math.round(u.r),Math.round(u.g),Math.round(u.b)];
    btn.title='照片 '+rgb.join(',')+' ±'+u.tol+' → '+origName(u.to);
    const base=mixPos(mixList[u.to]||[1,1,1,1]);
    const n=per[u.to]|0;per[u.to]=n+1;
    const ang=n*0.9-0.4;
    const ox=Math.cos(ang)*14,oy=Math.sin(ang)*14;
    const ring=mixRgb(mixList[u.to]||[0,0,0,4]);
    btn.style.cssText=posStyle(base.x,base.y,ox,oy)+';background:rgb('+rgb[0]+','+rgb[1]+','+rgb[2]+');border-color:rgb('+ring[0]+','+ring[1]+','+ring[2]+')';
    plot.appendChild(btn);
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
function addOrSelectRule(r,g,b){
  if(mapTo<0||mapTo>=mixList.length){toast('先点色盘上的原始颜色');return;}
  r=Math.round(r);g=Math.round(g);b=Math.round(b);
  const found=rules.findIndex(function(u){return sameRgb(u,r,g,b);});
  if(found>=0){
    const prev=rules[found].to;
    if(prev===mapTo){selRule=found;renderRules();return;}
    pushRulesHist();
    rules[found].to=mapTo;
    selRule=found;
    renderRules();
    schedule(true);
    return;
  }
  pushRulesHist();
  rules.push({r:r,g:g,b:b,tol:val('tol'),to:mapTo});
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
function buildMixes(){
  mixList=[];
  for(let a=0;a<=4;a++)
    for(let b=0;b<=4-a;b++)
      for(let c=0;c<=4-a-b;c++)
        mixList.push([a,b,c,4-a-b-c]);
  const groups={};
  mixList.forEach(function(cnt,i){
    const p=mixPos(cnt);
    const key=Math.round(p.x*4)+','+Math.round(p.y*4);
    (groups[key]=groups[key]||[]).push({i:i,cnt:cnt,p:p});
  });
  const pureCls=['pal-k','pal-w','pal-y','pal-r'];
  let html='';
  Object.keys(groups).forEach(function(key){
    const g=groups[key];
    g.forEach(function(it,gi){
      const rgb=mixRgb(it.cnt);
      let ox=0,oy=0;
      if(g.length>1){
        const ang=gi*(Math.PI*2)/g.length-Math.PI/2;
        ox=Math.cos(ang)*9;
        oy=Math.sin(ang)*9;
      }
      const sty=posStyle(it.p.x,it.p.y,ox,oy);
      const ink=pureInkOf(it.cnt);
      if(ink>=0){
        html+='<button type=button class="sw-pure '+pureCls[ink]+'" data-m="'+it.i+'" style="'+sty+'" title="'+PN[ink]+'" aria-label="'+PN[ink]+'"></button>';
      }else{
        html+='<button type=button class=sw-mix data-m="'+it.i+'" style="'+sty+';background:rgb('+rgb[0]+','+rgb[1]+','+rgb[2]+')" title="'+mixLabel(it.cnt)+'"></button>';
      }
    });
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
    drag=true;lx=e.clientX;ly=e.clientY;syncCursor();return;
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
  $('panx').value=val('panx')+(e.clientX-lx)*W/r.width;
  $('pany').value=val('pany')+(e.clientY-ly)*H/r.height;
  lx=e.clientX;ly=e.clientY;scheduleGeom(false);
};
srcCv.onpointerup=function(){if(drag){drag=false;syncCursor();scheduleGeom(true);}};
srcCv.onpointercancel=function(){if(drag){drag=false;syncCursor();scheduleGeom(true);}else syncCursor();};
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
bindLevel('inBlk','inBlkN',function(v){blk=Math.max(0,Math.min(mid-1,v|0));});
bindLevel('inMid','inMidN',function(v){mid=Math.max(blk+1,Math.min(wht-1,v|0));});
bindLevel('inWht','inWhtN',function(v){wht=Math.max(mid+1,Math.min(255,v|0));});
['outBlk','outWht'].forEach(function(id){
  $(id).oninput=function(){
    outBlk=Math.max(0,Math.min(254,val('outBlk')));
    outWht=Math.max(outBlk+1,Math.min(255,val('outWht')));
    setEm();schedule(false);
  };
  $(id).onchange=function(){schedule(true);};
});
['zoom','yel','red','brush','brt','con','sat','hue','tmp','chr','dith'].forEach(function(id){
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
  zoom:100,brush:6,brt:100,con:100,sat:100,hue:0,tmp:0,
  yel:100,red:100,chr:8,dith:100,tol:36,
  inBlk:0,inMid:128,inWht:255,outBlk:0,outWht:255
};
function applySliderDefault(id){
  const d=SLIDER_DEF[id];
  if(d==null||!$(id)) return;
  $(id).value=d;
  if(id==='inBlk'){blk=Math.max(0,Math.min(mid-1,d));fillLevelInputs();}
  else if(id==='inMid'){mid=Math.max(blk+1,Math.min(wht-1,d));fillLevelInputs();}
  else if(id==='inWht'){wht=Math.max(mid+1,Math.min(255,d));fillLevelInputs();}
  else if(id==='outBlk'){outBlk=d;if(outWht<=outBlk) outWht=Math.min(255,outBlk+1);fillLevelInputs();}
  else if(id==='outWht'){outWht=d;if(outWht<=outBlk) outBlk=Math.max(0,outWht-1);fillLevelInputs();}
  else if(id==='tol'&&selRule>=0&&rules[selRule]){rules[selRule].tol=d;renderRules();}
  setEm();
  if(id==='brush') return;
  if(id==='zoom'){scheduleGeom(true);return;}
  if(id==='inBlk'||id==='inMid'||id==='inWht') drawHist();
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
function resetView(){
  $('zoom').value=100;$('panx').value=0;$('pany').value=0;cover=true;
  paint=new Int8Array(W*H);paint.fill(-1);
}
function resetSide(){
  blk=0;mid=128;wht=255;outBlk=0;outWht=255;
  $('brt').value=100;$('con').value=100;$('sat').value=100;$('hue').value=0;$('tmp').value=0;
  $('yel').value=100;$('red').value=100;$('chr').value=8;$('dith').value=100;$('tol').value=36;
  rules=[];selRule=-1;setMapTo(-1);renderRules();setGrain(true,true);fillLevelInputs();
}
$('btnReset').onclick=function(){resetView();schedule(true);};
$('btnSideReset').onclick=function(){resetSide();schedule(true);};
$('btnClearMaps').onclick=function(){if(!rules.length)return;pushRulesHist();rules=[];selRule=-1;renderRules();schedule(true);};
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
    $('btnUpload').textContent=editName?'覆盖':'上传';
    $('convInfo').textContent='已载入 .2bp'+(editName?(' '+editName):'')+'（未重新抖动）';
    toast('已载入 .2bp');
  };
  im.src=c.toDataURL('image/png');
}
$('pick').onchange=function(){
  const f=this.files[0];
  if(!f) return;
  const is2bp=/\.2bp$/i.test(f.name)||f.type==='application/octet-stream';
  if(is2bp||f.size===FRAME){
    const rd=new FileReader();
    rd.onload=function(){
      try{
        loadFrom2bp(new Uint8Array(rd.result),f.name,/\.2bp$/i.test(f.name));
      }catch(e){
        $('fileName').textContent=String(e);
        toast(String(e),true);
      }
    };
    rd.onerror=function(){$('fileName').textContent='读取失败';};
    rd.readAsArrayBuffer(f);
    return;
  }
  const url=URL.createObjectURL(f);
  const im=new Image();
  im.onload=function(){URL.revokeObjectURL(url);loadImage(im,f.name,false);};
  im.onerror=function(){$('fileName').textContent='图片加载失败';URL.revokeObjectURL(url);};
  im.src=url;
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
  }
}
$('btnConvert').onclick=function(){run(true);};
$('btnUpload').onclick=async function(){
  try{await sendPacked(!!editName);}catch(e){$('convInfo').textContent='上传失败：'+e;toast('上传失败：'+e,true);}
};
$('btnSaveAs').onclick=async function(){
  try{await sendPacked(false);}catch(e){$('convInfo').textContent='上传失败：'+e;toast('上传失败：'+e,true);}
};
async function refreshList(){
  const el=$('list');
  try{
    const r=await fetch('/album/list');
    const j=await r.json();
    const hdr=$('showHeader');
    if(hdr&&typeof j.show_header==='boolean') hdr.checked=j.show_header;
    if(!j.files||!j.files.length){el.innerHTML='<p class=meta>(还没有照片)</p>';syncSelAll();return;}
    el.innerHTML=j.files.map(function(it){
      const n=typeof it==='string'?it:it.name;
      const show=typeof it==='string'?true:!!it.show;
      const on=n===editName?' on':'';
      const dim=show?'':' off';
      return '<div class="thumb'+on+dim+'"><canvas width=400 height=300 data-name="'+n+'"></canvas>'
        +'<div class="row meta-row">'
        +'<label class=show-lab><input type=checkbox data-show="'+n+'"'+(show?' checked':'')+'>展示</label>'
        +'<code>'+n+'</code></div>'
        +'<div class="row acts">'
        +'<button type=button class=secondary data-dl="'+n+'" title="下载无损 .2bp">下载 .2bp</button>'
        +'<button type=button class=secondary data-edit="'+n+'">编辑</button>'
        +'<button type=button class=danger data-del="'+n+'">删</button></div></div>';
    }).join('');
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
          syncSelAll();
        }catch(e){toast('删除失败：'+e,true);}
      };
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
    syncSelAll();
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
  doc["max"] = ALBUM_MAX;
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
    g_srv->send(400, "text/plain", "upload failed (need exactly 30000 bytes, max 30)");
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

void webAlbumRegisterRoutes(WebServer &server) {
  g_srv = &server;
  server.on("/album/list", HTTP_GET, handleAlbumList);
  server.on("/album/delete", HTTP_POST, handleAlbumDelete);
  server.on("/album/show", HTTP_POST, handleAlbumShow);
  server.on("/album/show-all", HTTP_POST, handleAlbumShowAll);
  server.on("/album/header", HTTP_POST, handleAlbumHeader);
  server.on("/album/file", HTTP_GET, handleAlbumFileGet);
  server.on("/album/upload", HTTP_POST, handleAlbumUploadDone, handleAlbumUploadFile);
}
