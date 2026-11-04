import mido
# pais: (midi, negra en ms, maximo de tiempos)
CFG = {'US':('US_m.midi',420,40),'FR':('FR_m.midi',380,40),'GB':('GB_m.midi',450,40),
       'CA':('CA_m.midi',380,32),'JP':('JP_m.midi',520,24),'CN':('CN_m.midi',420,24),
       'ID':('ID_m.midi',380,33),'DE':('DE_m.midi',450,18),'IT':('IT_m.midi',380,40),'NL':('NL_m.midi',450,40)}
out = []
for k,(f,q,maxb) in CFG.items():
    m = mido.MidiFile(f); tpb = m.ticks_per_beat
    notes=[]  # (start, end, pitch)
    for tr in m.tracks:
        t=0; on={}
        for msg in tr:
            t+=msg.time
            if msg.type=='note_on' and msg.velocity>0: on[msg.note]=t
            elif msg.type in('note_off','note_on') and msg.note in on:
                notes.append((on.pop(msg.note),t,msg.note))
    notes.sort()
    # monofonico: la nota mas aguda en cada inicio
    mono={}
    for s,e,p in notes:
        if s not in mono or p>mono[s][1]: mono[s]=(e,p)
    seq=[]; cur=0
    starts=sorted(mono)
    if starts and starts[0]>0: cur=starts[0]  # sin silencio inicial
    for s in starts:
        e,p=mono[s]
        if s/tpb > maxb + starts[0]/tpb: break
        if s>cur: seq.append((0,(s-cur)/tpb))
        seq.append((440*2**((p-69)/12),(e-s)/tpb)); cur=e
    while seq and seq[-1][0]==0: seq.pop()
    # transponer a una octava comoda para el zumbador (media entre 450 y 900 Hz)
    fs=[x for x,_ in seq if x]; avg=sum(fs)/len(fs); sh=1.0
    while avg*sh<450: sh*=2
    while avg*sh>900: sh/=2
    body=',\n    '.join('{%.2ff, %d}'%(fr*sh,round(b*q)) for fr,b in seq)
    total=sum(round(b*q) for _,b in seq)
    out.append('// %s: %d notas, %.1f s\nstatic const Note ANTHEM_%s[] = {\n    %s,\n};\n'%(k,len(seq),total/1000,k,body))
    print(k,len(seq),'notas',round(total/1000,1),'s')
open('anthems.inc','w').write('\n'.join(out))
