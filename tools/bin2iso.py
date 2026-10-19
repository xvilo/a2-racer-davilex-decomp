import sys
src, dst = sys.argv[1], sys.argv[2]
SYNC = b'\x00' + b'\xff'*10 + b'\x00'
stats = {'form1':0,'form2':0,'nosync':0,'mode':{}}
with open(src,'rb') as f, open(dst,'wb') as o:
    while True:
        s = f.read(2352)
        if len(s) < 2352: break
        if s[:12] != SYNC:
            stats['nosync'] += 1; o.write(b'\0'*2048); continue
        mode = s[15]; stats['mode'][mode] = stats['mode'].get(mode,0)+1
        if mode == 1: o.write(s[16:16+2048]); stats['form1']+=1
        else:
            if s[18] & 0x20: stats['form2'] += 1
            else: stats['form1'] += 1
            o.write(s[24:24+2048])
print(stats)
