#pragma once
template <typename T>
class RingBuffer {
private:
	size_t size;
	size_t wrPtr;
	size_t rdPtr;
	T *buffer;

public:
	RingBuffer(size_t size) : size(size), wrPtr(0), rdPtr(0) {
		buffer = new T[size + 1];
	}

	~RingBuffer() {
		delete[] buffer;
	}

	void push(T value) {
		buffer[wrPtr++] = value;
		if (wrPtr == size) wrPtr = 0;

		if (wrPtr == rdPtr) {
			rdPtr = (rdPtr + 1) % size; // Overwrite the oldest value
		}
	}

	T pop() {
		if (wrPtr == rdPtr) {
			return T(); // Buffer is empty
		}

		T value = buffer[rdPtr++];
		if (rdPtr == size) rdPtr = 0;
		return value;
	}

	bool isEmpty() const {
		return wrPtr == rdPtr;
	}

	bool isFull() const {
		return (wrPtr + 1) % size == rdPtr;
	}
	size_t itemCount() const {
		if (wrPtr >= rdPtr)
			return wrPtr - rdPtr;
		else
			return size - rdPtr + wrPtr;
	}
	size_t freeSpace() const {
		return size - itemCount() - 1;
	}
	uint8_t resize(size_t newSize) {
		T *newBuffer = new T[newSize + 1];
		if (newBuffer == nullptr) return 1;
		size_t i = 0;
		while (!isEmpty() && i < newSize) {
			newBuffer[i++] = pop();
		}
		delete[] buffer;
		buffer = newBuffer;
		size   = newSize;
		wrPtr  = i;
		rdPtr  = 0;
		return 0;
	}
	T &operator[](size_t index) {
		return buffer[(index + rdPtr) % size];
	}
	void clear() {
		wrPtr = 0;
		rdPtr = 0;
	}
	// erase all items before index (0 inclusive to index exclusive)
	void erase(size_t index) {
		if (index > itemCount()) index = itemCount();
		rdPtr = (rdPtr + index) % size;
	}
	void copyToArray(T *array, size_t start, size_t arraySize) {
		extern uint32_t crashInfo[256];
		crashInfo[17] = 100;
		if (start >= itemCount()) return;
		crashInfo[17] = 101;
		if (start + arraySize > itemCount()) arraySize = itemCount() - start;
		crashInfo[17] = 102;
		// use memcpy for speed
		if (wrPtr > rdPtr || rdPtr + start + arraySize < size) {
			memcpy(array, buffer + rdPtr + start, arraySize * sizeof(T));
		} else if (rdPtr + start >= size) {
			memcpy(array, buffer + rdPtr + start - size, arraySize * sizeof(T));
		} else {
			size_t itemsInFirstPart = size - rdPtr - start;
			memcpy(array, buffer + rdPtr + start, itemsInFirstPart * sizeof(T));
			crashInfo[17] = 107;
			memcpy(array + itemsInFirstPart, buffer, (arraySize - itemsInFirstPart) * sizeof(T));
			crashInfo[17] = 108;
		}
	}
};