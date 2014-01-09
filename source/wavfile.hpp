#ifndef WAVFILE_HPP_
#define WAVFILE_HPP_

#include <string>
#include <cstdio>

using namespace std;

class WavFile
{
public:
	WavFile(const string & fileName, const string & mode);
	~WavFile();

	size_t read(void * buffer, int Nframes);
	size_t write(const void * buffer, int Nframes);

	int sampleRate() const;
	void sampleRate(int rate);
	int channelCount() const;
	void channelCount(int count);
	int bitsPerSample() const;
	void bitsPerSample(int nbits);
	int Subchunk2Size() const;
	void Subchunk2Size(int size);

	float pos_s() const;
	void rewind();
	bool closed() const;
	bool eof() const;
	float level_db(int ch_id);
	void level_db(double * db);

private:
	FILE * mFid;
	const string mFileName;
	const string mMode;
	unsigned n_samples;
	static const size_t WAV_HEADER_SIZE = 44;

	struct WavHeader {
		int ChunkSize;
		int Subchunk1Size;
		int AudioFormat;
		int NumChannels;
		int SampleRate;
		int ByteRate;
		int BlockAlign;
		int BitsPerSample;
		int Subchunk2Size;
	};

	float * mLevels;
	struct WavHeader mHeader;
	fpos_t fDataPos;

	void byteRate();

	void read_header(struct WavHeader * header);
	void write_header(struct WavHeader * header);
};
#endif /* WAVREADER_HPP_ */
