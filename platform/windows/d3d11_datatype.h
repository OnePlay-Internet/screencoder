/**
 * @file d3d11_datatype.h
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2022-07-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __D3D11_DATATYPE_H__
#define __D3D11_DATATYPE_H__

#include <d3d11.h>
#include <d3d11_4.h>
#include <d3dcommon.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <dxgi1_2.h>

namespace dxgi
{
    typedef IDXGIFactory1                  *Factory            ; 
    typedef IDXGIDevice                    *Device              ; 
    typedef IDXGIDevice1                   *Device1             ; 
    typedef IDXGIAdapter1                  *Adapter            ; 
    typedef IDXGIOutput                    *Output              ; 
    typedef IDXGIOutput1                   *Output1             ; 
    typedef IDXGIOutputDuplication         *OutputDuplication   ; 
    typedef IDXGIResource                  *Resource            ; 
}

namespace d3d
{
    typedef ID3DBlob                       *Blob                ; 
}

namespace d3d11
{
    typedef ID3D11Texture2D                *Texture2D            ; 
    typedef ID3D11Device                   *Device               ; 
    typedef ID3D11DeviceContext            *DeviceContext        ; 
    typedef ID3D11Texture1D                *Texture1D            ; 
    typedef ID3D11Multithread              *Multithread          ; 
    typedef ID3D11VertexShader             *VertexShader         ; 
    typedef ID3D11PixelShader              *PixelShader          ; 
    typedef ID3D11BlendState               *BlendState           ; 
    typedef ID3D11InputLayout              *InputLayout          ; 
    typedef ID3D11RenderTargetView         *RenderTargetView     ; 
    typedef ID3D11ShaderResourceView       *ShaderResourceView   ; 
    typedef ID3D11Buffer                   *Buffer               ; 
    typedef ID3D11RasterizerState          *RasterizerState      ; 
    typedef ID3D11SamplerState             *SamplerState         ; 
    typedef ID3D11DepthStencilState        *DepthStencilState    ; 
    typedef ID3D11DepthStencilView         *DepthStencilView     ; 
}

#endif