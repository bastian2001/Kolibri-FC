#include <iostream>
#include <fstream>
#include <string>

using namespace std;

ifstream file("./36.rpbb.txt");
char readInt8()
{
	char reader;
	file.read(&reader, 1);
	return reader;
}
uint8_t readUInt8()
{
	uint8_t reader;
	file.read((char *)&reader, 1);
	return reader;
}
int16_t readInt16()
{
	int16_t reader;
	file.read((char *)&reader, 2);
	return reader;
}
uint16_t readUInt16()
{
	uint16_t reader;
	file.read((char *)&reader, 2);
	return reader;
}
int32_t readInt32()
{
	int32_t reader;
	file.read((char *)&reader, 4);
	return reader;
}
uint32_t readUInt32()
{
	uint32_t reader;
	file.read((char *)&reader, 4);
	return reader;
}
uint64_t readUInt64()
{
	uint64_t reader;
	file.read((char *)&reader, 8);
	return reader;
}

typedef struct flag
{
	string name;
	uint8_t length;
} Flag;

uint16_t *rollELRSRaw, *pitchELRSRaw, *throttleELRSRaw, *yawELRSRaw;
int16_t *rollSetpoint, *pitchSetpoint, *throttleSetpoint, *yawSetpoint;
int16_t *rollGyroRaw, *pitchGyroRaw, *yawGyroRaw;
int16_t *rollPIDP, *rollPIDI, *rollPIDD, *rollPIDFF, *rollPIDS;
int16_t *pitchPIDP, *pitchPIDI, *pitchPIDD, *pitchPIDFF, *pitchPIDS;
int16_t *yawPIDP, *yawPIDI, *yawPIDD, *yawPIDFF, *yawPIDS;
uint64_t *motorOutputs;
uint16_t *altitude;

Flag logFlags[] = {
	{"LOG_ROLL_ELRS_RAW", 2},
	{"LOG_PITCH_ELRS_RAW", 2},
	{"LOG_THROTTLE_ELRS_RAW", 2},
	{"LOG_YAW_ELRS_RAW", 2},
	{"LOG_ROLL_SETPOINT", 2},
	{"LOG_PITCH_SETPOINT", 2},
	{"LOG_THROTTLE_SETPOINT", 2},
	{"LOG_YAW_SETPOINT", 2},
	{"LOG_ROLL_GYRO_RAW", 2},
	{"LOG_PITCH_GYRO_RAW", 2},
	{"LOG_YAW_GYRO_RAW", 2},
	{"LOG_ROLL_PID_P", 2},
	{"LOG_ROLL_PID_I", 2},
	{"LOG_ROLL_PID_D", 2},
	{"LOG_ROLL_PID_FF", 2},
	{"LOG_ROLL_PID_S", 2},
	{"LOG_PITCH_PID_P", 2},
	{"LOG_PITCH_PID_I", 2},
	{"LOG_PITCH_PID_D", 2},
	{"LOG_PITCH_PID_FF", 2},
	{"LOG_PITCH_PID_S", 2},
	{"LOG_YAW_PID_P", 2},
	{"LOG_YAW_PID_I", 2},
	{"LOG_YAW_PID_D", 2},
	{"LOG_YAW_PID_FF", 2},
	{"LOG_YAW_PID_S", 2},
	{"LOG_MOTOR_OUTPUTS", 7},
	{"LOG_ALTITUDE", 2},
};

int main()
{
	file.ignore(23);

	uint32_t magic = readUInt32();
	bool b = magic == 0x99A12720;
	cout << (b ? "Magic bytes correct\n" : "Magic bytes incorrect, terminating\n");
	if (!b)
	{
		printf("0x%08X", magic);
		return 1;
	}
	cout << "File version: " << (int)readUInt8() << '.' << (int)readUInt8() << '.' << (int)readUInt8() << endl;
	cout << "Start time: " << readUInt32() << endl;
	cout << "PID loop frequency: " << (3200 / (1 << readUInt8())) << " Hz" << endl;
	cout << "Loop divider: " << (int)readUInt8() << endl;
	uint8_t gyrAccelRange = readUInt8();
	cout << "Gyro/Accel range: " << (gyrAccelRange & 0b111) << ' ' << (gyrAccelRange >> 3) << endl;
	double rateFactors[5][3];
	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			rateFactors[i][j] = (double)readInt32() / 65536.;
		}
	}
	cout << "Rate factors:\tx^1\tx^2\tx^3\tx^4\tx^5" << endl;
	cout << "\tRoll:\t";
	for (int i = 0; i < 5; i++)
		cout << rateFactors[i][0] << '\t';
	cout << "\n\tPitch:\t";
	for (int i = 0; i < 5; i++)
		cout << rateFactors[i][1] << '\t';
	cout << "\n\tYaw:\t";
	for (int i = 0; i < 5; i++)
		cout << rateFactors[i][2] << '\t';
	cout << endl;
	double pidConstants[3][7];
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 7; j++)
		{
			pidConstants[i][j] = (double)readInt32() / 65536.;
		}
	}
	cout << "PID constants:\tkP\tkI\t\tkD\tkFF\tkS\t\tiFalloff\treserved" << endl;
	cout << "\tRoll:\t";
	for (int i = 0; i < 7; i++)
	{
		cout << pidConstants[0][i] << '\t';
	}
	cout << "\n\tPitch:\t";
	for (int i = 0; i < 7; i++)
	{
		cout << pidConstants[1][i] << '\t';
	}
	cout << "\n\tYaw:\t";
	for (int i = 0; i < 7; i++)
	{
		cout << pidConstants[2][i] << '\t';
	}
	cout << endl;
	cout << "Enabled flags:" << endl;
	uint64_t bbFlags = readUInt64();
	uint32_t dataFrameLength = 0;
	for (int i = 0; i < (sizeof(logFlags)) / (sizeof(Flag)); i++)
	{
		if (bbFlags & (1 << i))
		{
			cout << "  - " << logFlags[i].name << endl;
			dataFrameLength += logFlags[i].length;
		}
	}
	cout << "  => " << dataFrameLength << " Bytes per data frame" << endl;
	int pos = file.tellg();
	file.seekg(0, ios::end);
	int end = file.tellg();
	end -= 30;
	end -= pos;
	int frameCount = end / dataFrameLength;
	cout << "  => " << frameCount << " data frames" << endl;
	if (end % dataFrameLength != 0)
		cout << "Warning: " << (end % dataFrameLength) << " Bytes of trailing data" << endl;
	if (bbFlags & (1 << 0))
		rollELRSRaw = new uint16_t[frameCount];
	if (bbFlags & (1 << 1))
		pitchELRSRaw = new uint16_t[frameCount];
	if (bbFlags & (1 << 2))
		throttleELRSRaw = new uint16_t[frameCount];
	if (bbFlags & (1 << 3))
		yawELRSRaw = new uint16_t[frameCount];
	if (bbFlags & (1 << 4))
		rollSetpoint = new int16_t[frameCount];
	if (bbFlags & (1 << 5))
		pitchSetpoint = new int16_t[frameCount];
	if (bbFlags & (1 << 6))
		throttleSetpoint = new int16_t[frameCount];
	if (bbFlags & (1 << 7))
		yawSetpoint = new int16_t[frameCount];
	if (bbFlags & (1 << 8))
		rollGyroRaw = new int16_t[frameCount];
	if (bbFlags & (1 << 9))
		pitchGyroRaw = new int16_t[frameCount];
	if (bbFlags & (1 << 10))
		yawGyroRaw = new int16_t[frameCount];
	if (bbFlags & (1 << 11))
		rollPIDP = new int16_t[frameCount];
	if (bbFlags & (1 << 12))
		rollPIDI = new int16_t[frameCount];
	if (bbFlags & (1 << 13))
		rollPIDD = new int16_t[frameCount];
	if (bbFlags & (1 << 14))
		rollPIDFF = new int16_t[frameCount];
	if (bbFlags & (1 << 15))
		rollPIDS = new int16_t[frameCount];
	if (bbFlags & (1 << 16))
		pitchPIDP = new int16_t[frameCount];
	if (bbFlags & (1 << 17))
		pitchPIDI = new int16_t[frameCount];
	if (bbFlags & (1 << 18))
		pitchPIDD = new int16_t[frameCount];
	if (bbFlags & (1 << 19))
		pitchPIDFF = new int16_t[frameCount];
	if (bbFlags & (1 << 20))
		pitchPIDS = new int16_t[frameCount];
	if (bbFlags & (1 << 21))
		yawPIDP = new int16_t[frameCount];
	if (bbFlags & (1 << 22))
		yawPIDI = new int16_t[frameCount];
	if (bbFlags & (1 << 23))
		yawPIDD = new int16_t[frameCount];
	if (bbFlags & (1 << 24))
		yawPIDFF = new int16_t[frameCount];
	if (bbFlags & (1 << 25))
		yawPIDS = new int16_t[frameCount];
	if (bbFlags & (1 << 26))
		motorOutputs = new uint64_t[frameCount];
	if (bbFlags & (1 << 27))
		altitude = new uint16_t[frameCount];
	file.seekg(pos);

	for (int i = 0; i < frameCount; i++)
	{
		if (bbFlags & (1 << 0))
			rollELRSRaw[i] = readUInt16();
		if (bbFlags & (1 << 1))
			pitchELRSRaw[i] = readUInt16();
		if (bbFlags & (1 << 2))
			throttleELRSRaw[i] = readUInt16();
		if (bbFlags & (1 << 3))
			yawELRSRaw[i] = readUInt16();
		if (bbFlags & (1 << 4))
			rollSetpoint[i] = readInt16();
		if (bbFlags & (1 << 5))
			pitchSetpoint[i] = readInt16();
		if (bbFlags & (1 << 6))
			throttleSetpoint[i] = readInt16();
		if (bbFlags & (1 << 7))
			yawSetpoint[i] = readInt16();
		if (bbFlags & (1 << 8))
			rollGyroRaw[i] = readInt16();
		if (bbFlags & (1 << 9))
			pitchGyroRaw[i] = readInt16();
		if (bbFlags & (1 << 10))
			yawGyroRaw[i] = readInt16();
		if (bbFlags & (1 << 11))
			rollPIDP[i] = readInt16();
		if (bbFlags & (1 << 12))
			rollPIDI[i] = readInt16();
		if (bbFlags & (1 << 13))
			rollPIDD[i] = readInt16();
		if (bbFlags & (1 << 14))
			rollPIDFF[i] = readInt16();
		if (bbFlags & (1 << 15))
			rollPIDS[i] = readInt16();
		if (bbFlags & (1 << 16))
			pitchPIDP[i] = readInt16();
		if (bbFlags & (1 << 17))
			pitchPIDI[i] = readInt16();
		if (bbFlags & (1 << 18))
			pitchPIDD[i] = readInt16();
		if (bbFlags & (1 << 19))
			pitchPIDFF[i] = readInt16();
		if (bbFlags & (1 << 20))
			pitchPIDS[i] = readInt16();
		if (bbFlags & (1 << 21))
			yawPIDP[i] = readInt16();
		if (bbFlags & (1 << 22))
			yawPIDI[i] = readInt16();
		if (bbFlags & (1 << 23))
			yawPIDD[i] = readInt16();
		if (bbFlags & (1 << 24))
			yawPIDFF[i] = readInt16();
		if (bbFlags & (1 << 25))
			yawPIDS[i] = readInt16();
		if (bbFlags & (1 << 26))
		{
			motorOutputs[i] = readUInt8(); // for now only
			motorOutputs[i] <<= 8;		   // for now only
			motorOutputs[i] |= readUInt8();
			motorOutputs[i] <<= 8;
			motorOutputs[i] |= readUInt8();
			motorOutputs[i] <<= 8;
			motorOutputs[i] |= readUInt8();
			motorOutputs[i] <<= 8;
			motorOutputs[i] |= readUInt8();
			motorOutputs[i] <<= 8;
			motorOutputs[i] |= readUInt8();
			motorOutputs[i] <<= 8;
			motorOutputs[i] |= readUInt8();
		}
		if (bbFlags & (1 << 27))
			altitude[i] = readUInt16();
	}

	if (bbFlags & (1 << 26))
	{
		cout << "Motor outputs:" << endl;
		for (int i = 0; i < frameCount; i++)
		{
			cout << ((motorOutputs[i] >> 36) & 0xFFF) << ' ' << ((motorOutputs[i] >> 24) & 0xFFF) << ' ' << ((motorOutputs[i] >> 12) & 0xFFF) << ' ' << (motorOutputs[i] & 0xFFF) << endl;
		}
	}

	return 0;
}