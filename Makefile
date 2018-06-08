tproto:
	cc main.c tproto.c -lmhash -o tproto

.PHONY: clean

clean:
	rm tproto
	rm received/recvd.*
