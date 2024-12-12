#pragma once
template <typename T>
class RingBuffer {
private:
	size_t size;
	size_t pSize; // physical size
	size_t wrPtr;
	size_t rdPtr;
	T *buffer;

public:
	RingBuffer(size_t size) : size(size), wrPtr(0), rdPtr(0), pSize(size + 1) {
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
	void push(T value) {
		buffer[wrPtr++] = value;
		if (wrPtr == pSize) wrPtr = 0;

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
	T pop() {
		if (wrPtr == rdPtr) {
			return T(); // Buffer is empty
		}

		T value = buffer[rdPtr++];
		if (rdPtr == pSize) rdPtr = 0;
		return value;
	}

	/// @brief check if the buffer is empty
	/// @return true if the buffer is empty
	bool isEmpty() const {
		return wrPtr == rdPtr;
	}

	/// @brief check if the buffer is full
	/// @return true if the buffer is full
	bool isFull() const {
		return (wrPtr + 1) % pSize == rdPtr;
	}
	/// @brief get the number of items in the buffer
	/// @return size_t the number of items in the buffer
	size_t itemCount() const {
		if (wrPtr >= rdPtr)
			return wrPtr - rdPtr;
		else
			return pSize - rdPtr + wrPtr;
	}
	/// @brief get the number of free spaces in the buffer
	size_t freeSpace() const {
		return size - itemCount();
	}
	/**
	 * @brief resize the buffer, keeping the oldest items
	 *
	 * @param newSize
	 * @return u8
	 */
	u8 resize(size_t newSize) {
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
	/// @brief get the item at index index, does not pop
	T &operator[](size_t index) {
		return buffer[(index + rdPtr) % pSize];
	}
	/// @brief clear the buffer
	void clear() {
		wrPtr = 0;
		rdPtr = 0;
	}
	/// @brief erase all items before index (0 inclusive to index exclusive)
	void erase(size_t index) {
		if (index > itemCount()) index = itemCount();
		rdPtr = (rdPtr + index) % pSize;
	}
	/**
	 * @brief copy the buffer to an array
	 *
	 * @param array pointer where to copy
	 * @param start skip the first start items
	 * @param arraySize number of items to copy
	 */
	void copyToArray(T *array, size_t start, size_t arraySize) {
		if (start >= itemCount()) return;
		if (start + arraySize > itemCount()) arraySize = itemCount() - start;
		// use memcpy for speed
		if (wrPtr > rdPtr || rdPtr + start + arraySize < pSize) {
			memcpy(array, buffer + rdPtr + start, arraySize * sizeof(T));
		} else if (rdPtr + start >= pSize) {
			memcpy(array, buffer + rdPtr + start - size, arraySize * sizeof(T));
		} else {
			size_t itemsInFirstPart = pSize - rdPtr - start;
			memcpy(array, buffer + rdPtr + start, itemsInFirstPart * sizeof(T));
			memcpy(array + itemsInFirstPart, buffer, (arraySize - itemsInFirstPart) * sizeof(T));
		}
	}
};