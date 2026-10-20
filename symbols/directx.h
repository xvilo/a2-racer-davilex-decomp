/*
 * DirectX interfaces and structures used by A2 Racer (DirectX 3 era: DirectDraw,
 * DirectDraw2, Direct3D immediate mode with execute buffers, DirectSound with 3D
 * buffers, DirectInput). Written from the DirectX 3/5 SDK layouts for 32-bit x86;
 * ApplyWatcom.java parses this file into each Ghidra program so that COM calls read
 * as obj->lpVtbl->Method(obj, ...) instead of raw vtable offsets.
 *
 * Plain C only (Ghidra's C parser runs without a preprocessor). Windows base types
 * (DWORD, HRESULT, GUID, RECT, HWND, ...) come from Ghidra's windows_vs12_32 archive.
 * Struct tags match the names in A2MENU.DAT's debug info (_DDPIXELFORMAT, ...).
 */


/* ---------------------------------------------------------------- DirectDraw */

typedef struct _DDCOLORKEY {
	DWORD dwColorSpaceLowValue;
	DWORD dwColorSpaceHighValue;
} DDCOLORKEY;

typedef struct _DDSCAPS {
	DWORD dwCaps;
} DDSCAPS;

typedef struct _DDPIXELFORMAT {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;      /* also dwYUVBitCount, dwZBufferBitDepth, dwAlphaBitDepth */
	DWORD dwRBitMask;         /* also dwYBitMask */
	DWORD dwGBitMask;         /* also dwUBitMask */
	DWORD dwBBitMask;         /* also dwVBitMask */
	DWORD dwRGBAlphaBitMask;  /* also dwYUVAlphaBitMask */
} DDPIXELFORMAT;

typedef struct _DDSURFACEDESC {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwHeight;
	DWORD dwWidth;
	LONG lPitch;              /* also dwLinearSize */
	DWORD dwBackBufferCount;
	DWORD dwZBufferBitDepth;  /* also dwMipMapCount, dwRefreshRate */
	DWORD dwAlphaBitDepth;
	DWORD dwReserved;
	void *lpSurface;
	DDCOLORKEY ddckCKDestOverlay;
	DDCOLORKEY ddckCKDestBlt;
	DDCOLORKEY ddckCKSrcOverlay;
	DDCOLORKEY ddckCKSrcBlt;
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS ddsCaps;
} DDSURFACEDESC;

typedef struct _DDBLTFX {
	DWORD dwSize;
	DWORD dwDDFX;
	DWORD dwROP;
	DWORD dwDDROP;
	DWORD dwRotationAngle;
	DWORD dwZBufferOpCode;
	DWORD dwZBufferLow;
	DWORD dwZBufferHigh;
	DWORD dwZBufferBaseDest;
	DWORD dwZDestConstBitDepth;
	DWORD dwZDestConst;
	DWORD dwZSrcConstBitDepth;
	DWORD dwZSrcConst;
	DWORD dwAlphaEdgeBlendBitDepth;
	DWORD dwAlphaEdgeBlend;
	DWORD dwReserved;
	DWORD dwAlphaDestConstBitDepth;
	DWORD dwAlphaDestConst;
	DWORD dwAlphaSrcConstBitDepth;
	DWORD dwAlphaSrcConst;
	DWORD dwFillColor;        /* also dwFillDepth, lpDDSPattern */
	DDCOLORKEY ddckDestColorkey;
	DDCOLORKEY ddckSrcColorkey;
} DDBLTFX;

typedef struct IDirectDraw IDirectDraw;
typedef struct IDirectDraw2 IDirectDraw2;
typedef struct IDirectDrawSurface IDirectDrawSurface;
typedef struct IDirectDrawPalette IDirectDrawPalette;
typedef struct IDirectDrawClipper IDirectDrawClipper;

typedef struct IDirectDrawVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectDraw *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectDraw *This);
	ULONG (__stdcall *Release)(IDirectDraw *This);
	HRESULT (__stdcall *Compact)(IDirectDraw *This);
	HRESULT (__stdcall *CreateClipper)(IDirectDraw *This, DWORD dwFlags, IDirectDrawClipper **lplpDDClipper, void *pUnkOuter);
	HRESULT (__stdcall *CreatePalette)(IDirectDraw *This, DWORD dwFlags, PALETTEENTRY *lpColorTable, IDirectDrawPalette **lplpDDPalette, void *pUnkOuter);
	HRESULT (__stdcall *CreateSurface)(IDirectDraw *This, DDSURFACEDESC *lpDDSurfaceDesc, IDirectDrawSurface **lplpDDSurface, void *pUnkOuter);
	HRESULT (__stdcall *DuplicateSurface)(IDirectDraw *This, IDirectDrawSurface *lpDDSurface, IDirectDrawSurface **lplpDupDDSurface);
	HRESULT (__stdcall *EnumDisplayModes)(IDirectDraw *This, DWORD dwFlags, DDSURFACEDESC *lpDDSurfaceDesc, void *lpContext, void *lpEnumModesCallback);
	HRESULT (__stdcall *EnumSurfaces)(IDirectDraw *This, DWORD dwFlags, DDSURFACEDESC *lpDDSD, void *lpContext, void *lpEnumSurfacesCallback);
	HRESULT (__stdcall *FlipToGDISurface)(IDirectDraw *This);
	HRESULT (__stdcall *GetCaps)(IDirectDraw *This, void *lpDDDriverCaps, void *lpDDHELCaps);
	HRESULT (__stdcall *GetDisplayMode)(IDirectDraw *This, DDSURFACEDESC *lpDDSurfaceDesc);
	HRESULT (__stdcall *GetFourCCCodes)(IDirectDraw *This, DWORD *lpNumCodes, DWORD *lpCodes);
	HRESULT (__stdcall *GetGDISurface)(IDirectDraw *This, IDirectDrawSurface **lplpGDIDDSSurface);
	HRESULT (__stdcall *GetMonitorFrequency)(IDirectDraw *This, DWORD *lpdwFrequency);
	HRESULT (__stdcall *GetScanLine)(IDirectDraw *This, DWORD *lpdwScanLine);
	HRESULT (__stdcall *GetVerticalBlankStatus)(IDirectDraw *This, BOOL *lpbIsInVB);
	HRESULT (__stdcall *Initialize)(IDirectDraw *This, GUID *lpGUID);
	HRESULT (__stdcall *RestoreDisplayMode)(IDirectDraw *This);
	HRESULT (__stdcall *SetCooperativeLevel)(IDirectDraw *This, HWND hWnd, DWORD dwFlags);
	HRESULT (__stdcall *SetDisplayMode)(IDirectDraw *This, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP);
	HRESULT (__stdcall *WaitForVerticalBlank)(IDirectDraw *This, DWORD dwFlags, HANDLE hEvent);
} IDirectDrawVtbl;
struct IDirectDraw { IDirectDrawVtbl *lpVtbl; };

typedef struct IDirectDraw2Vtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectDraw2 *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectDraw2 *This);
	ULONG (__stdcall *Release)(IDirectDraw2 *This);
	HRESULT (__stdcall *Compact)(IDirectDraw2 *This);
	HRESULT (__stdcall *CreateClipper)(IDirectDraw2 *This, DWORD dwFlags, IDirectDrawClipper **lplpDDClipper, void *pUnkOuter);
	HRESULT (__stdcall *CreatePalette)(IDirectDraw2 *This, DWORD dwFlags, PALETTEENTRY *lpColorTable, IDirectDrawPalette **lplpDDPalette, void *pUnkOuter);
	HRESULT (__stdcall *CreateSurface)(IDirectDraw2 *This, DDSURFACEDESC *lpDDSurfaceDesc, IDirectDrawSurface **lplpDDSurface, void *pUnkOuter);
	HRESULT (__stdcall *DuplicateSurface)(IDirectDraw2 *This, IDirectDrawSurface *lpDDSurface, IDirectDrawSurface **lplpDupDDSurface);
	HRESULT (__stdcall *EnumDisplayModes)(IDirectDraw2 *This, DWORD dwFlags, DDSURFACEDESC *lpDDSurfaceDesc, void *lpContext, void *lpEnumModesCallback);
	HRESULT (__stdcall *EnumSurfaces)(IDirectDraw2 *This, DWORD dwFlags, DDSURFACEDESC *lpDDSD, void *lpContext, void *lpEnumSurfacesCallback);
	HRESULT (__stdcall *FlipToGDISurface)(IDirectDraw2 *This);
	HRESULT (__stdcall *GetCaps)(IDirectDraw2 *This, void *lpDDDriverCaps, void *lpDDHELCaps);
	HRESULT (__stdcall *GetDisplayMode)(IDirectDraw2 *This, DDSURFACEDESC *lpDDSurfaceDesc);
	HRESULT (__stdcall *GetFourCCCodes)(IDirectDraw2 *This, DWORD *lpNumCodes, DWORD *lpCodes);
	HRESULT (__stdcall *GetGDISurface)(IDirectDraw2 *This, IDirectDrawSurface **lplpGDIDDSSurface);
	HRESULT (__stdcall *GetMonitorFrequency)(IDirectDraw2 *This, DWORD *lpdwFrequency);
	HRESULT (__stdcall *GetScanLine)(IDirectDraw2 *This, DWORD *lpdwScanLine);
	HRESULT (__stdcall *GetVerticalBlankStatus)(IDirectDraw2 *This, BOOL *lpbIsInVB);
	HRESULT (__stdcall *Initialize)(IDirectDraw2 *This, GUID *lpGUID);
	HRESULT (__stdcall *RestoreDisplayMode)(IDirectDraw2 *This);
	HRESULT (__stdcall *SetCooperativeLevel)(IDirectDraw2 *This, HWND hWnd, DWORD dwFlags);
	HRESULT (__stdcall *SetDisplayMode)(IDirectDraw2 *This, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags);
	HRESULT (__stdcall *WaitForVerticalBlank)(IDirectDraw2 *This, DWORD dwFlags, HANDLE hEvent);
	HRESULT (__stdcall *GetAvailableVidMem)(IDirectDraw2 *This, DDSCAPS *lpDDSCaps, DWORD *lpdwTotal, DWORD *lpdwFree);
} IDirectDraw2Vtbl;
struct IDirectDraw2 { IDirectDraw2Vtbl *lpVtbl; };

typedef struct IDirectDrawSurfaceVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectDrawSurface *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectDrawSurface *This);
	ULONG (__stdcall *Release)(IDirectDrawSurface *This);
	HRESULT (__stdcall *AddAttachedSurface)(IDirectDrawSurface *This, IDirectDrawSurface *lpDDSAttachedSurface);
	HRESULT (__stdcall *AddOverlayDirtyRect)(IDirectDrawSurface *This, RECT *lpRect);
	HRESULT (__stdcall *Blt)(IDirectDrawSurface *This, RECT *lpDestRect, IDirectDrawSurface *lpDDSrcSurface, RECT *lpSrcRect, DWORD dwFlags, DDBLTFX *lpDDBltFx);
	HRESULT (__stdcall *BltBatch)(IDirectDrawSurface *This, void *lpDDBltBatch, DWORD dwCount, DWORD dwFlags);
	HRESULT (__stdcall *BltFast)(IDirectDrawSurface *This, DWORD dwX, DWORD dwY, IDirectDrawSurface *lpDDSrcSurface, RECT *lpSrcRect, DWORD dwTrans);
	HRESULT (__stdcall *DeleteAttachedSurface)(IDirectDrawSurface *This, DWORD dwFlags, IDirectDrawSurface *lpDDSAttachedSurface);
	HRESULT (__stdcall *EnumAttachedSurfaces)(IDirectDrawSurface *This, void *lpContext, void *lpEnumSurfacesCallback);
	HRESULT (__stdcall *EnumOverlayZOrders)(IDirectDrawSurface *This, DWORD dwFlags, void *lpContext, void *lpfnCallback);
	HRESULT (__stdcall *Flip)(IDirectDrawSurface *This, IDirectDrawSurface *lpDDSurfaceTargetOverride, DWORD dwFlags);
	HRESULT (__stdcall *GetAttachedSurface)(IDirectDrawSurface *This, DDSCAPS *lpDDSCaps, IDirectDrawSurface **lplpDDAttachedSurface);
	HRESULT (__stdcall *GetBltStatus)(IDirectDrawSurface *This, DWORD dwFlags);
	HRESULT (__stdcall *GetCaps)(IDirectDrawSurface *This, DDSCAPS *lpDDSCaps);
	HRESULT (__stdcall *GetClipper)(IDirectDrawSurface *This, IDirectDrawClipper **lplpDDClipper);
	HRESULT (__stdcall *GetColorKey)(IDirectDrawSurface *This, DWORD dwFlags, DDCOLORKEY *lpDDColorKey);
	HRESULT (__stdcall *GetDC)(IDirectDrawSurface *This, HDC *lphDC);
	HRESULT (__stdcall *GetFlipStatus)(IDirectDrawSurface *This, DWORD dwFlags);
	HRESULT (__stdcall *GetOverlayPosition)(IDirectDrawSurface *This, LONG *lplX, LONG *lplY);
	HRESULT (__stdcall *GetPalette)(IDirectDrawSurface *This, IDirectDrawPalette **lplpDDPalette);
	HRESULT (__stdcall *GetPixelFormat)(IDirectDrawSurface *This, DDPIXELFORMAT *lpDDPixelFormat);
	HRESULT (__stdcall *GetSurfaceDesc)(IDirectDrawSurface *This, DDSURFACEDESC *lpDDSurfaceDesc);
	HRESULT (__stdcall *Initialize)(IDirectDrawSurface *This, IDirectDraw *lpDD, DDSURFACEDESC *lpDDSurfaceDesc);
	HRESULT (__stdcall *IsLost)(IDirectDrawSurface *This);
	HRESULT (__stdcall *Lock)(IDirectDrawSurface *This, RECT *lpDestRect, DDSURFACEDESC *lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);
	HRESULT (__stdcall *ReleaseDC)(IDirectDrawSurface *This, HDC hDC);
	HRESULT (__stdcall *Restore)(IDirectDrawSurface *This);
	HRESULT (__stdcall *SetClipper)(IDirectDrawSurface *This, IDirectDrawClipper *lpDDClipper);
	HRESULT (__stdcall *SetColorKey)(IDirectDrawSurface *This, DWORD dwFlags, DDCOLORKEY *lpDDColorKey);
	HRESULT (__stdcall *SetOverlayPosition)(IDirectDrawSurface *This, LONG lX, LONG lY);
	HRESULT (__stdcall *SetPalette)(IDirectDrawSurface *This, IDirectDrawPalette *lpDDPalette);
	HRESULT (__stdcall *Unlock)(IDirectDrawSurface *This, void *lpSurfaceData);
	HRESULT (__stdcall *UpdateOverlay)(IDirectDrawSurface *This, RECT *lpSrcRect, IDirectDrawSurface *lpDDDestSurface, RECT *lpDestRect, DWORD dwFlags, void *lpDDOverlayFx);
	HRESULT (__stdcall *UpdateOverlayDisplay)(IDirectDrawSurface *This, DWORD dwFlags);
	HRESULT (__stdcall *UpdateOverlayZOrder)(IDirectDrawSurface *This, DWORD dwFlags, IDirectDrawSurface *lpDDSReference);
} IDirectDrawSurfaceVtbl;
struct IDirectDrawSurface { IDirectDrawSurfaceVtbl *lpVtbl; };

typedef struct IDirectDrawPaletteVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectDrawPalette *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectDrawPalette *This);
	ULONG (__stdcall *Release)(IDirectDrawPalette *This);
	HRESULT (__stdcall *GetCaps)(IDirectDrawPalette *This, DWORD *lpdwCaps);
	HRESULT (__stdcall *GetEntries)(IDirectDrawPalette *This, DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, PALETTEENTRY *lpEntries);
	HRESULT (__stdcall *Initialize)(IDirectDrawPalette *This, IDirectDraw *lpDD, DWORD dwFlags, PALETTEENTRY *lpDDColorTable);
	HRESULT (__stdcall *SetEntries)(IDirectDrawPalette *This, DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, PALETTEENTRY *lpEntries);
} IDirectDrawPaletteVtbl;
struct IDirectDrawPalette { IDirectDrawPaletteVtbl *lpVtbl; };

typedef struct IDirectDrawClipperVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectDrawClipper *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectDrawClipper *This);
	ULONG (__stdcall *Release)(IDirectDrawClipper *This);
	HRESULT (__stdcall *GetClipList)(IDirectDrawClipper *This, RECT *lpRect, void *lpClipList, DWORD *lpdwSize);
	HRESULT (__stdcall *GetHWnd)(IDirectDrawClipper *This, HWND *lphWnd);
	HRESULT (__stdcall *Initialize)(IDirectDrawClipper *This, IDirectDraw *lpDD, DWORD dwFlags);
	HRESULT (__stdcall *IsClipListChanged)(IDirectDrawClipper *This, BOOL *lpbChanged);
	HRESULT (__stdcall *SetClipList)(IDirectDrawClipper *This, void *lpClipList, DWORD dwFlags);
	HRESULT (__stdcall *SetHWnd)(IDirectDrawClipper *This, DWORD dwFlags, HWND hWnd);
} IDirectDrawClipperVtbl;
struct IDirectDrawClipper { IDirectDrawClipperVtbl *lpVtbl; };

/* -------------------------------------------------- Direct3D (immediate mode, DX3) */

typedef struct _D3DVECTOR {
	float x;
	float y;
	float z;
} D3DVECTOR;

typedef struct _D3DCOLORVALUE {
	float r;
	float g;
	float b;
	float a;
} D3DCOLORVALUE;

typedef struct _D3DRECT {
	LONG x1;
	LONG y1;
	LONG x2;
	LONG y2;
} D3DRECT;

typedef struct _D3DMATRIX {
	float _11; float _12; float _13; float _14;
	float _21; float _22; float _23; float _24;
	float _31; float _32; float _33; float _34;
	float _41; float _42; float _43; float _44;
} D3DMATRIX;

/* Transformed and lit vertex: screen x/y, z-buffer depth, 1/w, colours, texture u/v. */
typedef struct _D3DTLVERTEX {
	float sx;
	float sy;
	float sz;
	float rhw;
	DWORD color;
	DWORD specular;
	float tu;
	float tv;
} D3DTLVERTEX;

typedef struct _D3DLVERTEX {
	float x;
	float y;
	float z;
	DWORD dwReserved;
	DWORD color;
	DWORD specular;
	float tu;
	float tv;
} D3DLVERTEX;

typedef struct _D3DVERTEX {
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float tu;
	float tv;
} D3DVERTEX;

/* Execute-buffer instruction header; bOpcode is a D3DOPCODE (1 point, 2 line,
 * 3 triangle, 4 matrixload, 5 matrixmultiply, 6 statetransform, 7 statelight,
 * 8 staterender, 9 processvertices, 10 textureload, 11 exit, 12 branchforward,
 * 13 span, 14 setstatus). wCount records of bSize bytes follow. */
typedef struct _D3DINSTRUCTION {
	BYTE bOpcode;
	BYTE bSize;
	WORD wCount;
} D3DINSTRUCTION;

typedef struct _D3DTRIANGLE {
	WORD v1;
	WORD v2;
	WORD v3;
	WORD wFlags;
} D3DTRIANGLE;

typedef struct _D3DSTATE {
	DWORD dwStateType;   /* D3DTRANSFORMSTATETYPE, D3DLIGHTSTATETYPE or D3DRENDERSTATETYPE */
	DWORD dwArg;         /* also dvArg (float) */
} D3DSTATE;

typedef struct _D3DPROCESSVERTICES {
	DWORD dwFlags;
	WORD wStart;
	WORD wDest;
	DWORD dwCount;
	DWORD dwReserved;
} D3DPROCESSVERTICES;

typedef struct _D3DSTATUS {
	DWORD dwFlags;
	DWORD dwStatus;
	D3DRECT drExtent;
} D3DSTATUS;

typedef struct _D3DEXECUTEBUFFERDESC {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwCaps;
	DWORD dwBufferSize;
	void *lpData;
} D3DEXECUTEBUFFERDESC;

typedef struct _D3DEXECUTEDATA {
	DWORD dwSize;
	DWORD dwVertexOffset;
	DWORD dwVertexCount;
	DWORD dwInstructionOffset;
	DWORD dwInstructionLength;
	DWORD dwHVertexOffset;
	D3DSTATUS dsStatus;
} D3DEXECUTEDATA;

typedef struct _D3DVIEWPORT {
	DWORD dwSize;
	DWORD dwX;
	DWORD dwY;
	DWORD dwWidth;
	DWORD dwHeight;
	float dvScaleX;
	float dvScaleY;
	float dvMaxX;
	float dvMaxY;
	float dvMinZ;
	float dvMaxZ;
} D3DVIEWPORT;

typedef struct _D3DMATERIAL {
	DWORD dwSize;
	D3DCOLORVALUE diffuse;
	D3DCOLORVALUE ambient;
	D3DCOLORVALUE specular;
	D3DCOLORVALUE emissive;
	float power;
	DWORD hTexture;
	DWORD dwRampSize;
} D3DMATERIAL;

typedef struct _D3DLIGHT {
	DWORD dwSize;
	DWORD dltType;       /* 1 point, 2 spot, 3 directional, 4 parallel point */
	D3DCOLORVALUE dcvColor;
	D3DVECTOR dvPosition;
	D3DVECTOR dvDirection;
	float dvRange;
	float dvFalloff;
	float dvAttenuation0;
	float dvAttenuation1;
	float dvAttenuation2;
	float dvTheta;
	float dvPhi;
} D3DLIGHT;

typedef struct _D3DTRANSFORMCAPS {
	DWORD dwSize;
	DWORD dwCaps;
} D3DTRANSFORMCAPS;

typedef struct _D3DLIGHTINGCAPS {
	DWORD dwSize;
	DWORD dwCaps;
	DWORD dwLightingModel;
	DWORD dwNumLights;
} D3DLIGHTINGCAPS;

typedef struct _D3DPRIMCAPS {
	DWORD dwSize;
	DWORD dwMiscCaps;
	DWORD dwRasterCaps;
	DWORD dwZCmpCaps;
	DWORD dwSrcBlendCaps;
	DWORD dwDestBlendCaps;
	DWORD dwAlphaCmpCaps;
	DWORD dwShadeCaps;
	DWORD dwTextureCaps;
	DWORD dwTextureFilterCaps;
	DWORD dwTextureBlendCaps;
	DWORD dwTextureAddressCaps;
	DWORD dwStippleWidth;
	DWORD dwStippleHeight;
} D3DPRIMCAPS;

/* DirectX 3 size: 0xac bytes. */
typedef struct _D3DDeviceDesc {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dcmColorModel;  /* 1 mono (ramp), 2 RGB */
	DWORD dwDevCaps;
	D3DTRANSFORMCAPS dtcTransformCaps;
	BOOL bClipping;
	D3DLIGHTINGCAPS dlcLightingCaps;
	D3DPRIMCAPS dpcLineCaps;
	D3DPRIMCAPS dpcTriCaps;
	DWORD dwDeviceRenderBitDepth;
	DWORD dwDeviceZBufferBitDepth;
	DWORD dwMaxBufferSize;
	DWORD dwMaxVertexCount;
} D3DDEVICEDESC;

typedef struct _D3DFINDDEVICESEARCH {
	DWORD dwSize;
	DWORD dwFlags;
	BOOL bHardware;
	DWORD dcmColorModel;
	GUID guid;
	DWORD dwCaps;
	D3DPRIMCAPS dpcPrimCaps;
} D3DFINDDEVICESEARCH;

typedef struct _D3DFINDDEVICERESULT {
	DWORD dwSize;
	GUID guid;
	D3DDEVICEDESC ddHwDesc;
	D3DDEVICEDESC ddSwDesc;
} D3DFINDDEVICERESULT;

typedef struct IDirect3D IDirect3D;
typedef struct IDirect3DDevice IDirect3DDevice;
typedef struct IDirect3DExecuteBuffer IDirect3DExecuteBuffer;
typedef struct IDirect3DViewport IDirect3DViewport;
typedef struct IDirect3DMaterial IDirect3DMaterial;
typedef struct IDirect3DTexture IDirect3DTexture;
typedef struct IDirect3DLight IDirect3DLight;

typedef struct IDirect3DVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirect3D *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirect3D *This);
	ULONG (__stdcall *Release)(IDirect3D *This);
	HRESULT (__stdcall *Initialize)(IDirect3D *This, GUID *lpREFIID);
	HRESULT (__stdcall *EnumDevices)(IDirect3D *This, void *lpEnumDevicesCallback, void *lpUserArg);
	HRESULT (__stdcall *CreateLight)(IDirect3D *This, IDirect3DLight **lplpDirect3DLight, void *pUnkOuter);
	HRESULT (__stdcall *CreateMaterial)(IDirect3D *This, IDirect3DMaterial **lplpDirect3DMaterial, void *pUnkOuter);
	HRESULT (__stdcall *CreateViewport)(IDirect3D *This, IDirect3DViewport **lplpD3DViewport, void *pUnkOuter);
	HRESULT (__stdcall *FindDevice)(IDirect3D *This, D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR);
} IDirect3DVtbl;
struct IDirect3D { IDirect3DVtbl *lpVtbl; };

typedef struct IDirect3DDeviceVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirect3DDevice *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirect3DDevice *This);
	ULONG (__stdcall *Release)(IDirect3DDevice *This);
	HRESULT (__stdcall *Initialize)(IDirect3DDevice *This, IDirect3D *lpd3d, GUID *lpGUID, D3DDEVICEDESC *lpd3ddvdesc);
	HRESULT (__stdcall *GetCaps)(IDirect3DDevice *This, D3DDEVICEDESC *lpD3DHWDevDesc, D3DDEVICEDESC *lpD3DHELDevDesc);
	HRESULT (__stdcall *SwapTextureHandles)(IDirect3DDevice *This, IDirect3DTexture *lpD3DTex1, IDirect3DTexture *lpD3DTex2);
	HRESULT (__stdcall *CreateExecuteBuffer)(IDirect3DDevice *This, D3DEXECUTEBUFFERDESC *lpDesc, IDirect3DExecuteBuffer **lplpDirect3DExecuteBuffer, void *pUnkOuter);
	HRESULT (__stdcall *GetStats)(IDirect3DDevice *This, void *lpD3DStats);
	HRESULT (__stdcall *Execute)(IDirect3DDevice *This, IDirect3DExecuteBuffer *lpDirect3DExecuteBuffer, IDirect3DViewport *lpDirect3DViewport, DWORD dwFlags);
	HRESULT (__stdcall *AddViewport)(IDirect3DDevice *This, IDirect3DViewport *lpDirect3DViewport);
	HRESULT (__stdcall *DeleteViewport)(IDirect3DDevice *This, IDirect3DViewport *lpDirect3DViewport);
	HRESULT (__stdcall *NextViewport)(IDirect3DDevice *This, IDirect3DViewport *lpDirect3DViewport, IDirect3DViewport **lplpDirect3DViewport, DWORD dwFlags);
	HRESULT (__stdcall *Pick)(IDirect3DDevice *This, IDirect3DExecuteBuffer *lpDirect3DExecuteBuffer, IDirect3DViewport *lpDirect3DViewport, DWORD dwFlags, D3DRECT *lpRect);
	HRESULT (__stdcall *GetPickRecords)(IDirect3DDevice *This, DWORD *lpCount, void *lpD3DPickRec);
	HRESULT (__stdcall *EnumTextureFormats)(IDirect3DDevice *This, void *lpd3dEnumTextureProc, void *lpArg);
	HRESULT (__stdcall *CreateMatrix)(IDirect3DDevice *This, DWORD *lpD3DMatHandle);
	HRESULT (__stdcall *SetMatrix)(IDirect3DDevice *This, DWORD d3dMatHandle, D3DMATRIX *lpD3DMatrix);
	HRESULT (__stdcall *GetMatrix)(IDirect3DDevice *This, DWORD D3DMatHandle, D3DMATRIX *lpD3DMatrix);
	HRESULT (__stdcall *DeleteMatrix)(IDirect3DDevice *This, DWORD d3dMatHandle);
	HRESULT (__stdcall *BeginScene)(IDirect3DDevice *This);
	HRESULT (__stdcall *EndScene)(IDirect3DDevice *This);
	HRESULT (__stdcall *GetDirect3D)(IDirect3DDevice *This, IDirect3D **lpD3D);
} IDirect3DDeviceVtbl;
struct IDirect3DDevice { IDirect3DDeviceVtbl *lpVtbl; };

typedef struct IDirect3DExecuteBufferVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirect3DExecuteBuffer *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirect3DExecuteBuffer *This);
	ULONG (__stdcall *Release)(IDirect3DExecuteBuffer *This);
	HRESULT (__stdcall *Initialize)(IDirect3DExecuteBuffer *This, IDirect3DDevice *lpDirect3DDevice, D3DEXECUTEBUFFERDESC *lpDesc);
	HRESULT (__stdcall *Lock)(IDirect3DExecuteBuffer *This, D3DEXECUTEBUFFERDESC *lpDesc);
	HRESULT (__stdcall *Unlock)(IDirect3DExecuteBuffer *This);
	HRESULT (__stdcall *SetExecuteData)(IDirect3DExecuteBuffer *This, D3DEXECUTEDATA *lpData);
	HRESULT (__stdcall *GetExecuteData)(IDirect3DExecuteBuffer *This, D3DEXECUTEDATA *lpData);
	HRESULT (__stdcall *Validate)(IDirect3DExecuteBuffer *This, DWORD *lpdwOffset, void *lpFunc, void *lpUserArg, DWORD dwReserved);
	HRESULT (__stdcall *Optimize)(IDirect3DExecuteBuffer *This, DWORD dwDummy);
} IDirect3DExecuteBufferVtbl;
struct IDirect3DExecuteBuffer { IDirect3DExecuteBufferVtbl *lpVtbl; };

typedef struct IDirect3DViewportVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirect3DViewport *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirect3DViewport *This);
	ULONG (__stdcall *Release)(IDirect3DViewport *This);
	HRESULT (__stdcall *Initialize)(IDirect3DViewport *This, IDirect3D *lpDirect3D);
	HRESULT (__stdcall *GetViewport)(IDirect3DViewport *This, D3DVIEWPORT *lpData);
	HRESULT (__stdcall *SetViewport)(IDirect3DViewport *This, D3DVIEWPORT *lpData);
	HRESULT (__stdcall *TransformVertices)(IDirect3DViewport *This, DWORD dwVertexCount, void *lpData, DWORD dwFlags, DWORD *lpOffScreen);
	HRESULT (__stdcall *LightElements)(IDirect3DViewport *This, DWORD dwElementCount, void *lpData);
	HRESULT (__stdcall *SetBackground)(IDirect3DViewport *This, DWORD hMat);
	HRESULT (__stdcall *GetBackground)(IDirect3DViewport *This, DWORD *lphMat, BOOL *lpValid);
	HRESULT (__stdcall *SetBackgroundDepth)(IDirect3DViewport *This, IDirectDrawSurface *lpDDSurface);
	HRESULT (__stdcall *GetBackgroundDepth)(IDirect3DViewport *This, IDirectDrawSurface **lplpDDSurface, BOOL *lpValid);
	HRESULT (__stdcall *Clear)(IDirect3DViewport *This, DWORD dwCount, D3DRECT *lpRects, DWORD dwFlags);
	HRESULT (__stdcall *AddLight)(IDirect3DViewport *This, IDirect3DLight *lpDirect3DLight);
	HRESULT (__stdcall *DeleteLight)(IDirect3DViewport *This, IDirect3DLight *lpDirect3DLight);
	HRESULT (__stdcall *NextLight)(IDirect3DViewport *This, IDirect3DLight *lpDirect3DLight, IDirect3DLight **lplpDirect3DLight, DWORD dwFlags);
} IDirect3DViewportVtbl;
struct IDirect3DViewport { IDirect3DViewportVtbl *lpVtbl; };

typedef struct IDirect3DMaterialVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirect3DMaterial *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirect3DMaterial *This);
	ULONG (__stdcall *Release)(IDirect3DMaterial *This);
	HRESULT (__stdcall *Initialize)(IDirect3DMaterial *This, IDirect3D *lpDirect3D);
	HRESULT (__stdcall *SetMaterial)(IDirect3DMaterial *This, D3DMATERIAL *lpMat);
	HRESULT (__stdcall *GetMaterial)(IDirect3DMaterial *This, D3DMATERIAL *lpMat);
	HRESULT (__stdcall *GetHandle)(IDirect3DMaterial *This, IDirect3DDevice *lpDirect3DDevice, DWORD *lpHandle);
	HRESULT (__stdcall *Reserve)(IDirect3DMaterial *This);
	HRESULT (__stdcall *Unreserve)(IDirect3DMaterial *This);
} IDirect3DMaterialVtbl;
struct IDirect3DMaterial { IDirect3DMaterialVtbl *lpVtbl; };

typedef struct IDirect3DTextureVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirect3DTexture *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirect3DTexture *This);
	ULONG (__stdcall *Release)(IDirect3DTexture *This);
	HRESULT (__stdcall *Initialize)(IDirect3DTexture *This, IDirect3DDevice *lpD3DDevice, IDirectDrawSurface *lpDDSurface);
	HRESULT (__stdcall *GetHandle)(IDirect3DTexture *This, IDirect3DDevice *lpDirect3DDevice, DWORD *lpHandle);
	HRESULT (__stdcall *PaletteChanged)(IDirect3DTexture *This, DWORD dwStart, DWORD dwCount);
	HRESULT (__stdcall *Load)(IDirect3DTexture *This, IDirect3DTexture *lpD3DTexture);
	HRESULT (__stdcall *Unload)(IDirect3DTexture *This);
} IDirect3DTextureVtbl;
struct IDirect3DTexture { IDirect3DTextureVtbl *lpVtbl; };

typedef struct IDirect3DLightVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirect3DLight *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirect3DLight *This);
	ULONG (__stdcall *Release)(IDirect3DLight *This);
	HRESULT (__stdcall *Initialize)(IDirect3DLight *This, IDirect3D *lpDirect3D);
	HRESULT (__stdcall *SetLight)(IDirect3DLight *This, D3DLIGHT *lpLight);
	HRESULT (__stdcall *GetLight)(IDirect3DLight *This, D3DLIGHT *lpLight);
} IDirect3DLightVtbl;
struct IDirect3DLight { IDirect3DLightVtbl *lpVtbl; };

/* -------------------------------------------------------------- DirectSound */

typedef struct _DSBUFFERDESC {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwBufferBytes;
	DWORD dwReserved;
	WAVEFORMATEX *lpwfxFormat;
} DSBUFFERDESC;

typedef struct _DSBCAPS {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwBufferBytes;
	DWORD dwUnlockTransferRate;
	DWORD dwPlayCpuOverhead;
} DSBCAPS;

typedef struct IDirectSound IDirectSound;
typedef struct IDirectSoundBuffer IDirectSoundBuffer;
typedef struct IDirectSound3DListener IDirectSound3DListener;
typedef struct IDirectSound3DBuffer IDirectSound3DBuffer;

typedef struct IDirectSoundVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectSound *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectSound *This);
	ULONG (__stdcall *Release)(IDirectSound *This);
	HRESULT (__stdcall *CreateSoundBuffer)(IDirectSound *This, DSBUFFERDESC *lpDSBufferDesc, IDirectSoundBuffer **lplpDirectSoundBuffer, void *pUnkOuter);
	HRESULT (__stdcall *GetCaps)(IDirectSound *This, void *lpDSCaps);
	HRESULT (__stdcall *DuplicateSoundBuffer)(IDirectSound *This, IDirectSoundBuffer *lpDsbOriginal, IDirectSoundBuffer **lplpDsbDuplicate);
	HRESULT (__stdcall *SetCooperativeLevel)(IDirectSound *This, HWND hwnd, DWORD dwLevel);
	HRESULT (__stdcall *Compact)(IDirectSound *This);
	HRESULT (__stdcall *GetSpeakerConfig)(IDirectSound *This, DWORD *lpdwSpeakerConfig);
	HRESULT (__stdcall *SetSpeakerConfig)(IDirectSound *This, DWORD dwSpeakerConfig);
	HRESULT (__stdcall *Initialize)(IDirectSound *This, GUID *lpGuid);
} IDirectSoundVtbl;
struct IDirectSound { IDirectSoundVtbl *lpVtbl; };

typedef struct IDirectSoundBufferVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectSoundBuffer *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectSoundBuffer *This);
	ULONG (__stdcall *Release)(IDirectSoundBuffer *This);
	HRESULT (__stdcall *GetCaps)(IDirectSoundBuffer *This, DSBCAPS *lpDSBufferCaps);
	HRESULT (__stdcall *GetCurrentPosition)(IDirectSoundBuffer *This, DWORD *lpdwCurrentPlayCursor, DWORD *lpdwCurrentWriteCursor);
	HRESULT (__stdcall *GetFormat)(IDirectSoundBuffer *This, WAVEFORMATEX *lpwfxFormat, DWORD dwSizeAllocated, DWORD *lpdwSizeWritten);
	HRESULT (__stdcall *GetVolume)(IDirectSoundBuffer *This, LONG *lplVolume);
	HRESULT (__stdcall *GetPan)(IDirectSoundBuffer *This, LONG *lplPan);
	HRESULT (__stdcall *GetFrequency)(IDirectSoundBuffer *This, DWORD *lpdwFrequency);
	HRESULT (__stdcall *GetStatus)(IDirectSoundBuffer *This, DWORD *lpdwStatus);
	HRESULT (__stdcall *Initialize)(IDirectSoundBuffer *This, IDirectSound *lpDirectSound, DSBUFFERDESC *lpDSBufferDesc);
	HRESULT (__stdcall *Lock)(IDirectSoundBuffer *This, DWORD dwOffset, DWORD dwBytes, void **ppvAudioPtr1, DWORD *pdwAudioBytes1, void **ppvAudioPtr2, DWORD *pdwAudioBytes2, DWORD dwFlags);
	HRESULT (__stdcall *Play)(IDirectSoundBuffer *This, DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags);
	HRESULT (__stdcall *SetCurrentPosition)(IDirectSoundBuffer *This, DWORD dwNewPosition);
	HRESULT (__stdcall *SetFormat)(IDirectSoundBuffer *This, WAVEFORMATEX *lpfxFormat);
	HRESULT (__stdcall *SetVolume)(IDirectSoundBuffer *This, LONG lVolume);
	HRESULT (__stdcall *SetPan)(IDirectSoundBuffer *This, LONG lPan);
	HRESULT (__stdcall *SetFrequency)(IDirectSoundBuffer *This, DWORD dwFrequency);
	HRESULT (__stdcall *Stop)(IDirectSoundBuffer *This);
	HRESULT (__stdcall *Unlock)(IDirectSoundBuffer *This, void *pvAudioPtr1, DWORD dwAudioBytes1, void *pvAudioPtr2, DWORD dwAudioBytes2);
	HRESULT (__stdcall *Restore)(IDirectSoundBuffer *This);
} IDirectSoundBufferVtbl;
struct IDirectSoundBuffer { IDirectSoundBufferVtbl *lpVtbl; };

typedef struct IDirectSound3DListenerVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectSound3DListener *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectSound3DListener *This);
	ULONG (__stdcall *Release)(IDirectSound3DListener *This);
	HRESULT (__stdcall *GetAllParameters)(IDirectSound3DListener *This, void *lpListener);
	HRESULT (__stdcall *GetDistanceFactor)(IDirectSound3DListener *This, float *lpflDistanceFactor);
	HRESULT (__stdcall *GetDopplerFactor)(IDirectSound3DListener *This, float *lpflDopplerFactor);
	HRESULT (__stdcall *GetOrientation)(IDirectSound3DListener *This, D3DVECTOR *lpvOrientFront, D3DVECTOR *lpvOrientTop);
	HRESULT (__stdcall *GetPosition)(IDirectSound3DListener *This, D3DVECTOR *lpvPosition);
	HRESULT (__stdcall *GetRolloffFactor)(IDirectSound3DListener *This, float *lpflRolloffFactor);
	HRESULT (__stdcall *GetVelocity)(IDirectSound3DListener *This, D3DVECTOR *lpvVelocity);
	HRESULT (__stdcall *SetAllParameters)(IDirectSound3DListener *This, void *lpcListener, DWORD dwApply);
	HRESULT (__stdcall *SetDistanceFactor)(IDirectSound3DListener *This, float flDistanceFactor, DWORD dwApply);
	HRESULT (__stdcall *SetDopplerFactor)(IDirectSound3DListener *This, float flDopplerFactor, DWORD dwApply);
	HRESULT (__stdcall *SetOrientation)(IDirectSound3DListener *This, float xFront, float yFront, float zFront, float xTop, float yTop, float zTop, DWORD dwApply);
	HRESULT (__stdcall *SetPosition)(IDirectSound3DListener *This, float x, float y, float z, DWORD dwApply);
	HRESULT (__stdcall *SetRolloffFactor)(IDirectSound3DListener *This, float flRolloffFactor, DWORD dwApply);
	HRESULT (__stdcall *SetVelocity)(IDirectSound3DListener *This, float x, float y, float z, DWORD dwApply);
	HRESULT (__stdcall *CommitDeferredSettings)(IDirectSound3DListener *This);
} IDirectSound3DListenerVtbl;
struct IDirectSound3DListener { IDirectSound3DListenerVtbl *lpVtbl; };

typedef struct IDirectSound3DBufferVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectSound3DBuffer *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectSound3DBuffer *This);
	ULONG (__stdcall *Release)(IDirectSound3DBuffer *This);
	HRESULT (__stdcall *GetAllParameters)(IDirectSound3DBuffer *This, void *lpDs3dBuffer);
	HRESULT (__stdcall *GetConeAngles)(IDirectSound3DBuffer *This, DWORD *lpdwInsideConeAngle, DWORD *lpdwOutsideConeAngle);
	HRESULT (__stdcall *GetConeOrientation)(IDirectSound3DBuffer *This, D3DVECTOR *lpvOrientation);
	HRESULT (__stdcall *GetConeOutsideVolume)(IDirectSound3DBuffer *This, LONG *lplConeOutsideVolume);
	HRESULT (__stdcall *GetMaxDistance)(IDirectSound3DBuffer *This, float *lpflMaxDistance);
	HRESULT (__stdcall *GetMinDistance)(IDirectSound3DBuffer *This, float *lpflMinDistance);
	HRESULT (__stdcall *GetMode)(IDirectSound3DBuffer *This, DWORD *lpdwMode);
	HRESULT (__stdcall *GetPosition)(IDirectSound3DBuffer *This, D3DVECTOR *lpvPosition);
	HRESULT (__stdcall *GetVelocity)(IDirectSound3DBuffer *This, D3DVECTOR *lpvVelocity);
	HRESULT (__stdcall *SetAllParameters)(IDirectSound3DBuffer *This, void *lpcDs3dBuffer, DWORD dwApply);
	HRESULT (__stdcall *SetConeAngles)(IDirectSound3DBuffer *This, DWORD dwInsideConeAngle, DWORD dwOutsideConeAngle, DWORD dwApply);
	HRESULT (__stdcall *SetConeOrientation)(IDirectSound3DBuffer *This, float x, float y, float z, DWORD dwApply);
	HRESULT (__stdcall *SetConeOutsideVolume)(IDirectSound3DBuffer *This, LONG lConeOutsideVolume, DWORD dwApply);
	HRESULT (__stdcall *SetMaxDistance)(IDirectSound3DBuffer *This, float flMaxDistance, DWORD dwApply);
	HRESULT (__stdcall *SetMinDistance)(IDirectSound3DBuffer *This, float flMinDistance, DWORD dwApply);
	HRESULT (__stdcall *SetMode)(IDirectSound3DBuffer *This, DWORD dwMode, DWORD dwApply);
	HRESULT (__stdcall *SetPosition)(IDirectSound3DBuffer *This, float x, float y, float z, DWORD dwApply);
	HRESULT (__stdcall *SetVelocity)(IDirectSound3DBuffer *This, float x, float y, float z, DWORD dwApply);
} IDirectSound3DBufferVtbl;
struct IDirectSound3DBuffer { IDirectSound3DBufferVtbl *lpVtbl; };

/* -------------------------------------------------------------- DirectInput */

typedef struct _DIOBJECTDATAFORMAT {
	GUID *pguid;
	DWORD dwOfs;
	DWORD dwType;
	DWORD dwFlags;
} DIOBJECTDATAFORMAT;

typedef struct _DIDATAFORMAT {
	DWORD dwSize;
	DWORD dwObjSize;
	DWORD dwFlags;
	DWORD dwDataSize;
	DWORD dwNumObjs;
	DIOBJECTDATAFORMAT *rgodf;
} DIDATAFORMAT;

typedef struct _DIPROPHEADER {
	DWORD dwSize;
	DWORD dwHeaderSize;
	DWORD dwObj;
	DWORD dwHow;
} DIPROPHEADER;

typedef struct _DIPROPRANGE {
	DIPROPHEADER diph;
	LONG lMin;
	LONG lMax;
} DIPROPRANGE;

typedef struct _DIPROPDWORD {
	DIPROPHEADER diph;
	DWORD dwData;
} DIPROPDWORD;

typedef struct _DIJOYSTATE {
	LONG lX;
	LONG lY;
	LONG lZ;
	LONG lRx;
	LONG lRy;
	LONG lRz;
	LONG rglSlider[2];
	DWORD rgdwPOV[4];
	BYTE rgbButtons[32];
} DIJOYSTATE;

typedef struct _DIDEVICEOBJECTDATA {
	DWORD dwOfs;
	DWORD dwData;
	DWORD dwTimeStamp;
	DWORD dwSequence;
} DIDEVICEOBJECTDATA;

typedef struct IDirectInputA IDirectInputA;
typedef struct IDirectInputDeviceA IDirectInputDeviceA;

typedef struct IDirectInputAVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectInputA *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectInputA *This);
	ULONG (__stdcall *Release)(IDirectInputA *This);
	HRESULT (__stdcall *CreateDevice)(IDirectInputA *This, GUID *rguid, IDirectInputDeviceA **lplpDirectInputDevice, void *pUnkOuter);
	HRESULT (__stdcall *EnumDevices)(IDirectInputA *This, DWORD dwDevType, void *lpCallback, void *pvRef, DWORD dwFlags);
	HRESULT (__stdcall *GetDeviceStatus)(IDirectInputA *This, GUID *rguidInstance);
	HRESULT (__stdcall *RunControlPanel)(IDirectInputA *This, HWND hwndOwner, DWORD dwFlags);
	HRESULT (__stdcall *Initialize)(IDirectInputA *This, HINSTANCE hinst, DWORD dwVersion);
} IDirectInputAVtbl;
struct IDirectInputA { IDirectInputAVtbl *lpVtbl; };

/* DirectX 5's IDirectInputDevice2 methods follow on the same vtable (Poll for joysticks). */
typedef struct IDirectInputDeviceAVtbl {
	HRESULT (__stdcall *QueryInterface)(IDirectInputDeviceA *This, GUID *riid, void **ppvObj);
	ULONG (__stdcall *AddRef)(IDirectInputDeviceA *This);
	ULONG (__stdcall *Release)(IDirectInputDeviceA *This);
	HRESULT (__stdcall *GetCapabilities)(IDirectInputDeviceA *This, void *lpDIDevCaps);
	HRESULT (__stdcall *EnumObjects)(IDirectInputDeviceA *This, void *lpCallback, void *pvRef, DWORD dwFlags);
	HRESULT (__stdcall *GetProperty)(IDirectInputDeviceA *This, GUID *rguidProp, DIPROPHEADER *pdiph);
	HRESULT (__stdcall *SetProperty)(IDirectInputDeviceA *This, GUID *rguidProp, DIPROPHEADER *pdiph);
	HRESULT (__stdcall *Acquire)(IDirectInputDeviceA *This);
	HRESULT (__stdcall *Unacquire)(IDirectInputDeviceA *This);
	HRESULT (__stdcall *GetDeviceState)(IDirectInputDeviceA *This, DWORD cbData, void *lpvData);
	HRESULT (__stdcall *GetDeviceData)(IDirectInputDeviceA *This, DWORD cbObjectData, DIDEVICEOBJECTDATA *rgdod, DWORD *pdwInOut, DWORD dwFlags);
	HRESULT (__stdcall *SetDataFormat)(IDirectInputDeviceA *This, DIDATAFORMAT *lpdf);
	HRESULT (__stdcall *SetEventNotification)(IDirectInputDeviceA *This, HANDLE hEvent);
	HRESULT (__stdcall *SetCooperativeLevel)(IDirectInputDeviceA *This, HWND hwnd, DWORD dwFlags);
	HRESULT (__stdcall *GetObjectInfo)(IDirectInputDeviceA *This, void *pdidoi, DWORD dwObj, DWORD dwHow);
	HRESULT (__stdcall *GetDeviceInfo)(IDirectInputDeviceA *This, void *pdidi);
	HRESULT (__stdcall *RunControlPanel)(IDirectInputDeviceA *This, HWND hwndOwner, DWORD dwFlags);
	HRESULT (__stdcall *Initialize)(IDirectInputDeviceA *This, HINSTANCE hinst, DWORD dwVersion, GUID *rguid);
	HRESULT (__stdcall *CreateEffect)(IDirectInputDeviceA *This, GUID *rguid, void *lpeff, void **ppdeff, void *punkOuter);
	HRESULT (__stdcall *EnumEffects)(IDirectInputDeviceA *This, void *lpCallback, void *pvRef, DWORD dwEffType);
	HRESULT (__stdcall *GetEffectInfo)(IDirectInputDeviceA *This, void *pdei, GUID *rguid);
	HRESULT (__stdcall *GetForceFeedbackState)(IDirectInputDeviceA *This, DWORD *pdwOut);
	HRESULT (__stdcall *SendForceFeedbackCommand)(IDirectInputDeviceA *This, DWORD dwFlags);
	HRESULT (__stdcall *EnumCreatedEffectObjects)(IDirectInputDeviceA *This, void *lpCallback, void *pvRef, DWORD fl);
	HRESULT (__stdcall *Escape)(IDirectInputDeviceA *This, void *pesc);
	HRESULT (__stdcall *Poll)(IDirectInputDeviceA *This);
	HRESULT (__stdcall *SendDeviceData)(IDirectInputDeviceA *This, DWORD cbObjectData, DIDEVICEOBJECTDATA *rgdod, DWORD *pdwInOut, DWORD fl);
} IDirectInputDeviceAVtbl;
struct IDirectInputDeviceA { IDirectInputDeviceAVtbl *lpVtbl; };
