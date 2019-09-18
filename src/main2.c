#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <AL/alc.h>
#include <AL/al.h>
#include "stb_vorbis.c"

#define MAX_FRAME_SIZE 4096 * 128
#define NUM_BUFFERS 4

static int playing = 0;
static char* test_file = "res/test.ogg";

static int initializing_buffers = 1;
static int max_length, offset;
static unsigned char* data;

static int prev_rate;
static int prev_channels;

static int is_playing, has_buffered;

unsigned char* get_data(int* length){
	FILE* f = fopen(test_file, "r+");

	if(!f){
		printf("Unable to open file: %s\n", test_file);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	int file_length = ftell(f);
	rewind(f); 

	unsigned char* data = (unsigned char*)malloc(file_length * sizeof(char));
	if(!data){
		fclose(f);
	   	return NULL;
	}


	if(length){
		*length = file_length;
	}

	fread(data, sizeof(unsigned char), file_length, f);
	data[file_length] = 0;
	fclose(f);

	return data;
}

int push_data(stb_vorbis* vorbis, unsigned int source, unsigned int buffers[2], int data_length, int max_length){
	float** out;
	int num_channels, num_samples, bytes_used;
	
	int buffered = 0;

	ALint processed;
	ALenum state;

	//alGetSourcei(source, AL_SOURCE_STATE, &state);
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

	if(!is_playing && processed == 0){
		has_buffered = 0;
		return -1;
	}

	if(processed > 0){
		for(int i=0;i<processed;++i){
			ALuint buffer_id;

			alSourceUnqueueBuffers(source, 1, &buffer_id);

			/*if(offset + data_length > max_length){
				data_length = max_length - offset;
				if(data_length == 0){
					is_playing = 0;
					return -1;
				}	
			}*/

		reprocess:
			bytes_used = stb_vorbis_decode_frame_pushdata(vorbis, data+offset, data_length, &num_channels, &out, &num_samples);
			if(bytes_used == 0){
				printf("Unable to load samples from data with length of [%i]\n", data_length);	
				return -1;
			}

			printf("Decoded [%i] samples from [%i] bytes.\n", num_samples, bytes_used);

			offset += bytes_used;

			if(num_samples > 0){
				int pcm_length = sizeof(short) * num_samples * num_channels;
				short* pcm = (short*)malloc(pcm_length);
				memset(pcm, 1, pcm_length);

				int pcm_index = 0;	

				//convert float to interleaved short
				for(int j=0;j<num_samples;++j){
					for(int i=0;i<num_channels;++i){
						pcm[pcm_index] = out[i][j] * 32767;
						++pcm_index;
					}
				}

				int format = (num_channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
				alBufferData(buffer_id, format, pcm, pcm_length, vorbis->sample_rate);  
				alSourceQueueBuffers(source, 1, &buffer_id);

				//printf("Buffered [%i] samples.\n", num_samples);

				free(pcm);
				++buffered;
			}else if(is_playing){
				goto reprocess;
			}
		}
	}

	if(buffered == NUM_BUFFERS){
		alSourcePlay(source);
	}

	return buffered;
}

int main(int argc, char** argv){	
	//initialize openal
	ALCdevice* device = alcOpenDevice(NULL);
	ALCcontext* context = alcCreateContext(device, NULL);

	if(!alcMakeContextCurrent(context)){
		printf("Error creating OpenAL Context\n");
		return EXIT_FAILURE;
	}

	static int q, error, used;
	static stb_vorbis* v;

	data = get_data(&max_length);
	printf("[%i] bytes in file.\n", max_length);

	offset = 0;

	q = 1;
retry_init:
	v = stb_vorbis_open_pushdata(data, q, &used, &error, NULL);
	if(v == NULL){
		if(error == VORBIS_need_more_data){
			q += 1;
			goto retry_init;
		}
		printf("Error: [%i]\n", error);
		return EXIT_FAILURE;
	}

	offset += used;

	printf("[%i] bytes used in header.\n", used);
	printf("[%i] sample rate.\n", v->sample_rate);

	unsigned int source, buffers[NUM_BUFFERS];

	alGenSources(1, &source);
	alGenBuffers(NUM_BUFFERS, &buffers);

	alListenerf(AL_GAIN, 1.f); 
	alSourcef(source, AL_GAIN, 1.f);

	int bytes_used, num_channels, num_samples;
	int data_length = MAX_FRAME_SIZE;
	float** out;
	for(int i=0;i<NUM_BUFFERS;++i){
		unsigned int buffer = buffers[i];
init_buffer:
		if(offset + data_length > max_length){
			data_length = max_length - offset;
		}

		bytes_used = stb_vorbis_decode_frame_pushdata(v, data+offset, data_length, &num_channels, &out, &num_samples);

		if(bytes_used == 0){
			printf("Unable to load samples from data with length of [%i]\n", data_length);	
			break;
		}

		//printf("Decoded [%i] samples from [%i] bytes.\n", num_samples, bytes_used);
		offset += bytes_used;

		if(num_samples > 0){
			int pcm_length = sizeof(short) * num_samples * num_channels;
			short* pcm = (short*)malloc(pcm_length);
			memset(pcm, 1, pcm_length);

			int pcm_index = 0;	

			//printf("Converting [%i] samples.\n", num_samples);

			//convert float to interleaved short
			for(int j=0;j<num_samples;++j){
				for(int i=0;i<num_channels;++i){
					pcm[pcm_index] = out[i][j] * 32767;
					++pcm_index;
				}
			}

			int format = (num_channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
			alBufferData(buffer, format, pcm, pcm_length, v->sample_rate);  
			alSourceQueueBuffers(source, 1, &buffer);

			free(pcm);
		}else{
			goto init_buffer;	
		}
	}

	alSourcePlay(source);
	is_playing = 1;
	has_buffered = 1;
	while(is_playing && has_buffered){
		int processed = push_data(v, source, buffers, MAX_FRAME_SIZE, max_length);
	}

	alcCloseDevice(device);

	return EXIT_SUCCESS;
}
