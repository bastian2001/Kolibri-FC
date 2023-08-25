#include <iostream>
#include <fstream>
#include <string>

using namespace std;

ifstream file;

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

string joinArray(string arr[], int len, string sep = ", ")
{
	string ret = "";
	for (int i = 0; i < len; i++)
	{
		ret += arr[i];
		if (i < len - 1)
			ret += sep;
	}
	return ret;
}

bool hasEnding(std::string const &fullString, std::string const &ending)
{
	if (fullString.length() >= ending.length())
	{
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	}
	else
	{
		return false;
	}
}
int handleFile(string fname, bool print = true);

int main()
{
	// list all .rpbb files in current directory
	system("dir /b *.rpbb,*.txt > list.tmp");
	ifstream list("list.tmp");
	string files[100];
	int fileCount = 0;
	while (!list.eof())
	{
		getline(list, files[fileCount++]);
		if (fileCount >= 100)
			break;
	}
	fileCount--;
	list.close();
	system("del list.tmp");
	// ask user which file to open using 2 digit number, or to enter a path
	cout << "\nSelect file to open\nLeave empty to do all\nEnter relative path to do another file"
		 << endl;
	for (int i = 0; i < fileCount; i++)
	{
		printf("[%02d] %s\n", i, files[i].c_str());
	}
	cout << ">>  ";
	string fname = "";
	getline(cin, fname);
	if (fname.length() == 2)
	{
		int i = (fname[0] - '0') * 10 + (fname[1] - '0');
		if (i >= 0 && i < fileCount)
		{
			// check if entry was actually that number
			char c[3];
			snprintf(c, 3, "%02d", i);
			if (fname.substr(0, 2) == c)
			{
				fname = files[i];
			}
		}
	}
	if (fname == "")
	{
		for (int i = 0; i < fileCount; i++)
			handleFile(files[i], false);
		return 0;
	}
	// open file
	int ret = handleFile(fname);
	if (ret != 0)
		return ret;
	return 0;
}

int handleFile(const string fname, bool print)
{
	file.open(fname);
	if (!file.is_open())
	{
		cout << "Error opening file \"" << fname << "\", terminating\n";
		return 2;
	}
	file.ignore(23);

	uint32_t magic = readUInt32();
	bool b = magic == 0x99A12720;
	if (!b)
	{
		cout << "Magic bytes incorrect in file " << fname << ", terminating\n";
		printf("0x%08X", magic);
		return 1;
	}
	string fileVersion = to_string(readUInt8()) + '.' + to_string(readUInt8()) + '.' + to_string(readUInt8());
	uint32_t startTime = readUInt32();
	uint32_t pidLoopFreq = (3200 / (1 << readUInt8()));
	uint32_t loopDivider = readUInt8();
	uint32_t logFreq = pidLoopFreq / loopDivider;
	uint8_t gyrAccelRange = readUInt8();
	if (print)
	{
		cout << "File version: " << fileVersion << endl;
		cout << "Start time: " << startTime << endl;
		cout << "PID loop frequency: " << pidLoopFreq << " Hz" << endl;
		cout << "Loop divider: " << loopDivider << endl;
		cout << "Gyro/Accel range: " << (gyrAccelRange & 0b111) << ' ' << (gyrAccelRange >> 3) << endl;
	}
	double rateFactors[5][3];
	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			rateFactors[i][j] = (double)readInt32() / 65536.;
		}
	}
	if (print)
	{
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
	}
	double pidConstants[3][7];
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 7; j++)
		{
			pidConstants[i][j] = (double)readInt32() / 65536.;
		}
	}
	if (print)
	{
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
	}
	uint64_t bbFlags = readUInt64();
	uint32_t dataFrameLength = 0;
	for (int i = 0; i < (sizeof(logFlags)) / (sizeof(Flag)); i++)
	{
		if (bbFlags & (1 << i))
		{
			if (print)
				cout << "  - " << logFlags[i].name << endl;
			dataFrameLength += logFlags[i].length;
		}
	}
	if (print)
		cout << "  => " << dataFrameLength << " Bytes per data frame" << endl;
	int pos = file.tellg();
	file.seekg(0, ios::end);
	int end = file.tellg();
	end -= 30;
	end -= pos;
	int frameCount = end / dataFrameLength;
	if (print)
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

	file.close();

	string ofname = "./";
	ofname += fname;
	if (hasEnding(ofname, ".rpbb.txt"))
		ofname = ofname.substr(0, ofname.length() - 9);
	else if (hasEnding(ofname, ".rpbb"))
		ofname = ofname.substr(0, ofname.length() - 5);
	else if (hasEnding(ofname, ".txt"))
		ofname = ofname.substr(0, ofname.length() - 4);
	ofname += ".json";
	ofstream json(ofname);
	json << "{\n";
	json << "\t\"version\": \"" << fileVersion << "\",\n";
	json << "\t\"startTime\": " << startTime << ",\n";
	json << "\t\"pidLoopFreq\": " << pidLoopFreq << ",\n";
	json << "\t\"loopDivider\": " << loopDivider << ",\n";
	json << "\t\"gyroRange\": " << (2000 >> (gyrAccelRange & 0b111)) << ",\n";
	json << "\t\"accelRange\": " << (2 << (gyrAccelRange >> 3)) << ",\n";
	json << "\t\"rateFactors\": {\n";
	json << "\t\t\"roll\": [";
	for (int i = 0; i < 5; i++)
	{
		json << rateFactors[i][0];
		if (i < 4)
			json << ", ";
	}
	json << "],\n";
	json << "\t\t\"pitch\": [";
	for (int i = 0; i < 5; i++)
	{
		json << rateFactors[i][1];
		if (i < 4)
			json << ", ";
	}
	json << "],\n";
	json << "\t\t\"yaw\": [";
	for (int i = 0; i < 5; i++)
	{
		json << rateFactors[i][2];
		if (i < 4)
			json << ", ";
	}
	json << "]\n";
	json << "\t},\n";
	json << "\t\"pidConstants\": {\n";
	json << "\t\t\"roll\": {\n";
	json << "\t\t\t\"kP\": " << pidConstants[0][0] << ",\n";
	json << "\t\t\t\"kI\": " << pidConstants[0][1] << ",\n";
	json << "\t\t\t\"kD\": " << pidConstants[0][2] << ",\n";
	json << "\t\t\t\"kFF\": " << pidConstants[0][3] << ",\n";
	json << "\t\t\t\"kS\": " << pidConstants[0][4] << ",\n";
	json << "\t\t\t\"iFalloff\": " << pidConstants[0][5] << "\n";
	json << "\t\t},\n";
	json << "\t\t\"pitch\": {\n";
	json << "\t\t\t\"kP\": " << pidConstants[1][0] << ",\n";
	json << "\t\t\t\"kI\": " << pidConstants[1][1] << ",\n";
	json << "\t\t\t\"kD\": " << pidConstants[1][2] << ",\n";
	json << "\t\t\t\"kFF\": " << pidConstants[1][3] << ",\n";
	json << "\t\t\t\"kS\": " << pidConstants[1][4] << ",\n";
	json << "\t\t\t\"iFalloff\": " << pidConstants[1][5] << "\n";
	json << "\t\t},\n";
	json << "\t\t\"yaw\": {\n";
	json << "\t\t\t\"kP\": " << pidConstants[2][0] << ",\n";
	json << "\t\t\t\"kI\": " << pidConstants[2][1] << ",\n";
	json << "\t\t\t\"kD\": " << pidConstants[2][2] << ",\n";
	json << "\t\t\t\"kFF\": " << pidConstants[2][3] << ",\n";
	json << "\t\t\t\"kS\": " << pidConstants[2][4] << ",\n";
	json << "\t\t\t\"iFalloff\": " << pidConstants[2][5] << "\n";
	json << "\t\t}\n";
	json << "\t},\n";
	json << "\t\"enabledFlags\": [\n";
	string flags[(sizeof(logFlags)) / (sizeof(Flag))];
	int j = 0;
	for (int i = 0; i < (sizeof(logFlags)) / (sizeof(Flag)); i++)
		if (bbFlags & (1 << i))
			flags[j++] = logFlags[i].name;
	json << "\t\t\"" << joinArray(flags, j, "\",\n\t\t\"") << "\"\n\t],\n";
	json << "\t\"data\": {\n";
	if (bbFlags & (1 << 0))
	{
		json << "\t\t\"rollELRSRaw\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollELRSRaw[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 1))
	{
		json << "\t\t\"pitchELRSRaw\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchELRSRaw[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 2))
	{
		json << "\t\t\"throttleELRSRaw\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << throttleELRSRaw[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 3))
	{
		json << "\t\t\"yawELRSRaw\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawELRSRaw[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 4))
	{
		json << "\t\t\"rollSetpoint\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollSetpoint[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 5))
	{
		json << "\t\t\"pitchSetpoint\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchSetpoint[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 6))
	{
		json << "\t\t\"throttleSetpoint\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << throttleSetpoint[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 7))
	{
		json << "\t\t\"yawSetpoint\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawSetpoint[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 8))
	{
		json << "\t\t\"rollGyroRaw\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollGyroRaw[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 9))
	{
		json << "\t\t\"pitchGyroRaw\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchGyroRaw[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 10))
	{
		json << "\t\t\"yawGyroRaw\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawGyroRaw[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 11))
	{
		json << "\t\t\"rollPIDP\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollPIDP[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 12))
	{
		json << "\t\t\"rollPIDI\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollPIDI[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 13))
	{
		json << "\t\t\"rollPIDD\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollPIDD[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 14))
	{
		json << "\t\t\"rollPIDFF\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollPIDFF[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 15))
	{
		json << "\t\t\"rollPIDS\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << rollPIDS[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 16))
	{
		json << "\t\t\"pitchPIDP\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchPIDP[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 17))
	{
		json << "\t\t\"pitchPIDI\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchPIDI[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 18))
	{
		json << "\t\t\"pitchPIDD\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchPIDD[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 19))
	{
		json << "\t\t\"pitchPIDFF\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchPIDFF[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 20))
	{
		json << "\t\t\"pitchPIDS\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << pitchPIDS[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 21))
	{
		json << "\t\t\"yawPIDP\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawPIDP[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 22))
	{
		json << "\t\t\"yawPIDI\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawPIDI[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 23))
	{
		json << "\t\t\"yawPIDD\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawPIDD[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 24))
	{
		json << "\t\t\"yawPIDFF\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawPIDFF[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 25))
	{
		json << "\t\t\"yawPIDS\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << yawPIDS[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 26))
	{
		json << "\t\t\"motorOutputs\": [\n\t\t\t";
		for (int i = 0; i < frameCount; i++)
		{
			json << '[' << ((motorOutputs[i] >> 36) & 0xFFF) << ", " << ((motorOutputs[i] >> 24) & 0xFFF) << ", " << ((motorOutputs[i] >> 12) & 0xFFF) << ", " << (motorOutputs[i] & 0xFFF) << ']';
			if (i < frameCount - 1)
				json << ",\n\t\t\t";
		}
		json << "\n\t\t],\n";
	}
	if (bbFlags & (1 << 27))
	{
		json << "\t\t\"altitude\": [";
		for (int i = 0; i < frameCount; i++)
		{
			json << altitude[i];
			if (i < frameCount - 1)
				json << ", ";
		}
		json << "\n\t\t],\n";
	}
	json << "\t}\n";
	json << "}\n";
	json.close();
	if (print)
		cout << "\n\n";
	cout << "Successfully converted " << fname << " to " << ofname.substr(2) << endl;
	return 0;
}