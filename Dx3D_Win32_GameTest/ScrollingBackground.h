//--------------------------------------------------------------------------------------
// File: ScrollingBackground.h
//
// C++ version of the C# example on how to make a scrolling background with SpriteBatch
// http://msdn.microsoft.com/en-us/library/bb203868.aspx
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#include <exception>
#include <stdexcept>

#include "DirectXTK/SpriteBatch.h"

#include <wrl/client.h>

class ScrollingBackground
{
public:
    ScrollingBackground() noexcept :
        mScreenHeight(0),
        mTextureWidth(0),
        mTextureHeight(0),
        mScreenPos{},
        mTextureSize{},
        mOrigin{}
    {
    }

    ScrollingBackground(ScrollingBackground&&) = default;
    ScrollingBackground& operator= (ScrollingBackground&&) = default;

    ScrollingBackground(ScrollingBackground const&) = default;
    ScrollingBackground& operator= (ScrollingBackground const&) = default;

    void Load(ID3D11ShaderResourceView* texture)
    {
        mTexture = texture;

        if (texture)
        {
            Microsoft::WRL::ComPtr<ID3D11Resource> resource;
            texture->GetResource(resource.GetAddressOf());

            D3D11_RESOURCE_DIMENSION dim;
            resource->GetType(&dim);

            if (dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
                throw std::runtime_error("ScrollingBackground expects a Texture2D");

            Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
            resource.As(&tex2D);

            D3D11_TEXTURE2D_DESC desc;
            tex2D->GetDesc(&desc);

            mTextureWidth = int(desc.Width);
            mTextureHeight = int(desc.Height);

            mTextureSize.x = 0.f;
            mTextureSize.y = float(desc.Height);

            // 1. 초기화: Origin 설정 (특이점 분석)
            // 가로는 화면 중앙에 맞추기 위해 '중심(Center)'을 잡고,
            // 세로는 위에서부터 그려내리기 위해 '상단(Top)'을 잡습니다.
            mOrigin.x = float(desc.Width) / 2.f;
            mOrigin.y = 0.f;
        }
    }

    void SetWindow(int screenWidth, int screenHeight)
    {
        mScreenHeight = screenHeight;

        mScreenPos.x = float(screenWidth) / 2.f;
        mScreenPos.y = float(screenHeight) / 2.f;
    }

    void Update(float deltaY)
    {
        mScreenPos.y += deltaY;
        mScreenPos.y = fmodf(mScreenPos.y, float(mTextureHeight));
    }

    // 2. 렌더링: 무한 타일링 (Infinite Tiling Loop)
    void Draw(DirectX::SpriteBatch* batch) const
    {
        using namespace DirectX;

        XMVECTOR screenPos = XMLoadFloat2(&mScreenPos);
        XMVECTOR origin = XMLoadFloat2(&mOrigin);

        if (mScreenPos.y < float(mScreenHeight))
        {
            batch->Draw(mTexture.Get(), screenPos, nullptr,
                Colors::White, 0.f, origin, g_XMOne, SpriteEffects_None, 0.f);
        }

        // (0, TextureHeight) 값을 가진 벡터
        XMVECTOR textureSize = XMLoadFloat2(&mTextureSize);

        // [Step A] 현재 위치보다 '위쪽' 빈 공간 채우기
        // 스크롤이 내려오면서 윗부분이 비지 않도록, 텍스처 높이만큼 뺀 위치에 미리 하나 그립니다.
        screenPos -= textureSize;

        batch->Draw(mTexture.Get(), screenPos, nullptr,
            Colors::White, 0.f, origin, g_XMOne, SpriteEffects_None, 0.f);

        // [Step B] 화면 아래쪽 채우기 (Loop)
        // 현재 위치부터 시작해서, 화면 높이(mScreenHeight)를 넘어설 때까지
        // 계속해서 밑으로 이미지를 이어 붙여 그립니다.
        int currentPixelY = static_cast<int>(XMVectorGetY(screenPos)) + mTextureHeight;
        while (currentPixelY < mScreenHeight)
        {
            // 다음 위치로 이동
            screenPos += textureSize;
            currentPixelY += mTextureHeight;

            batch->Draw(mTexture.Get(), screenPos, nullptr,
                Colors::White, 0.f, origin, g_XMOne, SpriteEffects_None, 0.f);
        }
    }

private:
    int                                                 mScreenHeight;
    int                                                 mTextureWidth;
    int                                                 mTextureHeight;
    DirectX::XMFLOAT2                                   mScreenPos;
    DirectX::XMFLOAT2                                   mTextureSize;
    DirectX::XMFLOAT2                                   mOrigin;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    mTexture;
};