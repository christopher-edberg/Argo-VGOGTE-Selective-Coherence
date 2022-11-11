#!/bin/bash -l

#############################
# Job comment
#############################
#SBATCH -N 2
#SBATCH -J TP_pr1
#SBATCH -o logSCN1p1.out
#SBATCH -t 04:00:00
#############################
# OpenMPI Infiniband flags
#############################
OMPIFLAGS="--map-by ppr:1:node "
OMPIFLAGS+="--mca mpi_leave_pinned 1 "
OMPIFLAGS+="--mca btl_openib_allow_ib 1 "
OMPIFLAGS+="--mca btl openib,self,vader "

mpirun $OMPIFLAGS taskset -c 0-15 ./tpcc_nvm
