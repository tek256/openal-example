all : $(OBJS)
	cc src/main.c -lopenal -lm -std=c99 -O3 

