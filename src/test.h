#ifndef TEST_H
#define TEST_H

typedef struct {
	const char* file_path;
	int file_offset, file_length;
	int data_offset, data_length, data_capacity;
	unsigned char* data;
	int has_remaining;
} f_buffer;

int init_buffer(f_buffer* buffer, int buffer_size);
unsigned char* pull_data(f_buffer* buffer, int request, int* pulled);
int load_more(f_buffer* buffer);

#endif
