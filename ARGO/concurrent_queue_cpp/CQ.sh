#!/bin/bash -l

#############################
# Job comment
#############################
#SBATCH -N 4
#SBATCH -J CQ_pr1
#SBATCH -o logNCN4p1_16TH_WEAK.out
#SBATCH -t 05:00:00
#############################
# OpenMPI Infiniband flags
#############################
OMPIFLAGS="--map-by ppr:1:node "
OMPIFLAGS+="--mca mpi_leave_pinned 1 "
OMPIFLAGS+="--mca btl_openib_allow_ib 1 "
OMPIFLAGS+="--mca btl openib,self,vader "

mpirun $OMPIFLAGS taskset -c 0-15 ./cq_nvm
