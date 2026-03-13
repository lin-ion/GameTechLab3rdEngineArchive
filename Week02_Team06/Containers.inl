#include "Containers.h"

//vector
template<typename T>
inline T& TArray<T>::operator[](uint32 index)
{
	return Container[index];
}

template<typename T>
inline const T& TArray<T>::operator[](uint32 index) const
{
	return Container[index];
}

template<typename T>
inline bool TArray<T>::IsEmpty()
{
	return Container.empty();
}

template<typename T>
inline void TArray<T>::PushBack(T Element)
{
	Container.push_back(Element);
}

template<typename T>
inline void TArray<T>::PopBack()
{
	Container.pop_back();
}

template<typename T>
inline size_t TArray<T>::Size()
{
	return Container.size();;
}

template<typename T>
inline void TArray<T>::Clear()
{
	Container.clear();
}

//array
template<typename T, size_t N>
inline T& TStaticArray<T, N>::operator[](size_t index)
{
	return Container[index];
}

template<typename T, size_t N>
inline const T& TStaticArray<T, N>::operator[](size_t index) const
{
	return Container[index];
}

template<typename T, size_t N>
inline void TStaticArray<T, N>::fill(size_t size)
{
	Container.fill(size);
}
