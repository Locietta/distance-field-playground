#pragma once

#include <istream>
#include <ostream>
#include <type_traits>

#if (__cplusplus < 202002L && __cplusplus >= 201402L)

/// pre-C++20, using SFINAE

// trivially-copyable
template <typename T, typename std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
void serialize(std::ostream &os, const T &val) {
    os.write(reinterpret_cast<const char *>(&val), sizeof(T));
}

template <typename T, typename std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
void deserialize(std::istream &is, T &val) {
    is.read(reinterpret_cast<char *>(&val), sizeof(T));
}

// containers
template <typename T, typename U = std::decay_t<decltype(*std::declval<const T &>().begin())>,
          typename std::enable_if_t<!std::is_trivially_copyable<U>::value && !std::is_trivially_copyable<T>::value, int> = 0>
void serialize(std::ostream &os, const T &val) {
    unsigned int size = val.size();
    os.write(reinterpret_cast<const char *>(&size), sizeof(size));
    for (auto &v : val) {
        serialize(os, v);
    }
}

template <typename T, typename U = std::decay_t<decltype(*std::declval<T &>().begin())>,
          typename std::enable_if_t<!std::is_trivially_copyable<U>::value && !std::is_trivially_copyable<T>::value, int> = 0>
void deserialize(std::istream &is, T &val) {
    unsigned int size = 0;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    val.resize(size);
    for (auto &v : val) {
        deserialize(is, v);
    }
}

// container of trivially copyables, better performance
template <typename T, typename U = std::decay_t<decltype(*std::declval<const T &>().begin())>,
          typename std::enable_if_t<std::is_trivially_copyable<U>::value && !std::is_trivially_copyable<T>::value, int> = 0>
void serialize(std::ostream &os, const T &val) {
    unsigned int size = val.size();
    os.write(reinterpret_cast<const char *>(&size), sizeof(size));
    os.write(reinterpret_cast<const char *>(val.data()), size * sizeof(U));
}

template <typename T, typename U = std::decay_t<decltype(*std::declval<T &>().begin())>,
          typename std::enable_if_t<std::is_trivially_copyable<U>::value && !std::is_trivially_copyable<T>::value, int> = 0>
void deserialize(std::istream &is, T &val) {
    unsigned int size = 0;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    val.resize(size);
    is.read(reinterpret_cast<char *>(val.data()), size * sizeof(U));
}

#else

#include <concepts>

template <typename T>
concept trivially_copyable = std::is_trivially_copyable_v<T>;

// bug of clang-format 15 on concepts: https://github.com/llvm/llvm-project/issues/55898
// and behavior change on requires expression indentation
// waiting for clang-format 16 to add `RequiresExpressionIndentation` and fix llvm/llvm-project#55898

// clang-format off

// a container that can't be copied trivially (true for std::vector, false for std::array)
template <typename T>
concept container = (!trivially_copyable<T>) && requires {
    std::declval<const T &>().begin();
    std::declval<T &>().begin();
    std::declval<T>().begin();
};

// clang-format on

template <typename T>
concept trivial_element = trivially_copyable<std::decay_t<decltype(*std::declval<T>().begin())>>;

template <typename T>
concept trivial_container = container<T> && trivial_element<T>;

template <typename T>
concept nontrivial_container = container<T> && (!trivial_element<T>);

// trivially-copyable
template <trivially_copyable T>
void serialize(std::ostream &os, const T &val) {
    os.write(reinterpret_cast<const char *>(&val), sizeof(T));
}

template <trivially_copyable T>
void deserialize(std::istream &is, T &val) {
    is.read(reinterpret_cast<char *>(&val), sizeof(T));
}

// containers
template <nontrivial_container T>
void serialize(std::ostream &os, const T &val) {
    unsigned int size = val.size();
    os.write(reinterpret_cast<const char *>(&size), sizeof(size));
    for (auto &v : val) {
        serialize(os, v);
    }
}

template <nontrivial_container T>
void deserialize(std::istream &is, T &val) {
    unsigned int size = 0;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    val.resize(size);
    for (auto &v : val) {
        deserialize(is, v);
    }
}

// container of trivially copyables, better performance
template <trivial_container T>
void serialize(std::ostream &os, const T &val) {
    using E = std::decay_t<decltype(*std::declval<T>().begin())>;

    unsigned int size = val.size();
    os.write(reinterpret_cast<const char *>(&size), sizeof(size));
    os.write(reinterpret_cast<const char *>(val.data()), size * sizeof(E));
}

template <trivial_container T>
void deserialize(std::istream &is, T &val) {
    using E = std::decay_t<decltype(*std::declval<T>().begin())>;

    unsigned int size = 0;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    val.resize(size);
    is.read(reinterpret_cast<char *>(val.data()), size * sizeof(E));
}

#endif