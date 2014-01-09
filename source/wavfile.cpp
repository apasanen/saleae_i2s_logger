#include <cstdlib>
#include <iostream>
#include <cstring>
#include <limits>
#include <cmath>
#include <cassert>
#include "wavfile.hpp"

#define ABSMAX(x,pos) ((fabs(x)>(pos))?fabs(x):(pos))

void textCompare(const char ** p, const char * str)
{
	size_t s = strlen(str);
	int ret = strncmp(p[0], str, s);
	if(ret != 0){
		char tmp[s+1];
		tmp[s] = '\0';
		//throw (string) "Error: " + str + " not found in wav header.";
		fprintf(stderr,
			"Error: %s not found in wav header (%s).\n", str,
			strncpy(tmp, p[0], s));
		exit(1);
	}
	*p += s;
}

short getValue2(const char ** p, bool littleEndian = true)
{
	short value;
	const char * table = p[0];
	if(!littleEndian){
		value = (0xff & table[0]) << 8 | (0xff & table[1]) << 0;
	} else {
		value = (0xff & table[0]) << 0 | (0xff & table[1]) << 8;
	}
	*p += 2;
	return value;
}

void setValue2(char ** p, short value, bool littleEndian = true)
{
	char * table = p[0];
	if(!littleEndian){
		// value = (0xff & table[0]) << 8 | (0xff & table[1]) << 0;
		table[0] = (0xff00 & value) << 8;
		table[1] = (0x00ff & value) << 0;
	} else {
		// value = (0xff & table[0]) << 0 | (0xff & table[1]) << 8;
		table[0] = (0x00ff & value) << 0;
		table[1] = (0xff00 & value) << 8;
	}
	*p += 2;
}


int getValue4(const char ** p, bool littleEndian = true)
{
	const char * table = p[0];
	int value;
	if(!littleEndian){
		value = (0xff & table[0]) << 24 |
			(0xff & table[1]) << 16 |
			(0xff & table[2]) << 8 |
			(0xff & table[3]) << 0;
	} else {
		value = (0xff & table[0]) << 0 |
			(0xff & table[1]) << 8 |
			(0xff & table[2]) << 16 |
			(0xff & table[3]) << 24;
	}
	*p += 4;
	return value;
}

void setValue4(char ** p, int value, bool littleEndian = true)
{
	char * table = p[0];
	if(!littleEndian){
		table[0] =  (0xff000000 & value) >> 24;
		table[1] =  (0x00ff0000 & value) >> 16;
		table[2] =  (0x0000ff00 & value) >> 8;
		table[3] =  (0x000000ff & value) >> 0;
	} else {
		table[0] =  (0x000000ff & value) >> 0;
		table[1] =  (0x0000ff00 & value) >> 8;
		table[2] =  (0x00ff0000 & value) >> 16;
		table[3] =  (0xff000000 & value) >> 24;
	}
	*p += 4;
}


WavFile::WavFile(const string & fileName, const string & mode)
	:mFileName(fileName), mMode(mode), n_samples(0), mLevels(NULL)
{
	mFid = fopen(mFileName.c_str(), mMode.c_str());
	if(!mFid){
		fprintf(stderr, "Error opening filename: %s.\n",
			mFileName.c_str());
		exit(1);
	}

	if(mMode[0] == 'r'){
		// Read
		read_header(&mHeader);
	} else {
		WavHeader tmpHeader = {
			0,     // int ChunkSize;
			16,    // int Subchunk1Size;
			1,     // int AudioFormat;
			1,     // int NumChannels;
			48000, // int SampleRate;
			48000 * 16 * 1 / 8,     // int ByteRate;
			4,     // int BlockAlign;
			16,    // int BitsPerSample;
			0      // int Subchunk2Size;
		};
		mHeader = tmpHeader;
		fseek(mFid, WAV_HEADER_SIZE, SEEK_SET);
	}

	if(fgetpos(mFid, &fDataPos)){
		fprintf(stderr, "fsetpos() returned error.\n");
		exit(1);
	}
}


WavFile::~WavFile()
{
	if(mLevels) {
		delete [] mLevels;
		mLevels = NULL;
	}

	if(mFid) {
		if(mMode[0] == 'r') {
			// Read
		} else {
			long datasize = n_samples * bitsPerSample() /
				8 * mHeader.NumChannels;
			if (!mHeader.Subchunk2Size){
				mHeader.Subchunk2Size = datasize;
			}
			mHeader.ChunkSize = 36 + datasize;
			// Write
			write_header(&mHeader);
		}
		fclose(mFid);
	}
}

void WavFile::read_header(struct WavHeader * header)
{
	char table[WAV_HEADER_SIZE];
	if(fread(table, sizeof(char), WAV_HEADER_SIZE, mFid) !=
	   WAV_HEADER_SIZE){
		fprintf(stderr, "Error reading WAV-header.\n");
		exit(1);
	}

	const char * p = table;

	textCompare(&p, "RIFF");
	header->ChunkSize = getValue4(&p);
	//cout << "ChunkSize:" << header->ChunkSize << endl;

	textCompare(&p, "WAVE");
	textCompare(&p, "fmt ");

	header->Subchunk1Size = getValue4(&p);
	//cout << "Subchunk1Size:" << header->Subchunk1Size << endl;
	if(header->Subchunk1Size != 16){
		//		throw (string) "Error not Subchunk1Size";
		fprintf(stderr, "Error wrong Subchunk1Size.\n");
		exit(1);
	}

	header->AudioFormat = getValue2(&p);
	//cout << "AudioFormat:" << header->AudioFormat << endl;
	if(header->AudioFormat != 1){
		//		throw (string) "Error not AudioFormat";
		fprintf(stderr, "Error wrong AudioFormat.\n");
		exit(1);
	}

	header->NumChannels = getValue2(&p);
	//cout << "NumChannels:" << header->NumChannels << endl;

	header->SampleRate = getValue4(&p);
	//cout << "SampleRate:" << header->SampleRate << endl;

	header->ByteRate = getValue4(&p);
	//cout << "ByteRate:" << header->ByteRate << endl;

	header->BlockAlign = getValue2(&p);
	//cout << "BlockAlign:" << header->BlockAlign << endl;

	header->BitsPerSample = getValue2(&p);
	//cout << "BitsPerSample:" << header->BitsPerSample << endl;

	textCompare(&p, "data");

	header->Subchunk2Size = getValue4(&p);
	//cout << "Subchunk2Size:" << header->Subchunk2Size << endl;
}

void WavFile::write_header(struct WavHeader * header)
{
	char table[WAV_HEADER_SIZE];

	char * p = table;

	memcpy(p, "RIFF", 4);
	p+=4;

	setValue4(&p, header->ChunkSize);

	memcpy(p, "WAVE", 4);
	p+=4;
	memcpy(p, "fmt ", 4);
	p+=4;

	setValue4(&p, header->Subchunk1Size);
	setValue2(&p, header->AudioFormat);
	setValue2(&p, header->NumChannels);
	setValue4(&p, header->SampleRate);
	setValue4(&p, header->ByteRate);
	setValue2(&p, header->BlockAlign);
	setValue2(&p, header->BitsPerSample);

	memcpy(p, "data", 4);
	p+=4;

	setValue4(&p, header->Subchunk2Size);

	if(fseek(mFid, 0, SEEK_SET)){
		fprintf(stderr, "fseek() failed.\n");
		exit(1);
	}

	if(fwrite(table, sizeof(char), WAV_HEADER_SIZE, mFid) !=
	   WAV_HEADER_SIZE) {
		fprintf(stderr, "Error writing WAV-header.\n");
		exit(1);
	}
}


void update_levels(float * mLevels,
		   const void * buffer,
		   int BitsPerSample,
		   int nFrames,
		   int NumChannels)
{
	if(!mLevels)
		return;

	int i;

	if(BitsPerSample == 8){
		assert(sizeof(char) == BitsPerSample);
		const char * tbl = (const char *) buffer;
		for (i=0;i<nFrames;i++){
			for (int j=0;j<NumChannels;j++){
				mLevels[j] = ABSMAX(tbl[i * NumChannels + j],
						    mLevels[j]);
			}
		}
	} else if(BitsPerSample == 16) {
		assert(sizeof(short) == BitsPerSample);
		const short * tbl = (const short *) buffer;
		for (i=0;i<nFrames;i++) {
			for (int j=0;j<NumChannels;j++)
			{
				mLevels[j] = ABSMAX(tbl[i * NumChannels + j],
						    mLevels[j]);

			}
		}
	} else if(BitsPerSample == 24) {
		const unsigned char * tbl = (const unsigned char *) buffer;
		for (i=0;i<nFrames;i++) {
			for (int j=0;j<NumChannels;j++) {
				int value =
				       tbl[(i * NumChannels + j)*3 + 0] << 8;
				value |=
				       tbl[(i * NumChannels + j)*3  + 1] << 16;
				value |=
				       tbl[(i * NumChannels + j)*3  + 2] << 24;
				value >>= 8;
				mLevels[j] = ABSMAX(value, mLevels[j]);
			}
		}
	} else if(BitsPerSample == 32) {
		assert(sizeof(int) == BitsPerSample);
		const int * tbl = (const int *) buffer;
		for (i=0;i<nFrames;i++) {
			for (int j=0;j<NumChannels;j++) {
				mLevels[j] = ABSMAX(tbl[i * NumChannels + j],
						    mLevels[j]);
			}
		}
	} else {
      /* no handler */
    }
}

size_t WavFile::read(void * buffer, int nFrames)
{
	size_t ret = 0;
	if(mFid) {
		ret = fread(buffer, (mHeader.BitsPerSample / 8) *
			    mHeader.NumChannels, nFrames, mFid);
		update_levels(mLevels, buffer, mHeader.BitsPerSample, nFrames,
			      mHeader.NumChannels);
	}
	n_samples += ret;
	return ret;
}

size_t WavFile::write(const void * buffer, int nFrames)
{
	size_t ret = 0;
	if(mFid) {
		ret = fwrite(buffer, (mHeader.BitsPerSample / 8) *
			     mHeader.NumChannels, nFrames, mFid);
		update_levels(mLevels, buffer, mHeader.BitsPerSample, nFrames,
			      mHeader.NumChannels);
	}
	n_samples += ret;
	return ret;
}


bool WavFile::closed() const
{
	bool ret = true;
	if(mFid)
		ret = false;

	return ret;
}

bool WavFile::eof() const
{
	return (feof(mFid) != 0);
}

void WavFile::rewind()
{
	if(fsetpos(mFid, &fDataPos)) {
		fprintf(stderr, "fsetpos() returned error.\n");
		exit(1);
	}
	n_samples = 0;
}

void WavFile::byteRate()
{
	mHeader.ByteRate =
		mHeader.SampleRate * mHeader.NumChannels *
		mHeader.BitsPerSample / 8;
}

int WavFile::sampleRate() const
{
	return mHeader.SampleRate;
}

void WavFile::sampleRate(int rate)
{
	mHeader.SampleRate = rate;
	byteRate();
}

int WavFile::channelCount() const
{
	return mHeader.NumChannels;
}

void WavFile::channelCount(int count)
{
	mHeader.NumChannels = count;
	byteRate();
}

int WavFile::bitsPerSample() const
{
	return mHeader.BitsPerSample;
}

void WavFile::bitsPerSample(int nbits)
{
	mHeader.BitsPerSample = nbits;
	byteRate();
}

int WavFile::Subchunk2Size() const
{
	return mHeader.Subchunk2Size;
}

void WavFile::Subchunk2Size(int size)
{
	mHeader.Subchunk2Size = size;
}

float WavFile::pos_s() const
{
	return (float) n_samples / mHeader.SampleRate;
}

float WavFile::level_db(int ch)
{
	if(mLevels == NULL) {
		mLevels = new float [mHeader.NumChannels];
		int i;
		for (i=0;i<mHeader.NumChannels;i++) {
			mLevels[i] = 0;
		}
	}

	long unsigned v = ((long unsigned)0x1<<(mHeader.BitsPerSample-1));
	float value_db = 20*log10(mLevels[ch] / v);
	mLevels[ch] = 0;
	return value_db;
}

void WavFile::level_db(double * db)
{
	for (int i=0;i<mHeader.NumChannels;i++){
		db[i] = level_db(i);
	}
}
