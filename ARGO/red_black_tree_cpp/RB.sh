#!/bin/bash -l

#############################
# Job comment
#############################
#SBATCH -N 2
#SBATCH -J red_black_tree_cpp
#SBATCH -o log.out
#SBATCH -p cluster
#############################
# OpenMPI Infiniband flags
#############################
OMPIFLAGS="--map-by ppr:1:node "
OMPIFLAGS+="--mca mpi_leave_pinned 1 "
OMPIFLAGS+="--mca btl_openib_allow_ib 1 "
OMPIFLAGS+="--mca btl openib,self,vader "

mpirun $OMPIFLAGS taskset -c 0-15 ./rb_nvm
