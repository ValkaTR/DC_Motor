CC     = g++
CFLAGS = `pkg-config --cflags gtk+-2.0 gmodule-2.0`
LIBS   = `pkg-config --libs   gtk+-2.0 gmodule-2.0`
DEBUG  = -Wall -g

PKGS =
PKGS += gtk+-3.0

OBJECTS =
OBJECTS += DC_motor.o

all: DC_motor

%.o: %.cpp
	$(CC) -g -Wall  -o $@ -c $^ $(shell pkg-config --cflags $(PKGS))

dc_motor: $(OBJECTS)
	$(CC) -g -Wall  -o $@ $(OBJECTS) $(shell pkg-config --libs $(PKGS))

clean:
	rm -f *.o DC_motor
