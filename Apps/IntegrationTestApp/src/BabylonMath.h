#pragma once

class Rect
{
public:
    float x;
    float y;
    float width;
    float height;

    Rect()
        : x(0), y(0), width(0), height(0) {}

    Rect(float x, float y, float width, float height)
        : x(x), y(y), width(width), height(height) {}

    float Left() const { return x; }
    float Top() const { return y; }
    float Right() const { return x + width; }
    float Bottom() const { return y + height; }

    bool Contains(float px, float py) const
    {
        return px >= x && px <= Right() && py >= y && py <= Bottom();
    }
};

class Matrix4
{
public:
    float m[16];

    Matrix4()
    {
        // Initialize as identity matrix
        for (int i = 0; i < 16; ++i)
            m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

    Matrix4(const float* values)
    {
        for (int i = 0; i < 16; ++i)
            m[i] = values[i];
    }

    static Matrix4 Identity()
    {
        return Matrix4();
    }

    Matrix4 operator*(const Matrix4& other) const
    {
        Matrix4 result;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                result.m[row * 4 + col] = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    result.m[row * 4 + col] += m[row * 4 + k] * other.m[k * 4 + col];
                }
            }
        }
        return result;
    }
};

class Vector3
{
public:
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};

class ICameraTransform
{
public:
    ICameraTransform()
        : m_nearClip(0.1f), m_farClip(1000.0f), m_position(0, 0, 0), m_targetPoint(0, 0, -1), m_upVector(0, 1, 0), m_fovInDegree(60.0f)
    {
    }

    float GetNearClip() const { return m_nearClip; }
    float GetFarClip() const { return m_farClip; }
    Vector3 GetPosition() const { return m_position; }
    Vector3 GetTargetPoint() const { return m_targetPoint; }
    Vector3 GetUpVector() const { return m_upVector; }
    float GetFovInDegree() const { return m_fovInDegree; }

    void SetNearClip(float nearClip) { m_nearClip = nearClip; }
    void SetFarClip(float farClip) { m_farClip = farClip; }
    void SetPosition(const Vector3& position) { m_position = position; }
    void SetTargetPoint(const Vector3& targetPoint) { m_targetPoint = targetPoint; }
    void SetUpVector(const Vector3& upVector) { m_upVector = upVector; }
    void SetFovInDegree(float fovInDegree) { m_fovInDegree = fovInDegree; }

private:
    float m_nearClip;
    float m_farClip;
    Vector3 m_position;
    Vector3 m_targetPoint;
    Vector3 m_upVector;
    float m_fovInDegree;
};