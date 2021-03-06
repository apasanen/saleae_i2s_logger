#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cassert>
#include <sstream>
#include <signal.h>
#include <string>
#include <cstring>
#include <memory>
#include <iostream>
#include <SaleaeDeviceApi.h>
#if defined(WIN32)
 #include <windows.h>
#else
 #include <unistd.h>
#endif
#include "voltmeter.hpp"
#include "wavfile.hpp"

void __stdcall OnConnect(U64 device_id, GenericInterface* device_interface,
			 void* user_data );
void __stdcall OnDisconnect(U64 device_id, void* user_data );
void __stdcall OnReadData(U64 device_id, U8* data, U32 data_length,
			  void* user_data );
void __stdcall OnWriteData(U64 device_id, U8* data, U32 data_length,
			   void* user_data );
void __stdcall OnError( U64 device_id, void* user_data );

//#define NDEBUG
#define BITS 24
#define BYTES ((BITS)/8)
#define CHANNELS 4
#define WIRES 2
#define WAIT_SETUP_SEC 1
#define WAIT_TEARDOWN_SEC 1
#define AUDIO_SAMPLING_RATE (48000)
#if defined(WIN32)
 #define USLEEP(t) Sleep((DWORD) ((t)/1e3))
 #define ENABLE_GRAPHICS 0
#else
 #define USLEEP(t) usleep((t))
 #define ENABLE_GRAPHICS 1
#endif
/* Disable to write raw data for debug purposes */
#define USE_WAV 1
#if 0
#define DBG(...) fprintf (stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif

#define NUM_ELEMENTS(array) (sizeof(array)/sizeof(array[0]))
LogicInterface* gDeviceInterface = NULL;
U64 gLogicId = 0;
volatile bool loop = true;
FILE * fdbg = NULL;
#if USE_WAV
WavFile * wav = NULL;
#else
FILE * wav = NULL;
#endif

enum protocol_state {
	IDLE,
	FRAME_START,
	FRAME_FIRST_BIT,
	FRAME_ACTIVE,
	DATA_BIT_ACTIVE,
};

struct protocol_transition {
	protocol_state current_state;
	U8 mask;
	U8 match;
	protocol_state new_state;
	void (*transition)(int state_index, U8 data);
};

protocol_state current_state = IDLE;
int channel[WIRES*CHANNELS] = { 0 };
int current_channel;
int current_bit;
int ascii = 0;
volatile unsigned long ndata = 0;


void handle_frame_end(int state_index, U8 data)
{
	DBG("%s\n", __func__);
	int channel_count = WIRES * CHANNELS;
	char arr[WIRES * CHANNELS * BYTES];
	for (int i = 0; i < channel_count; i++) {
		arr[i*BYTES+0] = (channel[i] & 0x0000ff) >> 0;
		arr[i*BYTES+1] = (channel[i] & 0x00ff00) >> 8;
		arr[i*BYTES+2] = (channel[i] & 0xff0000) >> 16;
	}

	if(wav) {
#if USE_WAV
		if(wav->write(arr, 1) != 1){
			fprintf(stderr, "Error in writing wav file.\n");
			exit(1);
		}
#else
		fwrite(arr, sizeof(arr), 1, wav);
#endif
	}
	ndata++;

	current_channel = 0;
	current_bit = 0;

	memset(channel, 0, sizeof(channel));
}

void handle_data_bit(int state_index, U8 data)
{
	if(current_channel < CHANNELS){
		channel[current_channel] <<= 1;
		channel[current_channel+CHANNELS] <<= 1;

		/* if (data & 0b00000100) */
		if (data & (0x01<<2))
			channel[current_channel] |= 1;
		/* if (data & 0b00001000) */
		if (data & (0x01<<3))
			channel[current_channel+CHANNELS] |= 1;

		current_bit++;
		if (current_bit == BITS) {
			current_bit = 0;
			current_channel++;
		}
	}
}

struct protocol_transition state_machine[] = {
	//current_state,    mask,      match,  new state
	{IDLE, 0x01, 0x01, FRAME_START, NULL},
	{FRAME_START, 0x02, 0x02, FRAME_FIRST_BIT, NULL},
	{FRAME_FIRST_BIT, 0x02, 0x00, FRAME_START, handle_data_bit},
	{FRAME_START, 0x01, 0x00, FRAME_ACTIVE, NULL},
	{FRAME_ACTIVE, 0x02, 0x02, DATA_BIT_ACTIVE, NULL},
	{DATA_BIT_ACTIVE, 0x02, 0x00, FRAME_ACTIVE, handle_data_bit},
	{FRAME_ACTIVE, 0x01, 0x01, FRAME_START, handle_frame_end},
};

void transition(U8 data)
{
	DBG("%s %d\n", __func__, current_state);
	int i;
	for (i = 0; i < NUM_ELEMENTS(state_machine); i++) {
		if (state_machine[i].current_state == current_state &&
		    (data & state_machine[i].mask) == state_machine[i].match) {
			if (state_machine[i].transition) {
				state_machine[i].transition(i, data);
			}
			current_state = state_machine[i].new_state;
			break;
		}
	}
}

void intHandler(int dummy=0) {
	// catch the ctrl-c
	loop = false;
}

int main( int argc, char *argv[])
{
	DBG("%s\n", __func__);
	signal(SIGINT, intHandler);
	U32 gSampleRateHz = 24000000;
	int readtime_sec = -1;
	bool verbose = false;
	assert(sizeof(int) == 4);


	for (int i = 1;i<argc;i++){
		std::string arg(argv[i]);
		if(arg == "-h"){
			std::cout << "usage: " << argv[0]
				  << " [-v] " 
				  << "[-r rate] "
				  << "[-t time] "
				  << "[-d raw_data.bin] " 
				  << "[file.wav] "
				  << std::endl;
			printf("Options:\n");
			printf(" %-20s%s\n", "-v", "Verbose mode");
			printf(" %-20s%s (%ld hz).\n", "-r", "Logic sampling rate", gSampleRateHz);
			printf(" %-20s%s\n", "-t", "Recogding time in seconds");
			printf(" %-20s%s\n", "-d", "Crate raw data file");
			printf(" %-20s%s\n", "-h", "Usage instructions");
			printf(" %-20s%s\n", "file.wav", "Create wav file");
			std::cout << std::endl << std::endl << "Logic wiring:" << std::endl;
			std::cout << " 1 - Frame Sync" << std::endl;
			std::cout << " 2 - Bit Clock" << std::endl;
			std::cout << " 3 - Data 1" << std::endl;
			std::cout << " 4 - Data 2" << std::endl;
			std::cout << "Note! TDM DSP Mode B, "
				  << " (bit clock inverted)"  << std::endl;
			DevicesManagerInterface::BeginConnect(); // Bug in SDK
			exit(0);
		}

		if(arg == "-r" && i + 1 < argc){
			++i;
			std::istringstream ( std::string(argv[i]) ) >>
				gSampleRateHz;
			continue;
		}

		if(arg == "-v"){
			verbose = true;
			continue;
		}

		if(arg == "-d" && i + 1 < argc){
			++i;
			std::cout << "Opening raw data file:" <<  argv[i] <<
				"." << std::endl;
			fdbg = fopen(argv[i], "wb");
			assert(fdbg);
			continue;
		}

		if(arg == "-t" && i + 1 <  argc){
			++i;
			std::istringstream ( std::string(argv[i]) ) >>
				readtime_sec;
			continue;
		}
#if USE_WAV
		wav = new WavFile(arg, "wb");
		wav->sampleRate(AUDIO_SAMPLING_RATE);
		wav->channelCount(WIRES * CHANNELS);
		wav->bitsPerSample(BITS);
#else
		wav = fopen(arg.c_str(), "wb");
#endif
		assert(wav);
	}

	DevicesManagerInterface::RegisterOnConnect( &OnConnect,
						    &gSampleRateHz);
	DevicesManagerInterface::RegisterOnDisconnect( &OnDisconnect );
	DevicesManagerInterface::BeginConnect();

	USLEEP(WAIT_SETUP_SEC*1e6);
	if (gDeviceInterface == NULL){
		std::cerr << "Sorry, no devices are connected." << std::endl;
		return 1;
	}
	gDeviceInterface->ReadStart();

	if(readtime_sec>0)
		std::cerr << "Reading data for " << readtime_sec <<
			" seconds." << std::endl;

	VoltMeter vm(WIRES * CHANNELS, 10, -130, 0, 20, ENABLE_GRAPHICS);
	std::cerr << "Press CTRL-C to quit" << std::endl;

#if USE_WAV
	double db[2*CHANNELS];
	if(wav && verbose) {
		wav->level_db(db);
		vm.set(db);
	}
#endif

	while(loop){
		USLEEP(0.19 * 1e6);
#if USE_WAV
		if(verbose && wav){
			if (ENABLE_GRAPHICS)
				printf("\033[%dA", wav->channelCount());
			else
				printf("\r");
			wav->level_db(db);
			vm.set(db);
		}
#endif
		fprintf(stderr, " %10.2f s.\r", (double) ndata/AUDIO_SAMPLING_RATE);
		if(readtime_sec > 0 && (double)ndata/AUDIO_SAMPLING_RATE > readtime_sec){
			loop = false;
		}
	}

	if(gDeviceInterface->IsStreaming())
		gDeviceInterface->Stop();

	std::cerr << std::endl << ndata << " samples read." << std::endl;
	USLEEP(WAIT_TEARDOWN_SEC*1e6);

	if(fdbg)
		fclose(fdbg);

	if(wav) {
#if USE_WAV
		delete wav;
#else
		fclose(wav);
#endif
		wav = NULL;
	}

	return 0;
}

void __stdcall OnConnect( U64 device_id, GenericInterface* device_interface,
			  void* user_data )
{
	DBG("%s\n", __func__);

	if( dynamic_cast<LogicInterface*>( device_interface ) != NULL ){
		U32* gSampleRateHz = (U32*) user_data;
		std::cerr << "A Logic device was connected (id=0x" <<
			std::hex << device_id << std::dec <<  ") at " <<
			*gSampleRateHz << "Hz." << std::endl;
		gDeviceInterface = (LogicInterface*)device_interface;
		gLogicId = device_id;
		gDeviceInterface->RegisterOnReadData( &OnReadData );
		gDeviceInterface->RegisterOnError( &OnError );
		gDeviceInterface->SetSampleRateHz( *gSampleRateHz );
	}
}

void __stdcall OnDisconnect( U64 device_id, void* user_data )
{
	DBG("%s\n", __func__);
	if( device_id == gLogicId ){
		std::cerr << "A device was disconnected (id=0x" <<
			std::hex << device_id << std::dec << ")." <<
			std::endl;
		gDeviceInterface = NULL;
	}
}

void __stdcall OnReadData( U64 device_id, U8* data, U32 data_length,
			   void* user_data )
{
	DBG("%s\n", __func__);
	if(fdbg)
		fwrite(data, sizeof(U8), data_length, fdbg);

	for (unsigned i = 0; i < data_length; i++) {
		transition(data[i]);
	}

	/*
	 * you own this data.  You don't have to delete it immediately,
	 * you could keep it and process it later, for example,
	 * or pass it to another thread for processing.
	 */
	DevicesManagerInterface::DeleteU8ArrayPtr( data );
}


void __stdcall OnError( U64 device_id, void* user_data )
{
	DBG("%s\n", __func__);
	std::cerr << "A device reported an Error. This probably means that it could not keep up at the given data rate, or was disconnected. You can re-start the capture automatically, if your application can tolerate gaps in the data." << std::endl;
	/*
	 * note that you should not attempt to restart data collection
	 *  from this function --
	 * you'll need to do it from your main thread
	 * (or at least not the one that just called this function).
	 */
	exit(EXIT_FAILURE);
}
