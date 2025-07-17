# Makefile for CargoForge-C

CC = gcc
CFLAGS = -O3 -Wall -std=c99

all: cargoforge

cargoforge: main.c
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f cargoforge