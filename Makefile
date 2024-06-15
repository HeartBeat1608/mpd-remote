OUTFILE=main
INFILE=mpd-remote.c

run: build
	./main

build:
	gcc -lmpdclient -o $(OUTFILE) $(INFILE)
