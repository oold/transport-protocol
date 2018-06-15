tproto: main.c tproto.c options.h defines.h api.h
	cc main.c tproto.c -lmhash -o tproto

.PHONY: clean

clean:
	rm tproto
	rm received/recvd.*
