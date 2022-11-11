#!/bin/bash -l

#############################
# Job comment
#############################
#SBATCH -N 4
#SBATCH -J CQSW
#SBATCH -o logNCN4p2_WEAK.out
#SBATCH -t 05:00:00
#############################
# OpenMPI Infiniband flags
#############################
OMPIFLAGS="--map-by ppr:2:node "
OMPIFLAGS+="--mca mpi_leave_pinned 1 "
OMPIFLAGS+="--mca btl_openib_allow_ib 1 "
OMPIFLAGS+="--mca btl openib,self,vader "

mpirun $OMPIFLAGS --bind-to socket ./cq_nvm
