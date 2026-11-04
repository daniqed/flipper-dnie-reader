# Retrato ficticio estilo foto de DNI, renderizado con sombreado (no es una persona real)
import numpy as np
from PIL import Image, ImageFilter
W,H=224,256
y,x=np.mgrid[0:H,0:W].astype(np.float32)+0.5
L=np.array([-0.12,-0.18,0.98]); L/=np.linalg.norm(L)  # luz desde arriba-izquierda
def ell(cx,cy,rx,ry): return ((x-cx)/rx)**2+((y-cy)/ry)**2
def soft(d,w=0.06): return np.clip((1-d)/w,0,1)
def shade_ellipsoid(cx,cy,rx,ry,base,amb=0.35):
    d=ell(cx,cy,rx,ry); nz=np.sqrt(np.clip(1-d,0,1))
    nx=(x-cx)/rx; ny=(y-cy)/ry
    n=np.stack([nx,ny,nz]); n/=np.linalg.norm(n,axis=0)+1e-6
    lam=np.clip((n*L[:,None,None]).sum(0),0,1)
    return base*(amb+(1-amb)*lam), d
img=np.full((H,W),228.0)-(y/H)*10                       # fondo claro uniforme
# Hombros y chaqueta oscura con camisa clara
sh,d=shade_ellipsoid(112,300,118,92,95); img=np.where(d<1,sh,img)
shirt=(np.abs(x-112)<(y-196)*0.55)&(y>196); img=np.where(shirt&(d<1),215-(y-196)*0.4,img)
col_l=(x<112)&(x>112-(y-190)*0.9)&(y>190)&(y<228)&(x>112-34); col_r=(x>112)&(x<112+(y-190)*0.9)&(y>190)&(y<228)&(x<112+34)
img=np.where((col_l|col_r)&(d<1),235,img)
# Cuello con sombra bajo la barbilla
nk,dn=shade_ellipsoid(112,190,26,40,215); neck=(np.abs(x-112)<25)&(y>150)&(y<205)
img=np.where(neck,nk*(0.75+0.25*np.clip((y-160)/40,0,1)),img)
# Pelo de detras (melena) con mechones
hair_back,dh=shade_ellipsoid(112,118,74,96,60,0.5)
strands=0.85+0.15*np.sin(x*0.9+np.sin(y*0.05)*3)
img=np.where(dh<1,hair_back*strands,img)
# Orejas
for ex in (66,158):
    e,de=shade_ellipsoid(ex,118,9,16,215); img=np.where(de<1,e,img)
# Cara
fc,df=shade_ellipsoid(112,112,46,60,240,0.72); img=np.where(df<1,fc,img)
# Flequillo y raya al lado
fr,dfr=shade_ellipsoid(104,62,58,30,70,0.55); fringe=(dfr<1)&(y<78+np.sin(x*0.12)*4)
img=np.where(fringe,fr*(0.85+0.15*np.sin(x*1.3)),img)
# Cejas
for cx,s in ((92,1),(132,-1)):
    br=ell(cx,92+(x-cx)*0.08*s,13,3.2); img=np.where(br<1,np.minimum(img,70),img)
# Ojos: parpado, blanco, iris, pupila y brillo
for cx in (92,132):
    lid=ell(cx,104,13,7); img=np.where(lid<1,img*0.82,img)
    wht=ell(cx,105,10.5,5); img=np.where(wht<1,225,img)
    iri=ell(cx,105,5,5); img=np.where((iri<1)&(wht<1),90,img)
    pup=ell(cx,105,2.4,2.4); img=np.where(pup<1,25,img)
    hl=ell(cx-1.8,103.5,1.2,1.2); img=np.where(hl<1,250,img)
    lash=(np.abs(y-(100.5+((x-cx)/10.5)**2*4))<1.1)&(np.abs(x-cx)<11); img=np.where(lash,40,img)
# Nariz: sombra lateral, punta y aletas
nose_side=ell(118,124,5,16); img=np.where(nose_side<1,img*0.88,img)
tip=ell(112,134,8,5); img=np.where(tip<1,img*0.93,img)
for cx in (105,119):
    nos=ell(cx,137,3,2); img=np.where(nos<1,img*0.6,img)
# Boca
up=ell(112,152,14,3.5); lo=ell(112,157,12,4.2)
img=np.where(lo<1,np.minimum(img,150),img); img=np.where(up<1,np.minimum(img,120),img)
img=np.where((np.abs(y-154.5)<0.9)&(np.abs(x-112)<14),85,img)
# Mejillas y sombra de la mandibula
jaw=ell(112,135,44,42); img=np.where((df<1)&(jaw>1),img*0.96,img)
im=Image.fromarray(np.clip(img,0,255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
im.save('face_big.png')
small=im.resize((56,64),Image.LANCZOS); small.save('face_small.png')
a=np.array(small)
# 4 bits por pixel
q=(a.astype(np.int32)*15+127)//255
packed=[(int(q.flat[i])<<4)|int(q.flat[i+1]) for i in range(0,q.size,2)]
body=',\n    '.join(', '.join('0x%02X'%b for b in packed[i:i+16]) for i in range(0,len(packed),16))
open('demo_face.inc','w').write('// Retrato ficticio para el modo demo (generado con tools/demo_face.py), 56x64, 4 bits\n#define DEMO_FACE_W 56\n#define DEMO_FACE_H 64\nstatic const uint8_t DEMO_FACE[] = {\n    %s,\n};\n'%body)
im.resize((112,128),Image.LANCZOS).resize((448,512),Image.NEAREST).save('preview.png')
