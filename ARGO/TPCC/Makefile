# Set your ArgoDSM installation directory
export ARGO_INSTALL_DIRECTORY=${HOME}/Argo-nosync_mod/argodsm/install

CXX      =mpic++
CXXFLAGS =-O3 -lpthread -std=c++11

IDFLAGS +=-I${ARGO_INSTALL_DIRECTORY}/include/argo
IDFLAGS +=-I${ARGO_INSTALL_DIRECTORY}/include/argo/synchronization
LDFLAGS +=-L${ARGO_INSTALL_DIRECTORY}/lib
LDFLAGS +=-Wl,-rpath=${ARGO_INSTALL_DIRECTORY}/lib
LDLIBS  +=-largo -largobackend-mpi -lnuma -lrt

SOURCES=tpcc_db.cc tpcc_nvm.cc
OBJECTS=$(SOURCES:.cpp=.o)
	EXECUTABLE=tpcc_nvm

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $(IDFLAGS) $(LDFLAGS) $(SOURCES) $(LDLIBS)

clean:
	rm -rf $(EXECUTABLE)
