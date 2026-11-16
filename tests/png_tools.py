"""PNG screenshot reader used by pixel integration tests (standard library only)."""
import struct
import zlib


def pixels(path):
    data=path.read_bytes();offset=8;compressed=b""
    while offset<len(data):
        length=struct.unpack_from(">I",data,offset)[0];tag=data[offset+4:offset+8];payload=data[offset+8:offset+8+length];offset+=length+12
        if tag==b"IHDR":
            width,height,bits,kind,_,_,interlace=struct.unpack(">IIBBBBB",payload)
            assert bits==8 and kind in (2,6) and interlace==0
        if tag==b"IDAT":compressed+=payload
    raw=zlib.decompress(compressed);channels=4 if kind==6 else 3;stride=width*channels;previous=bytearray(stride);rows=[];offset=0
    for _ in range(height):
        method=raw[offset];row=bytearray(raw[offset+1:offset+1+stride]);offset+=stride+1
        for x in range(stride):
            a=row[x-channels] if x>=channels else 0;b=previous[x];c=previous[x-channels] if x>=channels else 0
            p=a+b-c;choices=(a,b,c);paeth=choices[min(range(3),key=lambda i:abs(p-choices[i]))]
            row[x]=(row[x]+(0,a,b,(a+b)//2,paeth)[method])%256
        rows.append(row);previous=row
    return width,height,lambda x,y:tuple(rows[y][x*channels:x*channels+3])
