import re, subprocess, sys, mido, os
var = {'CA':'soprano','JP':'sopranoVoice','ID':'melody'}
for k in ['CA','JP','CN','ID','DE','IT','NL']:
    src = open(k+'.ly').read()
    if k in var:
        ly = '\\version "2.24.0"\n' + src + '\n\\score { \\unfoldRepeats \\new Staff \\%s \\midi { } }\n' % var[k]
    else:
        body = re.sub(r'\\layout\s*\{(?:[^{}]|\{[^{}]*\})*\}', '', src)
        ly = '\\version "2.24.0"\n\\score { \\unfoldRepeats { %s } \\midi { } }\n' % body
    open(k+'_m.ly','w').write(ly)
    r = subprocess.run(['lilypond','-dno-print-pages','-s','-o',k+'_m',k+'_m.ly'],capture_output=True,text=True)
    mid = k+'_m.midi' if os.path.exists(k+'_m.midi') else k+'_m.mid'
    print(k, 'ok' if os.path.exists(mid) else 'ERROR '+r.stderr[-300:])
