#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#include "test.h"

#define PACKET_SIZE 256

int init_buffer(f_buffer* buffer, int buffer_size){
	if(!buffer->file_path){
		printf("No file path provided.\n");
		return -1;
	}

	buffer->data = (unsigned char*)malloc(sizeof(unsigned char) * buffer_size);
	if(!buffer->data){
		printf("Unable to allocate space for the buffer with a size of [%i].\n", buffer_size);
		return -1;
	}

	buffer->data_capacity = sizeof(unsigned char) * buffer_size;

	FILE* f = fopen(buffer->file_path, "r+");
	if(!f){
		free(buffer->data);
		printf("Unable to open file: %s\n", buffer->file_path);
	}

	fseek(f, 0, SEEK_END);
	buffer->file_length = ftell(f);
	rewind(f);

	printf("File length [%i].\n", buffer->file_length);

	int data_read = fread(buffer->data, sizeof(unsigned char), buffer_size, f);
	buffer->data_length = data_read;
	
	if(data_read == 0){
		free(buffer->data);
		fclose(f);
		printf("Unable to read data from file: %s\n", buffer->file_path);
		return -1;
	}

	buffer->has_remaining = 1;

	fclose(f);
	return 1;
}

unsigned char* pull_data(f_buffer* buffer, int request, int* pulled){
	if(buffer->file_length == 0){
		printf("Unable to pull data from initialized buffer.\n");
		return 0;
	}	

	int remaining_in_file = buffer->file_length - buffer->file_offset;

	if(remaining_in_file < (buffer->data_capacity - buffer->data_offset)){
		request = remaining_in_file;
		buffer->file_offset = buffer->file_length;	
		buffer->has_remaining = 0;	
	}

	FILE* f = fopen(buffer->file_path, "r+");
	fseek(f, buffer->file_offset, SEEK_SET);

	unsigned char* data = (unsigned char*)malloc(sizeof(unsigned char) * (request + 1));
	int data_read = fread(data, sizeof(unsigned char), request, f);

	data[data_read] = 0;

	if(pulled){
		*pulled = data_read;
	}

	fclose(f);

	return data;
}

int load_more(f_buffer* buffer){
	if(!buffer->has_remaining){
		printf("No data left remaining to call for buffer: load_more()\n");
		return -1;
	}

	if(buffer->data_length - buffer->data_offset >= PACKET_SIZE){
		//printf("No need to buffer more data.\n");
		//++rollover;
		//return 0;
	}

	/*if(rollover > 0){
		printf("Rollover: [%i]\n", rollover);
		rollover = 0;
	}*/

	buffer->file_offset += buffer->data_offset;

	int remaining_data = buffer->data_length - buffer->data_offset;
	//printf("Data remaining in buffer [%i].\n", remaining_data);

	if(remaining_data > 0){
		//printf("Pushing remaining data to front.\n");
		memcpy(buffer->data, buffer->data + buffer->data_offset, remaining_data);	
	}else if(remaining_data < 1){
		//printf("No remaining data.\n");
		remaining_data = 0;
	}

	int data_to_request = buffer->data_capacity - remaining_data;
	printf("Requesting [%i] bytes.\n", data_to_request);

	//printf("Requesting new data from file [%i]\n", data_to_request);
	int pulled;	
	unsigned char* data = pull_data(buffer, data_to_request, &pulled);
	//printf("Pushing new data into buffer [%i]\n", pulled);
	printf("[%i] bytes pulled.\n", pulled);

	int to_buffer = (pulled > data_to_request) ? data_to_request : pulled;
	memcpy(buffer->data + remaining_data, data, to_buffer); 
	if(!to_buffer){
		printf("No data to buffer: [%i].\n", to_buffer);
	}

	buffer->data_offset = 0;
	buffer->data_length = pulled + remaining_data;
	//memset(buffer->data + buffer->data_length, 0, buffer->data_capacity - buffer->data_length);

	free(data);
	//printf("Loaded more data with size of [%i].\n", pulled);
	return 1;
}

/*
int main(int argc, char** argv){
	f_buffer buffer = (f_buffer){0};
	buffer.file_path = "res/test.ogg";
	if(!init_buffer(&buffer)){
		return EXIT_FAILURE;
	}

	srand(time(NULL));

	buffer.data_offset += 256 + (rand() % 16);	
	while(buffer.has_remaining){
		buffer.data_offset += 256 + (rand() % 16);
		if(load_more(&buffer)){
			printf("File offset [%i]\n", buffer.file_offset);
		}
	}

	free(buffer.data);
}*/
