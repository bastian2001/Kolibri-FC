/**
 * @file ringbuffer.h
 * @brief Generic ring buffer template implementation, mostly used for buffering IO data.
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
template <typename T>
class RingBuffer {
private:
	size_t size; // logical size (in items)
	size_t pSize; // physical size (in items)
	size_t wrPtr = 0;
	size_t rdPtr = 0;
	T *buffer;

public:
	RingBuffer(size_t size) : size(size), pSize(size + 1) {
		buffer = new T[size + 1];
	}

	~RingBuffer() {
		delete[] buffer;
	}

	/**
	 * @brief push a new value to the buffer, overwriting the oldest value if the buffer is full
	 *
	 * @param value The new value to be pushed
	 */
	inline void push(T value) {
		buffer[wrPtr++] = value;
		if (wrPtr >= pSize) wrPtr = 0;

		if (wrPtr == rdPtr) {
			rdPtr++; // Overwrite the oldest value
			if (rdPtr == pSize) rdPtr = 0;
		}
	}

	/**
	 * @brief pop the oldest value from the buffer (FIFO)
	 *
	 * @return T The oldest value in the buffer
	 */
	inline T pop() {
		if (wrPtr == rdPtr) {
			return T(); // Buffer is empty
		}

		size_t oldRdPtr = rdPtr;
		if (++rdPtr >= pSize) rdPtr = 0;
		return buffer[oldRdPtr];
	}

	/// @brief check if the buffer is empty
	/// @return true if the buffer is empty
	inline bool isEmpty() const {
		return wrPtr == rdPtr;
	}

	/// @brief check if the buffer is full
	/// @return true if the buffer is full
	inline bool isFull() const {
		return (wrPtr + 1) % pSize == rdPtr;
	}
	/// @brief get the number of items in the buffer
	/// @return size_t the number of items in the buffer
	inline size_t itemCount() const {
		if (wrPtr >= rdPtr)
			return wrPtr - rdPtr;
		else
			return pSize - rdPtr + wrPtr;
	}
	/// @brief get the number of free spaces in the buffer
	inline size_t freeSpace() const {
		return size - itemCount();
	}
	/**
	 * @brief resize the buffer, keeping the oldest items
	 *
	 * @param newSize
	 * @return u8
	 */
	inline u8 resize(size_t newSize) {
		T *newBuffer = new T[newSize + 1];
		if (newBuffer == nullptr) return 1;
		size_t i = 0;
		while (!isEmpty() && i < newSize) {
			newBuffer[i++] = pop();
		}
		delete[] buffer;
		buffer = newBuffer;
		size = newSize;
		pSize = newSize + 1;
		wrPtr = i;
		rdPtr = 0;
		return 0;
	}
	/// @brief get (peek at) the item at index index, does not pop
	inline T &operator[](size_t index) {
		return buffer[(index + rdPtr) % pSize];
	}
	/// @brief clear the buffer
	inline void clear() {
		wrPtr = 0;
		rdPtr = 0;
	}
	/// @brief erase all items before index (0 inclusive to index exclusive)
	inline void erase(size_t index) {
		if (index > itemCount()) index = itemCount();
		rdPtr = (rdPtr + index) % pSize;
	}

	/**
	 * @brief copy the buffer to an array
	 *
	 * leaves the buffer unchanged, does not pop any items
	 *
	 * if too many items are requested, nothing will be copied
	 *
	 * @param array pointer where to copy
	 * @param start skip the first start items
	 * @param arraySize number of items to copy
	 */
	inline void copyToArray(T *array, size_t start, size_t arraySize) const {
		if (start + arraySize > itemCount()) return;
		// use memcpy for speed
		size_t actualStart = rdPtr + start;
		if (actualStart >= pSize) actualStart -= pSize;
		if (actualStart + arraySize < pSize) { // no wrap
			memcpy(array, buffer + actualStart, arraySize * sizeof(T));
		} else { // split up
			size_t itemsInFirstPart = pSize - actualStart;
			memcpy(array, buffer + actualStart, itemsInFirstPart * sizeof(T));
			memcpy(array + itemsInFirstPart, buffer, (arraySize - itemsInFirstPart) * sizeof(T));
		}
	}

	/**
	 * @brief fill the buffer from an array
	 *
	 * does not overwrite any data if the array is too big
	 *
	 * @param array pointer to the array to copy from
	 * @param arraySize number of items to copy
	 */
	inline void fillFromArray(const T *array, size_t arraySize) {
		if (arraySize > freeSpace()) return;
		if (wrPtr + arraySize < pSize) { // no wrap
			memcpy(buffer + wrPtr, array, arraySize * sizeof(T));
			wrPtr += arraySize;
		} else { // split up
			size_t itemsInFirstPart = pSize - wrPtr;
			memcpy(buffer + wrPtr, array, itemsInFirstPart * sizeof(T));
			memcpy(buffer, array + itemsInFirstPart, (arraySize - itemsInFirstPart) * sizeof(T));
			wrPtr = arraySize - itemsInFirstPart;
		}
	}
};
