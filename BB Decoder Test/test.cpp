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
	// list all .kbb files in current directory
	system("dir /b *.kbb,*.txt > list.tmp");
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

	uint32_t magic = readUInt32();
	bool b = magic == 0x99A12720;
	if (!b)
	{
		cout << "Magic bytes incorrect in file " << fname << ", terminating\n";
		printf("0x%08X", magic);
		return 1;
	}
	uint8_t version[3];
	version[0] = readUInt8();
	version[1] = readUInt8();
	version[2] = readUInt8();
	string fileVersion = to_string(version[0]) + "." + to_string(version[1]) + "." + to_string(version[2]);
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

	string ofname = "./";
	ofname += fname;
	if (hasEnding(ofname, ".kbb.txt"))
		ofname = ofname.substr(0, ofname.length() - 9);
	else if (hasEnding(ofname, ".kbb"))
		ofname = ofname.substr(0, ofname.length() - 5);
	else if (hasEnding(ofname, ".txt"))
		ofname = ofname.substr(0, ofname.length() - 4);
	ofname += ".json";
	ofstream json(ofname);
	json << "{\n";
	json << "\t\"frameCount\": " << frameCount << ",\n";
	json << "\t\"rawFile\": [\n";
	file.seekg(0, ios::end);
	int rawFileSize = file.tellg();
	file.seekg(0, ios::beg);
	for (int i = 0; i < rawFileSize; i++)
	{
		json << (int)readUInt8();
		if (i < rawFileSize - 1)
			json << ",";
		if (i % 32 == 31)
			json << "\n";
	}
	json << "\n\t],\n";
	json << "\t\"version\": [" << to_string(version[0]) << "," << to_string(version[0]) << "," << to_string(version[0]) << "],\n";
	json << "\t\"startTime\": " << startTime << ",\n";
	json << "\t\"pidFrequency\": " << pidLoopFreq << ",\n";
	json << "\t\"frequencyDivider\": " << loopDivider << ",\n";
	json << "\t\"framesPerSecond\": " << logFreq << ",\n";
	json << "\t\"ranges\": {\n";
	const uint16_t gyroRanges[] = {2000, 1000, 500, 250, 125};
	const uint16_t accelRanges[] = {2, 4, 8, 16};
	json << "\t\t\"gyro\": " << gyroRanges[gyrAccelRange & 0b111] << ",\n";
	json << "\t\t\"accel\": " << accelRanges[gyrAccelRange >> 3] << "\n";
	json << "\t},\n";
	json << "\t\"rateFactors\": [\n";
	for (int i = 0; i < 5; i++)
	{
		json << "\t\t[";
		for (int j = 0; j < 3; j++)
		{
			json << rateFactors[i][j];
			if (j < 2)
				json << ", ";
		}
		json << "\t\t]" << ((i == 4) ? "\n" : ",\n");
	}
	json << "\t],\n";

	json << "\t\"pidConstants\": [\n";
	for (int i = 0; i < 3; i++)
	{
		json << "\t\t[";
		for (int j = 0; j < 7; j++)
		{
			json << pidConstants[i][j];
			if (j < 6)
				json << ", ";
		}
		json << "\t\t]" << ((i == 2) ? "\n" : ",\n");
	}
	json << "\t],\n";
	json << "\t\"flags\": [\n";
	string flags[(sizeof(logFlags)) / (sizeof(Flag))];
	int j = 0;
	for (int i = 0; i < (sizeof(logFlags)) / (sizeof(Flag)); i++)
		if (bbFlags & (1 << i))
			flags[j++] = logFlags[i].name;
	json << "\t\t\"" << joinArray(flags, j, "\",\n\t\t\"") << "\"\n\t],\n";
	json << "\t\"frames\": [\n";
	for (int i = 0; i < frameCount; i++)
	{
		int elrsCount = 0, setpointCount = 0, gyroCount = 0, pidRollCount = 0, pidPitchCount = 0, pidYawCount = 0;
		json << "\t\t{\n";
		json << "\t\t\t\"elrs\": {\n";
		if (bbFlags & (1 << 0))
			json << (elrsCount++ ? "," : "") << "\t\t\t\t\"roll\": " << rollELRSRaw[i] << "\n";
		if (bbFlags & (1 << 1))
			json << (elrsCount++ ? "," : "") << "\t\t\t\t\"pitch\": " << pitchELRSRaw[i] << "\n";
		if (bbFlags & (1 << 2))
			json << (elrsCount++ ? "," : "") << "\t\t\t\t\"throttle\": " << throttleELRSRaw[i] << "\n";
		if (bbFlags & (1 << 3))
			json << (elrsCount++ ? "," : "") << "\t\t\t\t\"yaw\": " << yawELRSRaw[i] << "\n";
		json << "\t\t\t},\n";
		json << "\t\t\t\"setpoint\": {\n";
		if (bbFlags & (1 << 4))
			json << (setpointCount++ ? "," : "") << "\t\t\t\t\"roll\": " << rollSetpoint[i] << "\n";
		if (bbFlags & (1 << 5))
			json << (setpointCount++ ? "," : "") << "\t\t\t\t\"pitch\": " << pitchSetpoint[i] << "\n";
		if (bbFlags & (1 << 6))
			json << (setpointCount++ ? "," : "") << "\t\t\t\t\"throttle\": " << throttleSetpoint[i] << "\n";
		if (bbFlags & (1 << 7))
			json << (setpointCount++ ? "," : "") << "\t\t\t\t\"yaw\": " << yawSetpoint[i] << "\n";
		json << "\t\t\t},\n";
		json << "\t\t\t\"gyro\": {\n";
		if (bbFlags & (1 << 8))
			json << (gyroCount++ ? "," : "") << "\t\t\t\t\"roll\": " << rollGyroRaw[i] << "\n";
		if (bbFlags & (1 << 9))
			json << (gyroCount++ ? "," : "") << "\t\t\t\t\"pitch\": " << pitchGyroRaw[i] << "\n";
		if (bbFlags & (1 << 10))
			json << (gyroCount++ ? "," : "") << "\t\t\t\t\"yaw\": " << yawGyroRaw[i] << "\n";
		json << "\t\t\t},\n";
		json << "\t\t\t\"pid\": {\n";
		json << "\t\t\t\t\"roll\": {\n";
		if (bbFlags & (1 << 11))
			json << (pidRollCount++ ? "," : "") << "\t\t\t\t\t\"p\": " << rollPIDP[i] << "\n";
		if (bbFlags & (1 << 12))
			json << (pidRollCount++ ? "," : "") << "\t\t\t\t\t\"i\": " << rollPIDI[i] << "\n";
		if (bbFlags & (1 << 13))
			json << (pidRollCount++ ? "," : "") << "\t\t\t\t\t\"d\": " << rollPIDD[i] << "\n";
		if (bbFlags & (1 << 14))
			json << (pidRollCount++ ? "," : "") << "\t\t\t\t\t\"ff\": " << rollPIDFF[i] << "\n";
		if (bbFlags & (1 << 15))
			json << (pidRollCount++ ? "," : "") << "\t\t\t\t\t\"s\": " << rollPIDS[i] << "\n";
		json << "\t\t\t\t},\n";
		json << "\t\t\t\t\"pitch\": {\n";
		if (bbFlags & (1 << 16))
			json << (pidPitchCount++ ? "," : "") << "\t\t\t\t\t\"p\": " << pitchPIDP[i] << "\n";
		if (bbFlags & (1 << 17))
			json << (pidPitchCount++ ? "," : "") << "\t\t\t\t\t\"i\": " << pitchPIDI[i] << "\n";
		if (bbFlags & (1 << 18))
			json << (pidPitchCount++ ? "," : "") << "\t\t\t\t\t\"d\": " << pitchPIDD[i] << "\n";
		if (bbFlags & (1 << 19))
			json << (pidPitchCount++ ? "," : "") << "\t\t\t\t\t\"ff\": " << pitchPIDFF[i] << "\n";
		if (bbFlags & (1 << 20))
			json << (pidPitchCount++ ? "," : "") << "\t\t\t\t\t\"s\": " << pitchPIDS[i] << "\n";
		json << "\t\t\t\t},\n";
		json << "\t\t\t\t\"yaw\": {\n";
		if (bbFlags & (1 << 21))
			json << (pidYawCount++ ? "," : "") << "\t\t\t\t\t\"p\": " << yawPIDP[i] << "\n";
		if (bbFlags & (1 << 22))
			json << (pidYawCount++ ? "," : "") << "\t\t\t\t\t\"i\": " << yawPIDI[i] << "\n";
		if (bbFlags & (1 << 23))
			json << (pidYawCount++ ? "," : "") << "\t\t\t\t\t\"d\": " << yawPIDD[i] << "\n";
		if (bbFlags & (1 << 24))
			json << (pidYawCount++ ? "," : "") << "\t\t\t\t\t\"ff\": " << yawPIDFF[i] << "\n";
		if (bbFlags & (1 << 25))
			json << (pidYawCount++ ? "," : "") << "\t\t\t\t\t\"s\": " << yawPIDS[i] << "\n";
		json << "\t\t\t\t}\n";
		json << "\t\t\t},\n";
		json << "\t\t\t\"motors\": {\n";
		if (bbFlags & (1 << 26))
		{
			json << "\t\t\t\t\"rr\": " << ((motorOutputs[i] >> 36) & 0xFFF) << ",\n";
			json << "\t\t\t\t\"rl\": " << ((motorOutputs[i] >> 24) & 0xFFF) << ",\n";
			json << "\t\t\t\t\"fr\": " << ((motorOutputs[i] >> 12) & 0xFFF) << ",\n";
			json << "\t\t\t\t\"fl\": " << ((motorOutputs[i] >> 0) & 0xFFF) << "\n";
		}
		json << "\t\t\t}" << ((bbFlags & (1 << 27)) ? ",\n" : "\n");
		if (bbFlags & (1 << 27))
			json << "\t\t\t\"altitude\": " << altitude[i] << "\n";
		json << "\t\t}" << ((i == frameCount - 1) ? "\n" : ",\n");
	}
	json << "\t]\n";
	json << "}\n";
	json.close();
	if (print)
		cout << "\n\n";
	cout << "Successfully converted " << fname << " to " << ofname.substr(2) << endl;
	return 0;
}