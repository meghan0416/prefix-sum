# Makefile for Prefix Sum assignment
# Authors: Meghan Grayson and Vaishnavi Karaguppi
# Date: February 11, 2024
# **********************************
# Variables
CC = g++
CFLAGS = -g -Wall
EXECUTABLES = my-count


# All files to be generated
all: $(EXECUTABLES) 
	$(CC) $(CFLAGS) -o $(EXECUTABLES) $(EXECUTABLES).cpp

# Clean the directory
clean: 
	rm -rf $(EXECUTABLES) *.o *.dSYM

