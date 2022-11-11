#!/bin/bash -l

#############################
# Job comment
#############################
#SBATCH -N 4
#SBATCH -J PC_pr1
#SBATCH -o logSCN4p1_4TH_WEAK.out
#SBATCH -t 04:00:00
#############################
# OpenMPI Infiniband flags
#############################
OMPIFLAGS="--map-by ppr:1:node "
OMPIFLAGS+="--mca mpi_leave_pinned 1 "
OMPIFLAGS+="--mca btl_openib_allow_ib 1 "
OMPIFLAGS+="--mca btl openib,self,vader "

mpirun $OMPIFLAGS taskset -c 0-15 ./pc_nvm
