EXE=xshell
SRC=src
DIST=bin

CXX=g++
RM=rm -rf
MKDIR=mkdir -p

SRCS=$(SRC)/cpp/*.cpp $(SRC)/main.cpp
STD=--std=c++1y

$(DIST)/$(EXE):
	$(MKDIR) $(DIST)
	$(CXX) $(SRCS) -o $(DIST)/$(EXE) $(STD)

clean:
	$(RM) $(DIST)

run:
	./$(DIST)/$(EXE)
