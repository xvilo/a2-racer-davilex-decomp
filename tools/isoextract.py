import sys, os, struct
iso, out = sys.argv[1], sys.argv[2]
f = open(iso,'rb'); B=2048
def rd(lba,n): f.seek(lba*B); return f.read(n)
# prefer Joliet SVD, else PVD
root=None; joliet=False; lba=16
while True:
    d=rd(lba,B)
    if d[1:6]!=b'CD001' or d[0]==255: break
    if d[0]==1 and root is None: root=d[156:190]
    if d[0]==2 and d[88:91] in (b'%/@',b'%/C',b'%/E'): root=d[156:190]; joliet=True
    lba+=1
print('joliet',joliet)
n=0
def walk(rec, path):
    global n
    ext,size=struct.unpack_from('<I',rec,2)[0],struct.unpack_from('<I',rec,10)[0]
    data=rd(ext,size); i=0
    while i<len(data):
        L=data[i]
        if L==0: i=(i//B+1)*B; continue
        r=data[i:i+L]; i+=L
        nl=r[32]; name=r[33:33+nl]
        if name in (b'\0',b'\1'): continue
        name=name.decode('utf-16-be') if joliet else name.decode('latin-1')
        name=name.split(';')[0].rstrip('.') or '_'
        assert '/' not in name and name not in ('.','..')
        p=os.path.join(path,name)
        if r[25]&2: os.makedirs(p,exist_ok=True); walk(r,p)
        else:
            e,s=struct.unpack_from('<I',r,2)[0],struct.unpack_from('<I',r,10)[0]
            with open(p,'wb') as o: o.write(rd(e,s)); n+=1
os.makedirs(out,exist_ok=True); walk(root,out); print('files',n)
