/*#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <AL/alc.h>
#include <AL/al.h>
#include "stb_vorbis.c"

#define MAX_SAMPLE_COUNT 128
#define SAMPLE_SIZE 16

//4kb max
#define MAX_FRAME_SIZE 4 * 1024 

static int playing = 0;

//Decode (Use a number of bytes) -> Rebuffer (move unused bytes to front, refill to capacity) -> Loop

typedef struct {
	//OpenAL resources
	unsigned int source;
	unsigned int buffers[2]; 
	int format; // Mono / Stereo

	short* pcm; // buffered PCM
	int pcm_limit; // max length of PCM in bytes

	//Vorbis Decoder
	stb_vorbis* vorbis;

	//Position States (bytes)
	unsigned int p; // position
	unsigned int q; // query

	//Raw Data
	unsigned char* frame; // raw data stored
	unsigned int frame_size; //  size of data stored
	unsigned int frame_capacity;
	unsigned int frame_offset; // offset from start of file
	unsigned int file_length; // in bytes of overall file
} stream;

static char* test_file = "res/test.ogg";

unsigned char* request_data(int* file_length, int* length_read, int offset, int requested_length){
	unsigned char* data;

	FILE* f = fopen(test_file, "r+");
	
	assert(f);

	fseek(f, 0, SEEK_END);
	int true_length = ftell(f);

	if(file_length){
		*file_length = true_length;
	}
	
	if(offset < true_length){
		fseek(f, offset, SEEK_SET);

		int data_left = true_length - offset;
		int to_read = (data_left > requested_length) ? requested_length : data_left;

		printf("[%i] : [%i] :: [%i]\n", data_left, offset, to_read);	
		assert(to_read > 0);	

		data = (unsigned char*)malloc(to_read + 1);

		int actually_read = fread(data, sizeof(char), to_read, f);

		if(length_read){
			*length_read = actually_read;
		}

		data[actually_read] = '\0';

		fclose(f);

		return data;
	}else{
		printf("Offset is outside of file bounds.\n");
		fclose(f);
		*length_read = 0;
		return 0;
	}
}	

void rebuffer(stream* s){
	if(!s){
		return;
	}
	
	if(s->frame_offset + s->frame_size >= s->file_length){
		printf("No data to append for.\n");
		return;	
	}
	
	int data_read;
	unsigned char* data_from_file = request_data(NULL, &data_read, s->frame_offset, s->frame_capacity);
			
	if(!data_from_file || !data_read){
		printf("Unable to read data from file.\n");
		return;
	}	

	int unused = s->frame_size - s->p;
	int remainder = s->frame_capacity - (s->p + unused);
	memcpy(s->frame, s->frame[s->p], unused);
	
	int file_cpy_limit = (s->p > data_read) ? data_read : s->p;
	memcpy(&s->frame[unused], data_from_file, file_cpy_limit);

	s->frame_offset += file_cpy_limit;
	s->frame_size = file_cpy_limit + unused;

	if(file_cpy_limit < remainder){
		memset(&s->frame[unused + file_cpy_limit], 0, remainder - file_cpy_limit);
	}

	s->p = 0;
}


void conv_out(int buf_c, short* buffer, int data_c, float** data, int d_offset, int len){
	if(!data || !buffer){
		printf("No data passed.\n");
		return;
	}

	int limit = buf_c < data_c ? buf_c : data_c; 
	int i;

	int buffer_index = 0;
	for(int j=0;j<len;++j){	
		//each channel
		for(i=0;i<limit;++i){
			buffer[buffer_index + j] = data[i][d_offset+j] * 32767;
			++buffer_index;
			//float f = data[j][d_offset+i];
			//*buffer++ = f * 32767;		
		}
		//place nothing into the other channel for the data
		for( ;i<buf_c;++i){
			buffer[buffer_index + j] = 0;
			++buffer_index;		
		}
	}

	printf("[%i] shorts converted.\n", buffer_index);
}

stream init_push(unsigned char* data, int length, int file_length){
	stream s = (stream){0};
	s.p = 0;
	//initialize with whole buffer
	unsigned int q = MAX_FRAME_SIZE;

	assert(length > 0);
	unsigned char* frame_data = (unsigned char*)malloc(length);
	memcpy(frame_data, data, length);

	free(data);

	s.frame = frame_data;
	s.frame_size = length;
	s.frame_offset = 0;
	s.file_length = file_length;

	int used, error, retries;
retry:
	s.vorbis = stb_vorbis_open_pushdata(frame_data, length, &used, &error, NULL);
	if(!s.vorbis){
		if(error = VORBIS_need_more_data){
			printf("Not enough data in [%i] to initialize push data API.\n", length);
		}

		printf("Error: [%i]\n", error);
		return s;
	}

	s.format = (s.vorbis->channels) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

	printf("Started at pushing [%i] resulting in [%i] used.\n", q, used);
	s.p += used;

	alGenSources(1, &s.source);
	alGenBuffers(2, &s.buffers);

	alSourcef(s.source, AL_GAIN, 1.f);

	rebuffer(&s);

	return s;	
}

int push_decode(stream* s){
	float** out;
	int num_samples, num_channels, bytes_used; //used in bytes
	int rebuffered = 0;

retry_decode:
	bytes_used = stb_vorbis_decode_frame_pushdata(s->vorbis, s->frame + s->p, s->frame_size, &num_channels, &out, &num_samples);

	if(bytes_used == 0){
		if(rebuffered){
			printf("Unable to decode any samples from whole frame size: [%i]\n", s->frame_size);
			return 0;
		}

		//attempt to rebuffer if the offset is set
		if(s->p > 0){
			rebuffer(s);
			rebuffered = 1;
			goto retry_decode;
		}

		//Reach on fail	
		return 0;	
	}

	s->p += bytes_used;

	if(num_samples == 0){
		printf("[%i] bytes used without creating any samples.\n", bytes_used);
		return 0;
	}else{
		printf("[%i] bytes used successfully.\n", bytes_used);
	}

	s->format = (s->vorbis->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	
	int samples_to_buffer = (MAX_SAMPLE_COUNT > num_samples) ? num_samples : MAX_SAMPLE_COUNT;
	if(!s->pcm){
		s->pcm_limit = MAX_SAMPLE_COUNT * s->vorbis->channels * sizeof(short);
		assert(s->pcm_limit > 0);
		s->pcm = malloc(s->pcm_limit);
		memset(s->pcm, 0, s->pcm_limit);	
	}

	//0 since we're not doing any offset for the data conversion	
	conv_out(s->vorbis->channels, s->pcm, s->vorbis->channels, out, 0, samples_to_buffer);
		
	return samples_to_buffer;
}

int main(int argc, char** argv){	
	//initialize openal
	ALCdevice* device = alcOpenDevice(NULL);
	ALCcontext* context = alcCreateContext(device, NULL);

	if(!alcMakeContextCurrent(context)){
		printf("Error creating OpenAL Context\n");
		return EXIT_FAILURE;
	}

	//straight_decode(&source, &buffers[0]);
	int length, file_length;
	unsigned char* data = request_data(&file_length, &length, 0, MAX_FRAME_SIZE);	
	int max_frame = (MAX_FRAME_SIZE > length) ? length : MAX_FRAME_SIZE;

	stream s = init_push(data, max_frame, file_length); 
	assert(s.vorbis);

	alListenerf(AL_GAIN, 1.f);

	for(int i=0;i<2;++i){
		unsigned int buffer_id = s.buffers[i];
		int samples_buffered = push_decode(&s);
		alBufferData(buffer_id, s.format, s.pcm, samples_buffered * (SAMPLE_SIZE / 8 * s.vorbis->channels), s.vorbis->sample_rate);
		alSourceQueueBuffers(s.source, 1, &buffer_id);
	}
	alSourcePlay(s.source);
	playing = 1;

	while(1){
		ALenum state;
		ALint proc;

		alGetSourcei(s.source, AL_SOURCE_STATE, &state);
		alGetSourcei(s.source, AL_BUFFERS_PROCESSED, &proc);

		int buffers_processed = 0;

		if(state == AL_STOPPED){
			playing = 0;
		}else if(proc > 0){
			for(int i=0;i<proc;++i){
				ALuint buffer_id;
				ALint error;
				alSourceUnqueueBuffers(s.source, 1, &buffer_id);
				if(!buffer_id || (error = alGetError()) == AL_INVALID_VALUE){
					printf("Unable to get buffer from source.\n");
					continue;
				}
				//buffer data
				int samples_buffered = push_decode(&s);

				//assert(s.pcm);

				alBufferData(buffer_id, s.format, s.pcm, samples_buffered * (SAMPLE_SIZE / 8 * s.vorbis->channels), s.vorbis->sample_rate);
				alSourceQueueBuffers(s.source, 1, &buffer_id);
				++buffers_processed;
			}
		}
	}

	return EXIT_SUCCESS;
}*/
