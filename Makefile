
maglev_hash: maglev_hash.c
	gcc -O0 -g  maglev_hash.c -o maglev_hash

clean:
	rm -f maglev_hash *.o



