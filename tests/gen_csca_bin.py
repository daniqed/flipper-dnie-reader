# csca.bin: CSCA de los paises principales (Master List del BSI), formato compacto:
#   repetido: u16 len_registro | u8 tipo (1=RSA, 2=otro) | 'CC' | u8 len_nombre | nombre
#             | u16 len_subject | subject DER | [RSA: u16 len_n | n | u8 len_e | e]
import sys, datetime, struct
from cryptography import x509
from cryptography.hazmat.primitives.asymmetric import rsa
d=open('/tmp/csca_dl/ml.content','rb').read()
def tlv(b,i):
    t=b[i];i+=1;l=b[i];i+=1
    if l&0x80: n=l&0x7f;l=int.from_bytes(b[i:i+n],'big');i+=n
    return t,i,l
t,i,l=tlv(d,0); t,j,l2=tlv(d,i); i=j+l2; t,i,l=tlv(d,i); end=i+l
TOP=['US','CN','DE','JP','IN','GB','FR','IT','CA','BR','RU','MX','AU','KR','ID','NL','TR','SA','CH','AR']
now=datetime.datetime(2026,11,4,tzinfo=datetime.timezone.utc)
out=b''; seen=set(); stats={}
certs=[]
while i<end:
    t,j,l=tlv(d,i); der=d[i:j+l]; i=j+l
    try: c=x509.load_der_x509_certificate(der)
    except Exception: continue
    try: cc=c.subject.get_attributes_for_oid(x509.oid.NameOID.COUNTRY_NAME)[0].value.upper()
    except Exception: continue
    if cc not in TOP or c.not_valid_after_utc<now: continue
    try: pk=c.public_key()
    except Exception: pk=None
    subj=c.subject.public_bytes()
    try: cn=c.subject.get_attributes_for_oid(x509.oid.NameOID.COMMON_NAME)[0].value
    except Exception: cn='CSCA '+cc
    cn=cn.encode('ascii','replace')[:40]
    if isinstance(pk,rsa.RSAPublicKey):
        pn=pk.public_numbers(); key=(cc,pn.n)
        if key in seen: continue
        seen.add(key)
        nb=pn.n.to_bytes((pn.n.bit_length()+7)//8,'big'); eb=pn.e.to_bytes((pn.e.bit_length()+7)//8,'big')
        body=bytes([1])+cc.encode()+bytes([len(cn)])+cn+struct.pack('>H',len(subj))+subj+struct.pack('>H',len(nb))+nb+bytes([len(eb)])+eb
    else:
        key=(cc,subj)
        if key in seen: continue
        seen.add(key)
        body=bytes([2])+cc.encode()+bytes([len(cn)])+cn+struct.pack('>H',len(subj))+subj
    out+=struct.pack('>H',len(body))+body
    stats[cc]=stats.get(cc,0)+1
    certs.append(der)
open(sys.argv[1],'wb').write(out)
# Todos los certificados (para el test: autofirmados y enlaces)
open('/tmp/csca_tools/top_certs.bin','wb').write(b''.join(struct.pack('>H',len(x))+x for x in certs))
print(len(out),'bytes', stats)
