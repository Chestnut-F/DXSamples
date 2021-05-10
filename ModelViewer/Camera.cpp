#include "stdafx.h"
#include "Camera.h"

DXCamera::DXCamera(XMVECTOR pos, XMVECTOR lookDir, XMVECTOR up):
	moveSpeed(10.f),
	turnSpeed(XM_PI),
	keysPressed{}
{
	XMStoreFloat3(&position, pos);
	XMStoreFloat3(&lookDirection, XMVector3Normalize(lookDir));
	XMStoreFloat3(&upDirection, XMVector3Normalize(up));
	
    float cosYaw = XMVectorGetX(XMVector3Dot(XMVector3Normalize({ lookDirection.x, 0.0f, lookDirection.z }), { 0.0f, 0.0f, 1.0f }));
	yaw = XMScalarACos(cosYaw);
    float cosPitch = XMVectorGetX(XMVector3Dot(XMVector3Normalize({ 0.0f, upDirection.y, upDirection.z }), { 0.0f, 1.0f, 0.0f }));
	pitch = XMScalarACos(cosPitch);

    pitch = min(pitch, XM_PIDIV4);
    pitch = max(-XM_PIDIV4, pitch);
}

void DXCamera::Update(float elapsedSeconds)
{
    XMFLOAT3 move(0, 0, 0);

    if (keysPressed.a)
        move.x -= 1.0f;
    if (keysPressed.d)
        move.x += 1.0f;
    if (keysPressed.w)
        move.z += 1.0f;
    if (keysPressed.s)
        move.z -= 1.0f;

    if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
    {
        XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
        move.x = XMVectorGetX(vector);
        move.z = XMVectorGetZ(vector);
    }

    float moveInterval = moveSpeed * elapsedSeconds;
    float rotateInterval = turnSpeed * elapsedSeconds;

    if (keysPressed.left)
        yaw += rotateInterval;
    if (keysPressed.right)
        yaw -= rotateInterval;
    if (keysPressed.up)
        pitch += rotateInterval;
    if (keysPressed.down)
        pitch -= rotateInterval;

    pitch = min(pitch, XM_PIDIV4);
    pitch = max(-XM_PIDIV4, pitch);

    // Move the camera in model space.
    float x = move.x * cosf(yaw) - move.z * sinf(yaw);
    float y = move.z * sinf(pitch);
    float z = move.x * sinf(yaw) * cosf(pitch) + move.z * cosf(yaw) * cosf(pitch);
    position.x += x * moveInterval;
    position.y += y * moveInterval;
    position.z += z * moveInterval;

    // Determine the look direction.
    lookDirection.x = cosf(pitch) * -sinf(yaw);
    lookDirection.y = sinf(pitch);
    lookDirection.z = cosf(pitch) * cosf(yaw);
}

XMVECTOR DXCamera::GetPosition()
{
	return XMLoadFloat3(&position);
}

XMMATRIX DXCamera::GetViewMatrix()
{
	return XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&lookDirection), XMLoadFloat3(&upDirection));
}

XMMATRIX DXCamera::GetProjectionMatrix(float fov, float aspectRatio, float nearPlane, float farPlane)
{
	return XMMatrixPerspectiveFovLH(fov, aspectRatio, nearPlane, farPlane);
}

void DXCamera::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case 'W':
        keysPressed.w = true;
        break;
    case 'A':
        keysPressed.a = true;
        break;
    case 'S':
        keysPressed.s = true;
        break;
    case 'D':
        keysPressed.d = true;
        break;
    case VK_LEFT:
        keysPressed.left = true;
        break;
    case VK_RIGHT:
        keysPressed.right = true;
        break;
    case VK_UP:
        keysPressed.up = true;
        break;
    case VK_DOWN:
        keysPressed.down = true;
        break;
    }
}

void DXCamera::OnKeyUp(WPARAM key)
{
    switch (key)
    {
    case 'W':
        keysPressed.w = false;
        break;
    case 'A':
        keysPressed.a = false;
        break;
    case 'S':
        keysPressed.s = false;
        break;
    case 'D':
        keysPressed.d = false;
        break;
    case VK_LEFT:
        keysPressed.left = false;
        break;
    case VK_RIGHT:
        keysPressed.right = false;
        break;
    case VK_UP:
        keysPressed.up = false;
        break;
    case VK_DOWN:
        keysPressed.down = false;
        break;
    }
}
