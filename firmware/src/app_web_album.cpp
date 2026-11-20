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
      "<span class=file-cta><input type=file id=pick accept='image/*'>选取图片</span>"
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
      "<div class=ed-side-h>"
      "<h3>颜色替换</h3>"
      "<button type=button id=btnClearMaps class=secondary>清空连线</button>"
      "</div>"
      "<div class=ed-block>"
      "<div class=ed-map-bar>"
      "<button type=button id=btnPick class=secondary>取色</button>"
      "<span class=hint style=margin:0>先点中间原色，再在左图上取色，或点两侧衍生色，就把该颜色替换成这个原色。点连线可改容差。左列偏暗，右列偏亮。</span>"
      "</div>"
      "<div class=ed-wire id=wireWrap>"
      "<div id=mixL class='mix-col side-l'></div>"
      "<div class=ed-inks id=mapPal>"
      "<button type=button id=map0 class=pal-k title=黑 aria-label=黑></button>"
      "<button type=button id=map1 class=pal-w title=白 aria-label=白></button>"
      "<button type=button id=map2 class=pal-y title=黄 aria-label=黄></button>"
      "<button type=button id=map3 class=pal-r title=红 aria-label=红></button>"
      "</div>"
      "<div id=mixR class='mix-col side-r'></div>"
      "<svg id=wires class=ed-wires></svg>"
      "</div>"
      "<div id=tolPop class=tol-pop hidden>"
      "<span id=tolLab>容差</span><input type=range id=tol min=8 max=120 value=36><em id=tolV>36</em>"
      "</div>"
      "<div id=rules class=rules></div>"
      "</div>"
      "</div></div>"
      "<aside class=ed-side>"
      "<div class=ed-mode>"
      "<button type=button id=modeGrain class=on>颗粒</button>"
      "<button type=button id=modeSolid>色块</button>"
      "</div>"
      "<div class=ed-block>"
      "<div class=ed-row><span>亮度</span><input type=range id=brt min=20 max=180 value=100><em id=brtV>100%</em></div>"
      "<div class=ed-row><span>对比</span><input type=range id=con min=20 max=220 value=100><em id=conV>100%</em></div>"
      "<div class=ed-row><span>饱和</span><input type=range id=sat min=0 max=250 value=100><em id=satV>100%</em></div>"
      "<div class=ed-row><span>色相</span><input type=range id=hue min=-180 max=180 value=0><em id=hueV>0°</em></div>"
      "<div class=ed-row><span>色温</span><input type=range id=tmp min=-80 max=80 value=0><em id=tmpV>0</em></div>"
      "<div class=ed-row id=dithRow><span>颗粒</span><input type=range id=dith min=20 max=160 value=100><em id=dithV>100%</em></div>"
      "</div>"
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
      "<p class=hint ed-help>左图载入后一直是原图。调色、色块和颜色替换只改右图。点中间原色后，可在左图取色或点两侧衍生色来替换；点连线改容差。左列偏暗，右列偏亮。色块是不带颗粒的纯色块。空格+拖动平移。</p>"
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

  b += R"ALBUMJS(<script>
const W=400,H=300,FRAME=30000;
const PAL=[[0,0,0],[255,255,255],[246,199,73],[112,27,14]];
const PN=['黑','白','黄','红'];
function $(id){return document.getElementById(id)}
function val(id){return +$(id).value}
let img=null,packed=null,rules=[],mapTo=-1,selRule=-1,cover=true,tool='pan',grain=true;
let mixList=[],mixSplit=128;
let drag=false,lx=0,ly=0,tmr=null,adjRgb=null,rawLum=null;
let baseIdx=null,paint=new Int8Array(W*H);
let blk=0,mid=128,wht=255,outBlk=0,outWht=255,histDrag=-1,editName='',brushCol=0,loupeRight=false,visCol=-1;
let spaceHeld=false,pvDrag=false,pvx=0,pvy=0;
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
function cropRGB(){
  const c=document.createElement('canvas');
  c.width=W;c.height=H;
  const ctx=c.getContext('2d',{willReadFrequently:true});
  ctx.fillStyle='#fff';ctx.fillRect(0,0,W,H);
  if(img){
    const s0=cover?Math.max(W/img.width,H/img.height):Math.min(W/img.width,H/img.height);
    const s=s0*(val('zoom')/100);
    const dw=img.width*s,dh=img.height*s;
    ctx.drawImage(img,(W-dw)/2+val('panx'),(H-dh)/2+val('pany'),dw,dh);
  }
  const id=ctx.getImageData(0,0,W,H);
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
  const buf=new Float32Array(W*H*3);
  const forced=new Int8Array(W*H);
  for(let i=0,j=0,p=0;i<d.length;i+=4,j+=3,p++){
    adjRgb[j]=d[i];adjRgb[j+1]=d[i+1];adjRgb[j+2]=d[i+2];
    const rgb=applyPixel(d[i],d[i+1],d[i+2]);
    forced[p]=mapPixel(d[i],d[i+1],d[i+2]);
    buf[j]=rgb[0];buf[j+1]=rgb[1];buf[j+2]=rgb[2];
  }
  const idx=new Uint8Array(W*H);
  for(let y=0;y<H;y++){
    for(let x=0;x<W;x++){
      const o=(y*W+x)*3;
      const f=forced[y*W+x];
      if(f>=0){
        idx[y*W+x]=f;
        continue;
      }
      let r=buf[o],g=buf[o+1],b=buf[o+2];
      const i=nearest(r,g,b);
      idx[y*W+x]=i;
      if(!dither||!grain) continue;
      const p=PAL[i];
      const fac=val('dith')/100;
      const er=(r-p[0])*fac,eg=(g-p[1])*fac,eb=(b-p[2])*fac;
      const spread=function(dx,dy,fac){
        const xx=x+dx,yy=y+dy;
        if(xx<0||xx>=W||yy<0||yy>=H)return;
        const t=(yy*W+xx)*3;
        buf[t]+=er*fac;buf[t+1]+=eg*fac;buf[t+2]+=eb*fac;
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
  pack(!!dither);
  $('btnUpload').disabled=false;
  $('btnSaveAs').disabled=false;
  $('btnUpload').textContent=editName?'覆盖':'上传';
  $('convInfo').textContent=(grain?(dither?'颗粒 ':'色块 '):'色块 ')+(editName?editName:'新照片');
}
function schedule(dither){
  clearTimeout(tmr);
  tmr=setTimeout(function(){run(dither);}, dither?120:40);
}
function mixLabel(cnt){
  const parts=[];
  for(let i=0;i<4;i++) if(cnt[i]) parts.push(PN[i]+'×'+cnt[i]);
  return parts.join(' ');
}
function sameRgb(u,r,g,b){return u.r===r&&u.g===g&&u.b===b;}
function ruleName(u){
  if(u&&typeof u.mix==='number'&&u.mix>=0&&mixList[u.mix]) return mixLabel(mixList[u.mix]);
  return '取色';
}
function swatchHtml(rgb,label){
  return '<i data-chip style="background:rgb('+rgb[0]+','+rgb[1]+','+rgb[2]+')"></i><span>'+label+'</span>';
}
function lumOf(rgb){return 0.299*rgb[0]+0.587*rgb[1]+0.114*rgb[2];}
function mixHost(rgb){
  return lumOf(rgb)<mixSplit ? $('mixL') : $('mixR');
}
function paintMixes(){
  document.querySelectorAll('#wireWrap [data-m]').forEach(function(btn){
    const mi=+btn.dataset.m;
    const hit=rules.findIndex(function(u){return u.mix===mi;});
    btn.classList.toggle('on', hit>=0);
    btn.classList.toggle('sel', hit===selRule);
  });
  document.querySelectorAll('#wireWrap [data-c]').forEach(function(btn){
    const i=+btn.dataset.c;
    btn.classList.toggle('on', true);
    btn.classList.toggle('sel', i===selRule);
  });
  for(let k=0;k<4;k++) $('map'+k).classList.toggle('arm', k===mapTo);
}
function syncTolPop(){
  const pop=$('tolPop');
  if(!pop) return;
  if(selRule<0||!rules[selRule]){pop.hidden=true;return;}
  const u=rules[selRule];
  $('tol').value=u.tol;
  $('tolLab').textContent=PN[u.to]+' ← '+ruleName(u)+'  容差';
  pop.hidden=false;
  setEm();
}
function drawWires(){
  const wrap=$('wireWrap'),svg=$('wires');
  if(!wrap||!svg) return;
  const wr=wrap.getBoundingClientRect();
  svg.setAttribute('width',Math.max(1,wr.width));
  svg.setAttribute('height',Math.max(1,wr.height));
  let html='';
  rules.forEach(function(u,i){
    const ink=$('map'+u.to);
    let tgt=null;
    if(typeof u.mix==='number'&&u.mix>=0) tgt=document.querySelector('#wireWrap [data-m="'+u.mix+'"] [data-chip]');
    else tgt=document.querySelector('#wireWrap [data-c="'+i+'"] [data-chip]');
    if(!ink||!tgt) return;
    const a=ink.getBoundingClientRect(),b=tgt.getBoundingClientRect();
    const onLeft=!!tgt.closest('#mixL');
    const x1=(onLeft?a.left:a.right)-wr.left,y1=a.top+a.height/2-wr.top;
    const x2=(onLeft?b.right:b.left)-wr.left,y2=b.top+b.height/2-wr.top;
    const p=PAL[u.to];
    const col=(u.to===1)?'#888':'rgb('+p[0]+','+p[1]+','+p[2]+')';
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
  document.querySelectorAll('#wireWrap [data-c]').forEach(function(el){el.remove();});
  rules.forEach(function(u,i){
    if(typeof u.mix==='number'&&u.mix>=0) return;
    const btn=document.createElement('button');
    btn.type='button';
    btn.dataset.c=String(i);
    btn.title='取色 ±'+u.tol;
    const rgb=[Math.round(u.r),Math.round(u.g),Math.round(u.b)];
    btn.innerHTML=swatchHtml(rgb,'取色 '+rgb.join(','));
    btn.onclick=function(){
      if(mapTo<0){selectRule(i,true);return;}
      addOrSelectRule(u.r,u.g,u.b,-1);
    };
    mixHost(rgb).appendChild(btn);
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
function addOrSelectRule(r,g,b,mix){
  if(mapTo<0||mapTo>3){toast('先点中间的原色');return;}
  r=Math.round(r);g=Math.round(g);b=Math.round(b);
  const found=rules.findIndex(function(u){
    if(typeof mix==='number'&&mix>=0) return u.mix===mix;
    return sameRgb(u,r,g,b);
  });
  if(found>=0){
    const prev=rules[found].to;
    rules[found].to=mapTo;
    selRule=found;
    renderRules();
    if(prev!==mapTo) schedule(true);
    return;
  }
  rules.push({r:r,g:g,b:b,tol:val('tol'),to:mapTo,mix:typeof mix==='number'?mix:-1});
  selRule=rules.length-1;
  renderRules();
  schedule(true);
}
function setMapTo(n){
  mapTo=n;
  selRule=-1;
  paintMixes();
  syncTolPop();
  if(n>=0&&n<=3) setTool('pick');
}
function buildMixes(){
  mixList=[];
  for(let a=0;a<=4;a++)
    for(let b=0;b<=4-a;b++)
      for(let c=0;c<=4-a-b;c++)
        mixList.push([a,b,c,4-a-b-c]);
  const items=[];
  mixList.forEach(function(cnt,i){
    if(isPureMix(cnt)) return;
    const rgb=mixRgb(cnt);
    items.push({i:i,rgb:rgb,lum:lumOf(rgb),html:'<button type=button title="'+mixLabel(cnt)+'" data-m="'+i+'">'+swatchHtml(rgb,mixLabel(cnt))+'</button>'});
  });
  items.sort(function(a,b){return a.lum-b.lum;});
  const mid=Math.floor(items.length/2);
  mixSplit=items.length?((items[mid-1].lum+items[mid].lum)/2):128;
  $('mixL').innerHTML=items.slice(0,mid).map(function(it){return it.html;}).join('');
  $('mixR').innerHTML=items.slice(mid).map(function(it){return it.html;}).join('');
  if(!$('wireWrap')._mixClick){
    $('wireWrap')._mixClick=true;
    $('wireWrap').addEventListener('click',function(e){
      const btn=e.target.closest('[data-m]');
      if(!btn) return;
      const mi=+btn.dataset.m;
      const rgb=mixRgb(mixList[mi]);
      addOrSelectRule(rgb[0],rgb[1],rgb[2],mi);
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
  lx=e.clientX;ly=e.clientY;schedule(false);
};
srcCv.onpointerup=function(){if(drag){drag=false;syncCursor();schedule(true);}};
srcCv.onpointercancel=function(){drag=false;syncCursor();};
srcCv.onpointerleave=function(){if(tool==='pick'&&!spaceHeld) $('loupe').style.display='none';};
const pvCv=$('preview');
const pvBox=$('pvInner').parentElement;
function paintAt(e){
  const xy=canvasXY(pvCv,e);
  stampPaint(xy[0],xy[1],val('brush'),tool==='erase'?1:brushCol);
  refreshFinal();
}
pvCv.onpointerdown=function(e){
  if(tool==='pick'){
    const xy=canvasXY(pvCv,e);
    const id=composedIdx();
    const p=PAL[id[xy[1]*W+xy[0]]];
    addOrSelectRule(p[0],p[1],p[2]);
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
  const xy=canvasXY(pvCv,e);
  if(tool==='erase'||tool==='brush') drawBrushCursor(xy[0],xy[1]);
  if(drag&&(tool==='erase'||tool==='brush')) paintAt(e);
};
pvCv.onpointerup=function(){pvDrag=false;drag=false;syncCursor();};
pvCv.onpointercancel=function(){pvDrag=false;drag=false;syncCursor();};
pvCv.onpointerleave=function(){clearCursor();};
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
  if(e.code!=='Space'&&e.key!==' ') return;
  if(typingTarget(e.target)) return;
  e.preventDefault();
  if(!spaceHeld){spaceHeld=true;syncCursor();}
});
window.addEventListener('keyup',function(e){
  if(e.code!=='Space'&&e.key!==' ') return;
  spaceHeld=false;
  if(drag){drag=false;schedule(true);}
  pvDrag=false;
  syncCursor();
});
window.addEventListener('blur',function(){spaceHeld=false;pvDrag=false;syncCursor();});
function histX(e){
  const r=$('hist').getBoundingClientRect();
  return Math.max(0,Math.min(255,Math.round((e.clientX-r.left)*255/r.width)));
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
  let x=histX(e);
  if(histDrag===0){blk=Math.min(x,mid-1); if(blk<0) blk=0;}
  if(histDrag===1){mid=Math.max(blk+1,Math.min(wht-1,x));}
  if(histDrag===2){wht=Math.max(x,mid+1); if(wht>255) wht=255;}
  setEm();drawHist();schedule(false);
};
$('hist').onpointerup=function(){if(histDrag>=0){histDrag=-1;schedule(true);}};
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
  $(id).oninput=function(){setEm();if(id==='brush') return;schedule(false);};
  $(id).onchange=function(){if(id==='brush') return;schedule(true);};
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
$('btnClearMaps').onclick=function(){rules=[];selRule=-1;renderRules();schedule(true);};
for(let i=0;i<4;i++) $('map'+i).onclick=function(){setMapTo(+this.id.slice(3));};
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
$('pick').onchange=function(){
  const f=this.files[0];
  if(!f) return;
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
  im.src=idxToCanvas(idx).toDataURL();
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
  idxToCanvas(decode2bp(buf)).toBlob(function(blob){
    saveBlob(blob, name.replace(/\.2bp$/i,'.png'));
  },'image/png');
}
async function loadSaved(name){
  const resp=await fetch('/album/file?name='+encodeURIComponent(name));
  const buf=new Uint8Array(await resp.arrayBuffer());
  if(buf.length!==FRAME) throw new Error('尺寸不对');
  idxToImage(decode2bp(buf),function(im){loadImage(im,name,true);});
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
        +'<button type=button class=secondary data-dl="'+n+'">下载</button>'
        +'<button type=button class=secondary data-edit="'+n+'">编辑</button>'
        +'<button type=button class=danger data-del="'+n+'">删</button></div></div>';
    }).join('');
    el.querySelectorAll('[data-dl]').forEach(function(btn){
      btn.onclick=function(ev){
        ev.stopPropagation();
        downloadSaved(btn.dataset.dl).then(function(){toast('已开始下载');})
          .catch(function(e){$('convInfo').textContent=e;toast(String(e),true);});
      };
    });
    el.querySelectorAll('[data-del]').forEach(function(btn){
      btn.onclick=async function(ev){
        ev.stopPropagation();
        if(!confirm('删除 '+btn.dataset.del+' ?')) return;
        await fetch('/album/delete?name='+encodeURIComponent(btn.dataset.del),{method:'POST'});
        if(editName===btn.dataset.del) editName='';
        toast('已删除 '+btn.dataset.del);
        refreshList();
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
$('showHeader').onchange=async function(){
  const show=this.checked?1:0;
  await fetch('/album/header?show='+show,{method:'POST'});
  toast(show?'已显示相册标题栏':'已隐藏相册标题栏');
};
$('selAll').onchange=async function(){
  const show=this.checked?1:0;
  this.indeterminate=false;
  toast(show?'正在全选展示…':'正在全部取消展示…');
  try{
    const r=await fetch('/album/show-all?show='+show,{method:'POST'});
    if(!r.ok) throw new Error(await r.text());
    toast(show?'已全部展示':'已全部取消展示');
    refreshList();
  }catch(e){toast(String(e),true);}
};
paint.fill(-1);
buildMixes();
window.addEventListener('resize',function(){requestAnimationFrame(drawWires);});
setTool('pan');setBrushCol(0);setEm();renderRules();drawHist();refreshList();
</script>)ALBUMJS";
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
