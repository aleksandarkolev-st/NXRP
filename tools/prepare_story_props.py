"""Convert the attributed downloaded story props; never generate geometry.

Requires numpy, Pillow and pygltflib. See assets/models/STORY_SOURCES.md.
Source paths are under build/asset-sources/story and construction-site-cabin.glb.
"""
import io
import json
import math
import struct
from pathlib import Path
import numpy as np
from PIL import Image
from pygltflib import GLTF2


def convert(path, name, span, damaged=False, door_only=False, node_names=None):
    model=GLTF2().load(str(path))
    buffers=[model.binary_blob() if not b.uri else (path.parent/b.uri).read_bytes() for b in model.buffers]
    def read(index):
        a=model.accessors[index];v=model.bufferViews[a.bufferView]
        dtype=np.dtype({5121:'<u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a.componentType])
        width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a.type]
        return np.ndarray((a.count,width),dtype=dtype,buffer=buffers[v.buffer],offset=(v.byteOffset or 0)+(a.byteOffset or 0),strides=(v.byteStride or width*dtype.itemsize,dtype.itemsize)).copy()
    parents={child:i for i,n in enumerate(model.nodes) for child in n.children or []}
    worlds={}
    def world(i):
        if i not in worlds:
            n=model.nodes[i]
            if n.matrix:local=np.array(n.matrix).reshape(4,4).T
            else:
                x,y,z,w=n.rotation or [0,0,0,1]
                rotation=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
                local=np.eye(4);local[:3,:3]=rotation@np.diag(n.scale or [1,1,1]);local[:3,3]=n.translation or [0,0,0]
            worlds[i]=world(parents[i])@local if i in parents else local
        return worlds[i]
    grid=math.ceil(math.sqrt(len(model.materials)));tile=512
    atlas=Image.new('RGB',(grid*tile,grid*tile))
    for i,m in enumerate(model.materials):
        p=m.pbrMetallicRoughness;factor=np.array((p.baseColorFactor or [1,1,1,1])[:3])**(1/2.2)
        if p.baseColorTexture:
            image=model.images[model.textures[p.baseColorTexture.index].source]
            if image.uri:im=Image.open(path.parent/image.uri)
            else:
                v=model.bufferViews[image.bufferView];im=Image.open(io.BytesIO(buffers[v.buffer][v.byteOffset:v.byteOffset+v.byteLength]))
            im=im.convert('RGB').resize((tile-6,tile-6),Image.Resampling.LANCZOS)
        else:im=Image.new('RGB',(tile-6,tile-6),'white')
        pixels=np.clip(np.asarray(im)*factor,0,255).astype('uint8')
        if damaged:pixels=(pixels*np.array([.52,.40,.30])).astype('uint8')
        # Extend edge texels so bilinear sampling cannot bleed adjacent materials.
        padded=np.pad(pixels,((3,3),(3,3),(0,0)),mode='edge')
        atlas.paste(Image.fromarray(padded),((i%grid)*tile,(i//grid)*tile))
    chunks=[]
    selected_chunks=[]
    for i,node in enumerate(model.nodes):
        if node.mesh is None:continue
        label=node.name or ''
        if door_only and label!='Cabin closed finite entry door':continue
        if damaged and ('door' in label.lower() or 'handle' in label.lower() or 'corner post 2960' in label):continue
        transform=world(i)
        for primitive in model.meshes[node.mesh].primitives:
            if primitive.mode not in (None,4):raise ValueError('Non-triangle source')
            material=primitive.material or 0
            factor=model.materials[material].pbrMetallicRoughness.baseColorFactor or [1,1,1,1]
            if factor[3]<.5:continue
            p=read(primitive.attributes.POSITION);n=read(primitive.attributes.NORMAL)
            uv=read(primitive.attributes.TEXCOORD_0) if primitive.attributes.TEXCOORD_0 is not None else np.full((len(p),2),.5)
            faces=read(primitive.indices).reshape(-1,3).astype('uint32') if primitive.indices is not None else np.arange(len(p),dtype='uint32').reshape(-1,3)
            p=np.column_stack((p,np.ones(len(p))))@transform.T;p=p[:,:3]
            n=n@np.linalg.inv(transform[:3,:3]);n/=np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-9)
            if np.linalg.det(transform[:3,:3])<0:faces=faces[:,::-1]
            if damaged and 'flat roof' in label:
                angle=.21;r=np.array([[np.cos(angle),-np.sin(angle),0],[np.sin(angle),np.cos(angle),0],[0,0,1]])
                pivot=p.mean(0);p=(p-pivot)@r.T+pivot;p[:,1]-=1.05;n=n@r.T
            if damaged and 'finite walls' in label:
                centres=p[faces].mean(axis=1)
                faces=faces[~((centres[:,0]>2.7)&(centres[:,1]>1.1))]
            if door_only:
                p=np.column_stack((p[:,2],p[:,1]*.25,-p[:,0]))
                n=np.column_stack((n[:,2],n[:,1]*4.,-n[:,0]));n/=np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-9)
                uv=(p[:,:2]-p[:,:2].min(0))/np.maximum(np.ptp(p[:,:2],axis=0),1e-9)
                # SFML textures use top-origin normalized coordinates when bound.
                uv[:,1]=1.-uv[:,1]
            uv=np.clip(uv,0,1)
            if not door_only:
                uv[:,0]=(material%grid+(3+uv[:,0]*(tile-7))/tile)/grid
                uv[:,1]=(material//grid+(3+uv[:,1]*(tile-7))/tile)/grid
            chunks.append((p,n,uv,faces))
            if node_names is None or label in node_names:selected_chunks.append((p,n,uv,faces))
    all_points=np.concatenate([c[0] for c in chunks]);low=all_points.min(0);high=all_points.max(0)
    origin=np.array([(low[0]+high[0])*.5,low[1],(low[2]+high[2])*.5])
    scale=span/max((high-low)[0],(high-low)[2])
    vertices=[];indices=[];offset=0
    for p,n,uv,faces in selected_chunks:
        vertices.append(np.concatenate(((p-origin)*scale,n,uv),axis=1));indices.append(faces+offset);offset+=len(p)
    vertices=np.concatenate(vertices).astype('<f4');indices=np.concatenate(indices).astype('<u4').reshape(-1)
    out=Path('assets/models');out.mkdir(exist_ok=True)
    with (out/(name+'.nxm')).open('wb') as f:
        f.write(b'NXM1'+struct.pack('<II',len(vertices),len(indices)));f.write(vertices.tobytes());f.write(indices.tobytes())
    atlas.save(out/(name+'.png'),optimize=True)
    print(name,len(vertices),len(indices)//3,'bounds',((high-low)*scale).round(3))


if __name__=='__main__':
    root=Path('build/asset-sources/story')
    for asset,name,span in [('vintage_suitcase','story-luggage',.42),('metal_toolbox','story-toolbox',.40),('portable_cassette_player','story-player',.30)]:
        convert(root/asset/(asset+'_1k.gltf'),name,span)
    cabin=Path('build/asset-sources/construction-site-cabin.glb')
    convert(cabin,'story-ruin',3.2,damaged=True)
    convert(cabin,'story-sign',.6,door_only=True)
    textures=Path('assets/textures');
    for source,out in [('footprints.png','story-foot-left.png'),('rightfootprint.png','story-foot-right.png')]:
        image=Image.open(Path('build/asset-sources')/source).convert('RGBA');image.thumbnail((256,256));image.save(textures/out,optimize=True)
