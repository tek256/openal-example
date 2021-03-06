#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <AL/alc.h>
#include <AL/al.h>
#include "stb_vorbis.c"

#define MAX_FRAME_SIZE     4096
#define NUM_BUFFERS        2
#define PACKETS_PER_BUFFER 16
#define VORBIS_PACKET_SIZE 256

#define MIN_BUFFER_SIZE VORBIS_PACKET_SIZE * 8

static char* test_file = "test.ogg";

static int            file_size, offset;
static unsigned char* data;

static short* pcm;
static int    pcm_capacity;

static int has_remaining;
static int is_playing, has_buffered;

typedef struct {
  unsigned char* data;
  int            offset, file_length;
} f_buffer;

unsigned char* get_data(int* length) {
  FILE* f = fopen(test_file, "r+");

  if (!f) {
    printf("Unable to open file: %s\n", test_file);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  int file_length = ftell(f);
  rewind(f);

  unsigned char* data = (unsigned char*)malloc(file_length * sizeof(char));
  if (!data) {
    fclose(f);
    return NULL;
  }

  if (length) {
    *length = file_length;
  }

  fread(data, sizeof(unsigned char), file_length, f);
  data[file_length] = 0;
  fclose(f);

  return data;
}

f_buffer get_buffer(const char* fp) {
  f_buffer buffer;
  buffer.data = get_data(&buffer.file_length);
  return buffer;
}

int push_data(stb_vorbis* vorbis, unsigned int source, unsigned int buffers[2],
              f_buffer* buffer) {
  float** out;
  int     num_channels, num_samples, bytes_used;

  int buffered = 0;

  ALint  processed;
  ALenum state;

  alGetSourcei(source, AL_SOURCE_STATE, &state);
  alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

  if (!is_playing && processed == 0) {
    has_buffered = 0;
    return -1;
  }

  int require_more = 0;

  if (processed > 0) {
    for (int i = 0; i < processed; ++i) {
      ALuint buffer_id;

      alSourceUnqueueBuffers(source, 1, &buffer_id);

      int packets_processed = 0;
      int pcm_index         = 0;
      int pcm_total_length  = 0;
      memset(pcm, 0, pcm_capacity);
      int frame_size;
      for (int i = 0; i < PACKETS_PER_BUFFER; ++i) {
      load_data:
        frame_size = (buffer->file_length - buffer->offset);
        frame_size =
            (frame_size > MAX_FRAME_SIZE) ? MAX_FRAME_SIZE : frame_size;
        // data limit is = length of buffer - offset
        bytes_used = stb_vorbis_decode_frame_pushdata(
            vorbis, buffer->data + buffer->offset, frame_size, &num_channels,
            &out, &num_samples);
        if (bytes_used == 0) {
          printf("Unable to load data from [%i] bytes after load.\n",
                 frame_size);
          is_playing = 0;
          return -1;
        }

        buffer->offset += bytes_used;

        // offset += bytes_used;
        if (num_samples > 0) {
          int shorts     = num_samples * num_channels;
          int pcm_length = sizeof(short) * shorts;
          pcm_total_length += pcm_length;
          if (pcm_length + pcm_index > pcm_capacity) {
            printf("Uhhh.\n");
            break;
          }
          for (int j = 0; j < num_samples; ++j) {
            for (int i = 0; i < num_channels; ++i) {
              pcm[pcm_index] = out[i][j] * 32767;
              ++pcm_index;
            }
          }
        }
      }

      printf("[%i] shorts used.\n", pcm_total_length);

      int format = (num_channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
      alBufferData(buffer_id, format, pcm, pcm_total_length,
                   vorbis->sample_rate);
      alSourceQueueBuffers(source, 1, &buffer_id);

      // printf("Buffered [%i] samples.\n", num_samples);

      ++buffered;
    }
  }

  if (buffered == NUM_BUFFERS) {
    printf("Restarting stream.\n");
    alSourcePlay(source);
  }

  return buffered;
}

int main(int argc, char** argv) {
  // initialize openal
  ALCdevice*  device  = alcOpenDevice(NULL);
  ALCcontext* context = alcCreateContext(device, NULL);

  if (!alcMakeContextCurrent(context)) {
    printf("Error creating OpenAL Context\n");
    return EXIT_FAILURE;
  }

  static int         q, error, used;
  static stb_vorbis* v;

  f_buffer buffer = get_buffer(test_file);

  printf("[%i] BUFFER_SIZE.\n", MIN_BUFFER_SIZE);

  if (buffer.file_length - buffer.offset < MIN_BUFFER_SIZE) {
    has_remaining = 0;
  }

  // data = get_data(&max_length);
  printf("[%i] bytes in file.\n", buffer.file_length);
  buffer.offset = 0;

  q = 1;
retry_init:
  v = stb_vorbis_open_pushdata(buffer.data, MAX_FRAME_SIZE, &used, &error,
                               NULL);
  if (v == NULL) {
    if (error == VORBIS_need_more_data) {
      q += 1;
      goto retry_init;
    }
    printf("Error: [%i]\n", error);
    return EXIT_FAILURE;
  }

  buffer.offset += used;
  // offset += used;

  pcm_capacity = sizeof(short) * MAX_FRAME_SIZE * PACKETS_PER_BUFFER;
  pcm          = (short*)malloc(pcm_capacity);

  printf("[%i] bytes used in header.\n", used);
  printf("[%i] sample rate.\n", v->sample_rate);
  printf("[%i] PCM capacity.\n", pcm_capacity);

  unsigned int source, buffers[NUM_BUFFERS];

  alGenSources(1, &source);
  alGenBuffers(NUM_BUFFERS, &buffers);

  alListenerf(AL_GAIN, 0.3f);
  alSourcef(source, AL_GAIN, 1.f);

  int compressed_packet = 0;

  int     bytes_used, num_channels, num_samples;
  int     data_length = MAX_FRAME_SIZE;
  float** out;
  for (int i = 0; i < NUM_BUFFERS; ++i) {
    unsigned int current_buffer = buffers[i];
  initialize_buffer:
    bytes_used = stb_vorbis_decode_frame_pushdata(
        v, buffer.data + buffer.offset, data_length, &num_channels, &out,
        &num_samples);

    if (bytes_used == 0) {
      printf("Unable to load samples from data with length of [%i]\n",
             data_length);
      return 0;
    }

    // printf("Decoded [%i] samples from [%i] bytes.\n", num_samples,
    // bytes_used); offset += bytes_used;

    buffer.offset += bytes_used;

    if (num_samples > 0) {
      memset(pcm, 0, pcm_capacity);
      int shorts     = num_samples * num_channels;
      int pcm_length = sizeof(short) * shorts;
      if (pcm_length > pcm_capacity) {
        printf("Uhhhh.\n");
      }

      printf("[%i] shorts used.\n", shorts);

      int pcm_index = 0;

      // convert float to interleaved short
      for (int j = 0; j < num_samples; ++j) {
        for (int i = 0; i < num_channels; ++i) {
          pcm[pcm_index] = out[i][j] * 32767;
          ++pcm_index;
        }
      }

      int format = (num_channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
      alBufferData(current_buffer, format, pcm, pcm_length, v->sample_rate);
      alSourceQueueBuffers(source, 1, &current_buffer);
    } else {
      goto initialize_buffer;
    }
  }

  alSourcePlay(source);
  is_playing   = 1;
  has_buffered = 1;
  while (is_playing && has_buffered) {
    int processed = push_data(v, source, buffers, &buffer);
  }

  alcCloseDevice(device);
  free(pcm);
  return EXIT_SUCCESS;
}
