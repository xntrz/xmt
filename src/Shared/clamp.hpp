#pragma once


template<class T>
inline constexpr T _clamp(T v, T mi, T ma) {
    return (v > ma) ? (ma) : ((v < mi) ? (mi) : (v));
};


#define clamp(v, mi, ma) (_clamp(v, mi, ma))