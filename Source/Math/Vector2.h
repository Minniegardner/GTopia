#pragma once

#include "../Precompiled.h"

template<typename T>
class Vector2 {
public:
    T x, y;

    Vector2() : x(0), y(0) {}
    Vector2(T x_, T y_) : x(x_), y(y_) {}

    bool operator==(const Vector2& rhs) const
    {
        return x == rhs.x && y == rhs.y;
    }

    bool operator!=(const Vector2<T>& rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const Vector2<T>& rhs) const
    {
        if(x < rhs.x) return true;
        else if(y < rhs.y) return true;
        return false;
    }

    bool operator>(const Vector2<T>& rhs) const
    {
        if(x > rhs.x) return true;
        else if(y > rhs.y) return true;
        return false;
    }

    Vector2<T> operator*(int32 rhs) const
    {
        return Vector2<T>(x * rhs, y * rhs);
    }

    Vector2<T>& operator+=(const Vector2<T>& rhs)
    {
        x += rhs.x; y += rhs.y;
        return *this;
    }

    Vector2<T>& operator-=(const Vector2<T>& rhs)
    {
        x -= rhs.x; y -= rhs.y;
        return *this;
    }

    Vector2<T>& operator=(const Vector2<T>& rhs) 
    {
        x = rhs.x; y = rhs.y;
        return *this;
    }

    Vector2<T> operator+(const Vector2<T>& rhs) const
    {
        return Vector2<T>(x + rhs.x, y + rhs.y);
    }

    Vector2<T> operator-(const Vector2<T>& rhs) const
    {
        return Vector2<T>(x - rhs.x, y - rhs.y);
    }

public:
    static const Vector2<T> New;
};

typedef Vector2<int32> Vector2Int;
typedef Vector2<float> Vector2Float;