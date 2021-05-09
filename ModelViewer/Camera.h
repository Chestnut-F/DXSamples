#pragma once
#include "DXSampleHelper.h"

class Camera
{
public:
	Camera();

    DirectX::XMVECTOR GetPosition()const;
    DirectX::XMFLOAT3 GetPosition3f()const;

    DirectX::XMMATRIX GetViewMatrix();
    DirectX::XMMATRIX GetProjectionMatrix(float fov, float aspectRatio, float nearPlane = 1.0f, float farPlane = 1000.0f);

	void OnKeyDown(WPARAM key);
	void OnKeyUp(WPARAM key);

private:
    struct KeysPressed
    {
        bool left;
        bool right;
        bool up;
        bool down;
    };
};