CC := gcc
CFLAGS := 
LDFLAGS := 

SRC := packet_parser.c
TGT := $(SRC:.c=)

build: ${TGT}

parse%: ${TGT}
	./${TGT} pkt$*

clean:
	${RM} ${TGT}
