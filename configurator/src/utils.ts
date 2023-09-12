export const leBytesToInt = (bytes: number[], signed = false) => {
	let value = 0;
	for (let i = 0; i < bytes.length; i++) {
		value += bytes[i] * Math.pow(256, i);
	}
	if (signed && bytes[bytes.length - 1] & 0b10000000) {
		value -= Math.pow(256, bytes.length);
	}
	return value;
};
