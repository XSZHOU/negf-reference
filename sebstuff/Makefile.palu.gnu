# MAKEFILE PROJECT NEGF
# CSCS-GNU VERSION

# ============================================================
# external include directories and libraries
# ============================================================

BASE_PATH    = /users/steiger/src/release/gnu
LIB_PATH     = $(BASE_PATH)/lib

DEF_LIB      = -lc -lrt
GCC_LIB      = -L$(LIB_PATH)/gcc -lgfortran -lstdc++ -lm 

BOOST_INC    = -I/users/steiger/src/master/boost_1_35_0

CBLAS_INC    = -I$(BASE_PATH)/cblas/src
CBLAS_LIB    = -L$(LIB_PATH) -lcblas

FLENS_INC    = -I$(BASE_PATH)/FLENS-lite_0608
FLENS_LIB    = -L$(LIB_PATH) -lflens 

LINALG_INC   = -I/opt/acml/4.0.1a/gfortran64/include
LINALG_LIB   = -L/opt/acml/4.0.1a/gfortran64/lib/ -lacml_mv 

TDKP_INC     = -I$(BASE_PATH)/tdkp
TDKP_LIB     = -L$(LIB_PATH) -ltdkp -ljdqz

ZLIB_INC     = -I$(BASE_PATH)/zlib-1.2.3
ZLIB         = -L$(LIB_PATH) -lz

UMFPACK_INC  = -I$(BASE_PATH)/umfpack/UMFPACK -I$(BASE_PATH)/umfpack/AMD/Include -I$(BASE_PATH)/umfpack/UFconfig
UMFPACK_LIB  = -L$(LIB_PATH) -lumfpack -lamd

DFISE_INC    = -I$(BASE_PATH)/sebise/sebise
DFISE_LIB    = -L$(LIB_PATH) -lsebise

#MPI_INC      = -I$(BASE_PATH)/mpich2/include
MPI_LIB      = -lmpich -lmpichcxx

OMPDUMMY      = bin/ompdummy.o

# ============================================================
# compiler settings
# ============================================================

# defines
DEFINES      = -DDEBUG -DAMD -DMPICH_IGNORE_CXX_SEEK -DCSCS

CPP			 = CC

CPP_FLAGS    = -Wall -Wno-deprecated -g -O1 $(DEFINES) -m64
# -Wshadow

CPP_INC      = -Isrc/common/ -Isrc/geometry/ -Isrc/io/ -Isrc/newton/ -Isrc/newton/solvers/ -Isrc/negf/ \
			   -Isrc/negf/selfenergies/ -Isrc/negf/hamiltonian/ -Isrc/kspace_Espace/ \
			   $(TDKP_INC) $(BOOST_INC) $(FLENS_INC) $(CBLAS_INC) $(MPI_INC) $(ZLIB_INC)

CPP_ALL_INC  = $(CPP_INC) $(DFISE_INC) $(UMFPACK_INC) $(LINALG_INC) 

CPP_LIBS     = -nodefaultlibs -Bstatic  $(TDKP_LIB) $(DFISE_LIB) $(UMFPACK_LIB) $(ZLIB) $(FLENS_LIB) $(CBLAS_LIB) $(LINALG_LIB) $(MPI_LIB) $(GCC_LIB) $(DEF_LIB) 

# ===============================================================
# from here on the makefile is platform- and compiler-independent
# ===============================================================

include definitions.negf
