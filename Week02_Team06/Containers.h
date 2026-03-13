#pragma once

template <typename T>
class TArray
{
public:
	T& operator[](uint32 index);
	const T& operator[](uint32 index) const;

	bool IsEmpty();
	void PushBack(T Element);
	void PopBack();
	size_t Size();
	void Clear();

private:
	std::vector<T> Container;
};

template <typename T, size_t N>
class TStaticArray
{
public:
	T& operator[](size_t index);
	const T& operator[](size_t index) const;

	void fill(size_t size);

private:
	std::array<T, N> Container;
};
#include "Containers.inl"
