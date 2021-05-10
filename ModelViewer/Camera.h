#pragma once
#include "DXSampleHelper.h"

using namespace DirectX;

class DXCamera
{
public:
	DXCamera(XMVECTOR pos, XMVECTOR lookDir, XMVECTOR up = { 0.0f, 1.0f, 0.0f });

    void Update(float elapsedSeconds);

    XMVECTOR GetPosition();

    XMMATRIX GetViewMatrix();
    XMMATRIX GetProjectionMatrix(float fov, float aspectRatio, float nearPlane = 1.0f, float farPlane = 1000.0f);

	void OnKeyDown(WPARAM key);
	void OnKeyUp(WPARAM key);

private:
    struct KeysPressed
    {
        bool w;
        bool a;
        bool s;
        bool d;

        bool left;
        bool right;
        bool up;
        bool down;
    };

    XMFLOAT3 position;
    XMFLOAT3 lookDirection; // The z-axis of the local coordinate system. (LH)
    XMFLOAT3 upDirection;   // The y-axis of the local coordinate system. (LH)

    float yaw;              // Rotate around the y-axis of the local coordinate system.
    float pitch;            // Rotate around the x-axis of the local coordinate system.

    float moveSpeed;
    float turnSpeed;

    KeysPressed keysPressed;
};